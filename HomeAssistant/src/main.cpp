#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <Preferences.h>

#include "config.h"
#include "wifi_manager.h"
#include "led_controller.h"
#include "sensor_manager.h"
#include "web_server.h"
#include "storage.h"
#include "home_assistant.h"

// Task handles
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t serverTaskHandle = NULL;
TaskHandle_t debugTaskHandle = NULL;

// Debug Task
void debugTask(void * parameter) {
  for (;;) {
    Serial.print("Sensor distance: ");
    Serial.println(getSensorDistance());
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Time update task
void updateTime() {
  static unsigned long lastTimeCheck = 0;
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastTimeCheck >= 1000) {
    lastTimeCheck = currentMillis;
    time_t nowSec = time(nullptr);
    
    if (nowSec < 1000000000UL) {
      setLightOn(true);
    } else {
      if (!isSmartHomeOverride()) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          int currentTotal = timeinfo.tm_hour * 60 + timeinfo.tm_min;
          int startTotal = getStartHour() * 60 + getStartMinute();
          int endTotal = getEndHour() * 60 + getEndMinute();
          
          if (startTotal <= endTotal)
            setLightOn(currentTotal >= startTotal && currentTotal < endTotal);
          else
            setLightOn(currentTotal >= startTotal || currentTotal < endTotal);
        }
      }
    }
  }
}

// Setup OTA
void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  
  ArduinoOTA.begin();
}

void setup() {
  Serial.begin(115200);
  Serial.println("LightTrack starting...");
  
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    return;
  }
  
  // Initialize storage
  initStorage();
  
  // Initialize LED controller
  initLEDController();
  
  // Initialize sensor interface
  initSensor();
  
  // Initialize time
  configTzTime("UTC0", "");
  
  // Setup WiFi
  setupWiFi();
  
  // Initialize web server
  initWebServer();
  
  // Initialize HomeAssistant integration
  initHomeAssistant();
  
  // Setup OTA
  setupOTA();
  
  // Create tasks
  xTaskCreatePinnedToCore(sensorTask, "Sensor Task", 2048, NULL, 2, &sensorTaskHandle, 1);
  xTaskCreatePinnedToCore(ledTask, "LED Task", 4096, NULL, 1, &ledTaskHandle, 0);
  xTaskCreatePinnedToCore(webServerTask, "WebServer Task", 4096, NULL, 1, &serverTaskHandle, 1);
  xTaskCreatePinnedToCore(debugTask, "Debug Task", 2048, NULL, 1, &debugTaskHandle, 1);
  
  Serial.println("LightTrack started!");
}

void loop() {
  ArduinoOTA.handle();
  handleHomeAssistant();
  updateTime();
  vTaskDelay(pdMS_TO_TICKS(1000));
}