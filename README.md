# ESP32_IR_RF_SwitchBot_Hub

ESP32をベースに、赤外線 (IR)、無線 (RF/433MHz)、および SwitchBot API (Wi-Fi) を統合制御するためのIoTハブデバイスです。
異なる通信規格を持つスマートホーム機器やレガシー家電を、単一のESP32デバイスから制御することを目的としています。

## 特徴

* **SwitchBot API v1.1 対応**: HMAC-SHA256署名生成を行い、HTTPS経由でSwitchBotデバイス（プラグ等）を直接制御可能。
* **赤外線 (IR) 送信**: `IRremoteESP8266` ライブラリを使用し、家電（エアコン、照明など）を制御。
* **RF (433MHz) 送信**: `rc-switch` ライブラリを使用し、リモコンコンセントなどを制御。
* **ネットワーク耐性**: Wi-Fi接続失敗時に代替手段（IR送信など）を実行するフェイルオーバーロジックの実装例を含みます。

## 動作要件

### ハードウェア
* ESP32 Development Board
* 赤外線LED (IR Transmitter)
* 433MHz RF Transmitter
* (Optional) ステータス確認用LED

### 接続ピン (デフォルト設定)
コード内の定義に基づきます。

| コンポーネント | GPIOピン | 定数名 in Code |
| :--- | :--- | :--- |
| **IR Transmitter** | GPIO 13 | `kIrSendPin` |
| **RF Transmitter** | GPIO 12 | `kRfSendPin` |
| **Status/Debug Pin** | GPIO 14 | - |

## ライブラリの依存関係
Arduino IDEのライブラリマネージャから以下をインストールしてください。

1.  **ArduinoJson** (by Benoit Blanchon)
2.  **IRremoteESP8266** (by David Conran, etc.)
3.  **rc-switch** (by sui77)
4.  **WiFiClientSecure** (ESP32標準)
5.  **mbedtls** (ESP32標準)

## 設定 (Configuration)

`server_startup_v1.ino` 内の以下の定数を、自身の環境に合わせて書き換えてください。
**※セキュリティ警告: トークンやパスワードを含むコードを公開リポジトリにアップロードしないよう注意してください。**

```cpp
// Wi-Fi設定
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// SwitchBot API設定 (アプリの開発者モードから取得)
const char* switchbot_token = "YOUR_TOKEN";
const char* switchbot_secret = "YOUR_SECRET"; 

// 制御対象のデバイスID
const char* device_id_1 = "DEVICE_ID_1";
// ...
````

## サンプルコードの動作解説 (Example Logic)

本リポジトリに含まれるコードは、サーバー機器の自動復旧を想定した**動作サンプル**です。
起動時に以下のロジックで動作します。

1.  **Wi-Fi接続試行**: 固定IPでの接続を試みる。
2.  **成功時 (Normal Mode)**:
      * NTPサーバーから時刻を取得。
      * SwitchBotプラグをすべて **OFF** にする（再起動シーケンス等のため）。
3.  **失敗時 (Recovery Mode)**:
      * 3分待機後、**赤外線(IR)信号** を送信（物理的な電源操作などを想定）。
      * さらに3分待機後、Wi-Fi再接続を試みる。
      * 接続できたら、特定のSwitchBotプラグを **ON** にする。

## 関数リファレンス

  * `controlSwitchBotPlug(String command, const char* target_device_id)`
      * SwitchBot APIを叩いてプラグの ON/OFF を行います。
  * `sendIrSignal()`
      * 配列 `rawSignalToSend` に定義されたIR信号を送信します。
  * `sendRfSignal()`
      * RF信号 (24bit) を送信します。

## License

MIT License