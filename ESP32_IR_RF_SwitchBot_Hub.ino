#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <RCSwitch.h>
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>
#include <string.h> 
#include <time.h>
#include <sys/time.h>

const char* ssid = "";
const char* password = "";
const char* switchbot_token = "";
const char* switchbot_secret = ""; 
const char* device_id_1 = "";
const char* device_id_2 = "";
const char* device_id_3 = "";

#define WIFI_TIMEOUT_MS 30000

const uint16_t kIrSendPin = 13;
const uint16_t kRfSendPin = 12;
uint16_t rawSignalToSend[] = {
    2550, 2700, 850, 800, 900, 800, 850, 1900, 850, 1900, 
    850, 1900, 800, 1900, 850, 850, 850, 850, 850
};
int rawSignalLen = 19;

IRsend irsend(kIrSendPin);
RCSwitch mySwitch = RCSwitch();
void hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t *output) {
    mbedtls_sha256_context ctx;
    uint8_t k_ipad[64]; 
    uint8_t k_opad[64]; 
    uint8_t inner_hash[32]; 

    if (key_len > 64) {
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts(&ctx, 0); 
        mbedtls_sha256_update(&ctx, key, key_len);
        mbedtls_sha256_finish(&ctx, (unsigned char *)k_ipad);
        key = k_ipad;
        key_len = 32;
    }

    memset(k_ipad, 0x36, 64);
    memset(k_opad, 0x5c, 64);
    for (size_t i = 0; i < key_len; i++) {
        k_ipad[i] ^= key[i];
        k_opad[i] ^= key[i];
    }

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, k_ipad, 64);
    mbedtls_sha256_update(&ctx, data, data_len);
    mbedtls_sha256_finish(&ctx, inner_hash);

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, k_opad, 64);
    mbedtls_sha256_update(&ctx, inner_hash, 32);
    mbedtls_sha256_finish(&ctx, output);
}


void setup() {
  Serial.begin(115200);
  irsend.begin();
  mySwitch.enableTransmit(kRfSendPin);
  pinMode(14, OUTPUT);
  digitalWrite(14, LOW);
  
  Serial.print("Connecting to ");
  Serial.println(ssid);

  IPAddress dns1(192, 168, 200, 2);
  IPAddress dns2(8, 8, 8, 8); 

  // IP, Gateway, Subnetに 0.0.0.0 を指定すると、DHCPが有効になり、DNSだけが指定されます。
  if (!WiFi.config(IPAddress(192, 168, 200, 116), IPAddress(192, 168, 200, 1), IPAddress(255, 255, 255, 0), dns1, dns2)) {
      Serial.println("WiFi.config (DNS only) failed.");
  }

  WiFi.disconnect(true);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected (Attempt 1)");
    Serial.println("IP address: " + WiFi.localIP().toString());
    configTime(3600 * 9, 0, "210.173.160.27", "216.239. 35.0");
    Serial.println("Waiting for NTP time...");
    time_t now = time(nullptr);
    while (now < 100000) { 
        delay(500);
        Serial.print("*");
        now = time(nullptr);
    }
    Serial.println("\nTime synchronized.");
    
    Serial.println("3台のSwitchBotプラグをOFFにします。");
    controlSwitchBotPlug("turnOff", device_id_1);
    controlSwitchBotPlug("turnOff", device_id_2);
    controlSwitchBotPlug("turnOff", device_id_3);
  } else {
    Serial.println("\nWiFi connection failed.");
    Serial.println("3分後に赤外線信号を送信します。");
    
    delay(180000);
    
    Serial.println("赤外線信号を送信します...");
    sendIrSignal();
    Serial.println("さらに3分後に 'turnOn' を試みます。");
    
    delay(180000); 
    
    Serial.println("Re-attempting WiFi connection...");
    WiFi.disconnect(true);
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    
    Serial.println("\nWiFi connected (Attempt 2)");
    Serial.println("IP address: " + WiFi.localIP().toString());
    configTime(3600 * 9, 0, "ntp.nict.jp", "time.google.com"); 
    time_t now = time(nullptr);
    while (now < 100000) { 
        delay(500);
        now = time(nullptr);
    }

    Serial.println("SwitchBotプラグ (1台目) をONにします。");
    controlSwitchBotPlug("turnOn", device_id_1);
  }
}

