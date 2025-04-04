#include "wifi_manager.h"
#include "config.h"
#include "storage.h"
#include "web_server.h"
#include <WiFi.h>
#include "esp_wifi.h"

static String deviceName;

String getDeviceName() {
  return deviceName;
}

void setupWiFi() {
  // Только режим точки доступа (AP)
  WiFi.mode(WIFI_AP);  
  
  // Настройка точки доступа
  IPAddress local_IP(192, 168, 4, 22);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  
  // Генерация имени точки доступа
  randomSeed(ESP.getEfuseMac());
  char suffix[4];
  for (int i = 0; i < 3; i++) {
    int r = random(0, 36);
    if (r < 10)
      suffix[i] = '0' + r;
    else
      suffix[i] = 'A' + (r - 10);
  }
  suffix[3] = '\0';
  
  deviceName = "LightTrack " + String(suffix);
  WiFi.softAP(deviceName.c_str(), AP_PASSWORD);
  
  // Снижаем мощность WiFi для экономии энергии
  esp_wifi_set_max_tx_power(21);
  
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Хардкодим MQTT настройки (опционально)
  // Можно удалить или закомментировать этот блок если не нужен MQTT
  // saveMqttSettings("192.168.1.100", 1883, "", "");
}

// Обработчик страницы настроек WiFi - оставляем для совместимости с web_server.cpp
void handleWiFiSettings() {
  String html = "<html><head><title>WiFi Settings</title>"
    "<meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>"
    "<style>"
      "body { margin: 0; padding: 0; background-color: #333; color: white; font-family: Arial, sans-serif; }"
      ".container { text-align: center; width: 90%; max-width: 800px; margin: auto; padding-top: 20px; }"
      "input[type=text], input[type=password] { width: 80%; padding: 8px; margin: 5px; }"
      "input[type=submit] { padding: 10px 20px; margin: 15px; background-color: #4CAF50; color: white; border: none; cursor: pointer; }"
      "a { color: #aaaaff; text-decoration: none; }"
    "</style>"
    "</head><body>"
    "<div class='container'>"
      "<h1>WiFi Settings</h1>"
      "<p>В текущей версии прошивки WiFi клиент отключен.</p>"
      "<p>Устройство работает только в режиме точки доступа.</p>"
      "<p>Для настройки MQTT, перейдите в раздел MQTT Settings.</p>"
      "<p><a href='/'>Вернуться на главную</a></p>"
    "</div></body></html>";
  
  server.send(200, "text/html", html);
}

// Обработчик сохранения настроек WiFi - оставляем пустым для совместимости
void handleWiFiSave() {
  server.sendHeader("Location", "/wifi");
  server.send(303);
}