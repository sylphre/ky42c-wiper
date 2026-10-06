# KY42C Wiper

KY-42C (MT6761) の Preloader の穴 (`CMD_SEND_IMAGE` / `CMD_BOOT_IMAGE`) を使って、Bootloaderはそのままに中身だけ工場出荷状態に戻すやつです。

元ネタは [shomykohai/kyocera-ky-42c-unlock](https://github.com/shomykohai/kyocera-ky-42c-unlock) 。あちらは Bootloader Unlock が目的ですが、こっちは `seccfg` に触らず `userdata` 周りだけを消す派生です。

> [!WARNING]
> 壊れても自己責任で。`userdata` は完全に消えます。元に戻せません。

確認できたのは 112.0.0153 あたり。新しめのファームでも Preloader が `0E8D:2000` で出てくればだいたい行けます。

## unlock と何が違うの

- `seccfg` / `chkcode` を書き換えない → Bootloader は Locked のまま。Orange State も出ない
- `userdata` / `cache` / `metadata` / `frp` をゼロ埋め → 再起動後に Android 側が「壊れてる」と判断して勝手にフォーマットして初期設定に戻る

やってることは unlock と同じで、Preloader に `lk` という名前で payload を送り込んで EL3 で動かして eMMC を直接叩いてます。

## 使い方

Python 3 があればOK。venv推奨。

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

権限エラーが出るなら一度だけ:

```bash
sudo usermod -aG dialout $USER
# 一回ログアウト/ログインし直す
```

あとは:

```bash
# 1. KY-42C の電源を長押しで完全に切る (10秒くらい)
# 2. 下のコマンドを打って Waiting にしておいてから USB を挿す
#    1〜2秒だけ 0E8D:2000 として認識される

sudo ./venv/bin/python main.py wipe
```

こんな感じになれば成功:

```
INFO: Using payload from bin/wipe.bin
INFO: Waiting for MediaTek Preloader device (0E8D:2000)...
INFO: Found device 0E8D:2000 at /dev/ttyACM0
INFO: MediaTek Preloader handshake completed!
INFO: Jumped to lk.
```

30〜60秒くらいで自動リセットがかかって再起動、初期設定画面が出れば終わり。

- `/dev/ttyACM0: Permission denied` → `sudo` で実行するか `dialout` に入り直す
- 起動ループ / 暗号化エラーになる → fast wipe だと足りてないので `payload/src/wipe.c` でフルワイプに切り替えてビルドし直す (時間かかるので普段は fast で十分)

ついでに本家のコマンドもそのまま残してます:

```bash
python main.py unlock  # Bootloader unlock (seccfg書き換え)
python main.py patch   # Preloaderパッチ (未署名DA用)
```

## 中身

昔の Preloader は `CFG_PRELOADER_AS_DA` が有効で `CMD_SEND_IMAGE (0x70)` と `CMD_BOOT_IMAGE (0x71)` が検証なしで叩けます。KY-42C はそれが残ってたので、512B のヘッダを付けた payload を `lk` として送るとそのまま実行できる。Wiper はそこで GPT を読んで該当パーティションだけ `mmc_write_blocks` で 0 埋めして WDT リセットで再起動、というだけです。

詳しい話は本家と [shomyさんのブログ](https://itssho.my/blog/article/unlocking-kyocera-ky42c) が詳しいです。

## ビルド

```bash
sudo apt install gcc-arm-none-eabi
./build.sh
# bin/wipe.bin / bin/unlock.bin / bin/patch.bin ができる
```

## 謝辞・ライセンス

- ベース: [shomykohai/kyocera-ky-42c-unlock](https://github.com/shomykohai/kyocera-ky-42c-unlock) (Shomy, AGPL-3.0-or-later)
- `payload/src/gpt.c` は [@R0rt1z2](https://github.com/R0rt1z2) (GPL-3.0-or-later)
- `mtk-payloads` は [@shomykohai](https://github.com/shomykohai/mtk-payloads) (MIT)
- `moto-experiments` は [@R0rt1z2](https://github.com/R0rt1z2/moto-experiments) (MIT)

このリポジトリ全体は `AGPL-3.0-or-later` です ([LICENSE](LICENSE))。

追加した部分 (`payload/src/wipe.c` と `main.py` の wipe 周り) は

```
SPDX-License-Identifier: AGPL-3.0-or-later
SPDX-FileCopyrightText: Copyright © 2014–2026 Laurier Sylph.
```

で同じライセンスで配布してます。
