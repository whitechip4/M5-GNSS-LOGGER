# サードパーティライセンス

このプロジェクトでは以下のサードパーティライブラリを使用しています。

## M5Stackファームウェア (m5GnssLogger/)

### ライブラリ一覧

| ライブラリ | バージョン | ライセンス | URL |
|-----------|----------|-----------|-----|
| M5Core2 | ^0.1.9 | MIT | https://github.com/m5stack/M5Core2 |
| SparkFun u-blox GNSS Arduino Library | ^2.2.27 | MIT | https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library |
| ArduinoJson | ^6.21.0 | MIT | https://github.com/bblanchon/ArduinoJson |
| LovyanGFX | ^1.1.8 | MIT OR BSD-2-Clause | https://github.com/lovyan03/LovyanGFX |
| espressif32 | 6.1.0 | Apache-2.0 | https://github.com/platformio/platform-espressif32 |
| framework-arduinoespressif32 | - | LGPL-2.1-or-later | https://github.com/espressif/arduino-esp32 |

### 各ライブラリのコピーライト表記

#### M5Core2
```
Copyright (c) M5Stack
```

#### SparkFun u-blox GNSS Arduino Library
```
Copyright (c) 2018 SparkFun Electronics
```

#### ArduinoJson
```
Copyright Benoit Blanchon 2014-2024
```

#### LovyanGFX
```
lovyan03, tobozo
```

#### espressif32 platform
```
Copyright (c) 2014-2015 Arduino LLC
```

#### framework-arduinoespressif32
```
Copyright (c) 2011-2021 Espressif Systems (Shanghai) CO LTD
```

## Cloudflare Workers/Pages Functions

### 実行時依存パッケージ
なし（全てのパッケージは開発時のみ使用されるdevDependenciesのみです）

### 開発時パッケージ（devDependencies）

| パッケージ | バージョン | ライセンス | URL |
|----------|----------|-----------|-----|
| @cloudflare/workers-types | ^4.0.0 | MIT OR Apache-2.0 | https://github.com/cloudflare/workers-types |
| typescript | ^5.0.0 | Apache-2.0 | https://github.com/microsoft/TypeScript |
| wrangler | ^3.0.0 | MIT OR Apache-2.0 | https://github.com/cloudflare/workers-sdk |

## プラットフォーム詳細

### espressif32 platform 6.1.0
- **ライセンス**: Apache-2.0
- **URL**: https://github.com/platformio/platform-espressif32
- **説明**: ESP32系列マイコン用のPlatformIO開発プラットフォーム

### framework-arduinoespressif32 (espressif32のコンポーネント)
- **ライセンス**: LGPL-2.1-or-later
- **URL**: https://github.com/espressif/arduino-esp32
- **説明**: ESP32用Arduinoフレームワーク（本プロジェクトで採用されているライセンスの理由）

## ライセンス全文

ライセンスの全文は各ライブラリのリポジトリまたは以下のURLで確認できます：

- **MIT License**: https://opensource.org/licenses/MIT
- **Apache-2.0**: https://www.apache.org/licenses/LICENSE-2.0
- **BSD-2-Clause**: https://opensource.org/licenses/BSD-2-Clause
- **LGPL-2.1**: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
- **LGPL-2.1-or-later**: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html

## プロジェクト全体のライセンス

本プロジェクトは **LGPL-2.1-or-later** としてライセンスされています。

これは、espressif32 platformに含まれるframework-arduinoespressif32コンポーネントのライセンスに準拠するためです。詳細はプロジェクトルートの [LICENSE](./LICENSE) ファイルを参照してください。