void loop() {
}

void sendIrSignal() {
  irsend.sendRaw(rawSignalToSend, rawSignalLen, 38);
}

void sendRfSignal() {
  digitalWrite(14, HIGH);
  delay(1);              
  mySwitch.send(5324, 24);  
  delay(1);              
  digitalWrite(14, LOW);
}


void controlSwitchBotPlug(String command, const char* target_device_id) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }
  
  WiFiClientSecure client;
  client.setInsecure();

  const char* api_host = "api.switch-bot.com";
  const char* api_path_prefix = "/v1.1/devices/";
  const char* api_path_suffix = "/commands";
  const int api_port = 443; 

  String api_path = String(api_path_prefix) + String(target_device_id) + String(api_path_suffix);
  struct timeval tv;
  gettimeofday(&tv, NULL);
  unsigned long long now_ms = (unsigned long long)tv.tv_sec * 1000ULL + (unsigned long long)tv.tv_usec / 1000ULL;
  String t = String(now_ms); 
  
  String nonce = "RequestID-" + t;
  String stringToSign = String(switchbot_token) + t + nonce;
  unsigned char hmacResult[32]; 
  hmac_sha256(
      (const uint8_t*)switchbot_secret, strlen(switchbot_secret), 
      (const uint8_t*)stringToSign.c_str(), stringToSign.length(), hmacResult
  );
  char base64EncodedSign[45]; 
  size_t encodedLen = 0; 
  mbedtls_base64_encode((unsigned char*)base64EncodedSign, sizeof(base64EncodedSign), &encodedLen, hmacResult, 32);
  base64EncodedSign[encodedLen] = '\0'; 

  JsonDocument doc;
  doc["command"] = command;
  doc["parameter"] = "default";
  doc["commandType"] = "command";
  String json_payload;
  serializeJson(doc, json_payload);
  if (!client.connect(api_host, api_port)) {
    Serial.print("Error on sending POST: -1 (HTTPS connection failed to ");
    Serial.print(api_host); Serial.println(")");
    return;
  }

  String request = "POST " + api_path + " HTTP/1.1\r\n" +
                   "Host: " + String(api_host) + "\r\n" +
                   "Authorization: " + String(switchbot_token) + "\r\n" +
                   "sign: " + String(base64EncodedSign) + "\r\n" +
                   "nonce: " + String(nonce) + "\r\n" +
                   "t: " + t + "\r\n" +
                   "Content-Type: application/json; charset=utf8\r\n" +
                   "Content-Length: " + String(json_payload.length()) + "\r\n" +
                   "Connection: close\r\n\r\n" +
                   json_payload;
  client.print(request);
  
  unsigned long timeout = millis();
  while (client.connected() && !client.available()) {
    if (millis() - timeout > 10000) { 
      Serial.println("Error on sending POST: -1 (Client Timeout)");
      client.stop();
      return;
    }
  }

  String response = "";
  int httpResponseCode = 0;
  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      if (line.startsWith("HTTP/1.1")) {
        httpResponseCode = line.substring(9, 12).toInt();
      } else if (line.length() == 0) {
        break;
      }
    }
  }
  
  while (client.available()) {
    response += client.readStringUntil('\n');
  }
  client.stop();

  if (httpResponseCode > 0) {
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    Serial.println("Response: " + response);

    JsonDocument responseDoc;
    if (deserializeJson(responseDoc, response) == DeserializationError::Ok) {
      if (responseDoc["statusCode"].as<int>() == 100) { 
        Serial.println("プラグの操作に成功しました！");
      } else {
        Serial.print("プラグの操作に失敗しました... Message: ");
        Serial.println(responseDoc["message"].as<const char*>());
      }
    } else {
      Serial.println("プラグの操作に失敗しました... (JSON解析エラー)");
    }
  } else {
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
  }
}