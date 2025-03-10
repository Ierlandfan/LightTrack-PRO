#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>
#include <EEPROM.h>
#include <SPIFFS.h>
#include <math.h>
#include "esp_wifi.h"
#include <time.h>
#include <ArduinoOTA.h>  // Added for OTA updates

// ------------------------- LED Configuration -------------------------
#define LED_PIN             2
#define NUM_LEDS            300
#define CHIPSET             WS2812B
#define COLOR_ORDER         GRB
CRGB leds[NUM_LEDS];

// ------------------------- Sensor Parameters -------------------------
#define SENSOR_HEADER       0xAA
#define MIN_DISTANCE        20
#define MAX_DISTANCE        1000
#define DEFAULT_DISTANCE    1000
#define NOISE_THRESHOLD     5

// ------------------------- Display Parameters -------------------------
int updateInterval = 20;
float movingIntensity = 0.3;
float stationaryIntensity = 0.0;  // This controls the background (LED) light intensity
int movingLength = 33;
int centerShift = 0;
int additionalLEDs = 0;
CRGB baseColor = CRGB(255, 200, 50);
float speedMultiplier = 2.0;

// New parameter: LED off delay (in seconds)
// Default value is 5 seconds!
int ledOffDelay = 5;

// Global sensor distance
volatile unsigned int g_sensorDistance = DEFAULT_DISTANCE;

// Background Light Mode: when active, the entire strip glows with brightness defined by stationaryIntensity
volatile bool backgroundModeActive = false;

// ------------------------- Time and Schedule Parameters -------------------------
int startHour   = 20;
int startMinute = 0;
int endHour     = 8;
int endMinute   = 30;
bool lightOn    = true;
unsigned long lastTimeCheck = 0;

// ------------------------- EEPROM -------------------------
#define EEPROM_SIZE 64

// Load settings from EEPROM
void loadSettings() {
  EEPROM.begin(EEPROM_SIZE);
  int offset = 0;
  // Use reinterpret_cast to avoid warnings
  EEPROM.get(offset, reinterpret_cast<int&>(updateInterval)); offset += sizeof(updateInterval);
  EEPROM.get(offset, reinterpret_cast<int&>(ledOffDelay)); offset += sizeof(ledOffDelay);
  EEPROM.get(offset, reinterpret_cast<float&>(movingIntensity)); offset += sizeof(movingIntensity);
  EEPROM.get(offset, reinterpret_cast<float&>(stationaryIntensity)); offset += sizeof(stationaryIntensity);
  EEPROM.get(offset, reinterpret_cast<int&>(movingLength)); offset += sizeof(movingLength);
  EEPROM.get(offset, reinterpret_cast<int&>(centerShift)); offset += sizeof(centerShift);
  {
    int temp = additionalLEDs;
    EEPROM.get(offset, temp); 
    additionalLEDs = temp;
    offset += sizeof(temp);
  }
  EEPROM.get(offset, baseColor); offset += sizeof(baseColor);
  offset += sizeof(speedMultiplier);  // Skip speedMultiplier
  EEPROM.get(offset, startHour); offset += sizeof(startHour);
  EEPROM.get(offset, startMinute); offset += sizeof(startMinute);
  EEPROM.get(offset, endHour); offset += sizeof(endHour);
  EEPROM.get(offset, endMinute);
  EEPROM.commit();
}

// Save settings to EEPROM
void saveSettings() {
  int offset = 0;
  EEPROM.put(offset, updateInterval); offset += sizeof(updateInterval);
  EEPROM.put(offset, ledOffDelay); offset += sizeof(ledOffDelay);
  EEPROM.put(offset, movingIntensity); offset += sizeof(movingIntensity);
  EEPROM.put(offset, stationaryIntensity); offset += sizeof(stationaryIntensity);
  EEPROM.put(offset, movingLength); offset += sizeof(movingLength);
  EEPROM.put(offset, centerShift); offset += sizeof(centerShift);
  {
    int temp = additionalLEDs;
    EEPROM.put(offset, temp); offset += sizeof(temp);
  }
  EEPROM.put(offset, baseColor); offset += sizeof(baseColor);
  offset += sizeof(speedMultiplier);
  EEPROM.put(offset, startHour); offset += sizeof(startHour);
  EEPROM.put(offset, startMinute); offset += sizeof(startMinute);
  EEPROM.put(offset, endHour); offset += sizeof(endHour);
  EEPROM.put(offset, endMinute);
  EEPROM.commit();
}

// ------------------------- Web Server -------------------------
WebServer server(80);

// HTTP handler prototypes
void handleRoot();
void handleSetInterval();
void handleSetLedOffDelay();
void handleSetBaseColor();
void handleSetMovingIntensity();
void handleSetStationaryIntensity();
void handleSetMovingLength();
void handleSetAdditionalLEDs();
void handleSetCenterShift();
void handleSetTime();
void handleSetSchedule();
void handleNotFound();
void handleDebugPage();
void handleGetSensorData();
void handleSmartHomeOn();
void handleSmartHomeOff();
void handleSmartHomeClear();
void handleToggleBackgroundMode();

// OTA setup prototype
void setupOTA();

// ------------------------- Debug Page and Sensor Data -------------------------
//
// Returns a JSON with the current sensor value
void handleGetSensorData() {
  String json = "{\"current\":" + String(g_sensorDistance) +
                ",\"noise_threshold\":" + String(NOISE_THRESHOLD) + "}";
  server.send(200, "application/json", json);
}

// Debug page with a graph (optimized for mobile portrait orientation)
void handleDebugPage() {
  String html = "<html><head><title>Sensor Debug</title>"
    "<meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>"
    "<style>"
      "body { font-family: Arial; background-color: #333; color: white; padding: 20px; margin: 0; }"
      "canvas { background-color: #222; border: 1px solid #444; width: 100%; max-width: 320px; height: 200px; display: block; margin: auto; }"
      ".data { font-size: 18px; margin: 10px 0; text-align: center; }"
      "a { color: #aaaaff; text-decoration: none; }"
    "</style>"
    "<script>"
      "let chart, dataPoints = [], maxDataPoints = 100;"
      "function initChart() {"
        "const ctx = document.getElementById('sensorChart').getContext('2d');"
        "chart = { canvas: ctx.canvas, width: ctx.canvas.width, height: ctx.canvas.height };"
        "setInterval(updateData, 100);"
        "drawChart();"
      "}"
      "function updateData() {"
        "fetch('/getSensorData')"
          ".then(response => response.json())"
          ".then(data => {"
            "document.getElementById('currentValue').textContent = data.current;"
            "dataPoints.push(data.current);"
            "if (dataPoints.length > maxDataPoints) dataPoints.shift();"
            "drawChart(data.noise_threshold);"
          "});"
      "}"
      "function drawChart(noiseThreshold) {"
        "const ctx = document.getElementById('sensorChart').getContext('2d');"
        "ctx.clearRect(0, 0, chart.canvas.width, chart.canvas.height);"
        "let min = Math.min(...dataPoints), max = Math.max(...dataPoints);"
        "min = Math.max(0, min - 50);"
        "max = max + 50;"
        "ctx.strokeStyle = '#666';"
        "ctx.beginPath();"
        "ctx.moveTo(30, 10);"
        "ctx.lineTo(30, chart.canvas.height - 20);"
        "ctx.stroke();"
        "ctx.beginPath();"
        "ctx.moveTo(30, chart.canvas.height - 20);"
        "ctx.lineTo(chart.canvas.width - 10, chart.canvas.height - 20);"
        "ctx.stroke();"
        "ctx.fillStyle = '#888';"
        "ctx.font = '10px Arial';"
        "const yStep = (chart.canvas.height - 30) / 5, valueStep = (max - min) / 5;"
        "for (let i = 0; i <= 5; i++) {"
          "const y = chart.canvas.height - 20 - (i * yStep);"
          "const value = min + (i * valueStep);"
          "ctx.fillText(Math.round(value), 5, y + 3);"
          "ctx.strokeStyle = '#444';"
          "ctx.beginPath();"
          "ctx.moveTo(30, y);"
          "ctx.lineTo(chart.canvas.width - 10, y);"
          "ctx.stroke();"
        "}"
        "if (noiseThreshold) {"
          "const y = chart.canvas.height - 20 - ((noiseThreshold - min) / (max - min) * (chart.canvas.height - 30));"
          "ctx.strokeStyle = '#ffaa00';"
          "ctx.beginPath();"
          "ctx.moveTo(30, y);"
          "ctx.lineTo(chart.canvas.width - 10, y);"
          "ctx.stroke();"
          "ctx.fillText('Noise', chart.canvas.width - 50, y - 5);"
        "}"
        "if (dataPoints.length > 1) {"
          "const xStep = (chart.canvas.width - 40) / (maxDataPoints - 1);"
          "ctx.strokeStyle = '#00aaff';"
          "ctx.lineWidth = 2;"
          "ctx.beginPath();"
          "for (let i = 0; i < dataPoints.length; i++) {"
            "const x = 30 + (i * xStep);"
            "const normalizedValue = (dataPoints[i] - min) / (max - min);"
            "const y = chart.canvas.height - 20 - (normalizedValue * (chart.canvas.height - 30));"
            "if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);"
          "}"
          "ctx.stroke();"
        "}"
      "}"
    "</script>"
    "</head><body onload='initChart()'>"
      "<h1 style='text-align:center;'>Sensor Debug</h1>"
      "<div class='data'>Current Value: <span id='currentValue'>-</span></div>"
      "<canvas id='sensorChart'></canvas>"
      "<p style='text-align:center;'><a href='/'>&larr; Return to main page</a></p>"
    "</body></html>";
  server.send(200, "text/html", html);
}

// ------------------------- Smart Home Integration Endpoints -------------------------
volatile bool smarthomeOverride = false;
void handleSmartHomeOn() {
  lightOn = true;
  smarthomeOverride = true;
  server.send(200, "text/plain", "Smart Home Override: ON");
}
void handleSmartHomeOff() {
  lightOn = false;
  smarthomeOverride = true;
  server.send(200, "text/plain", "Smart Home Override: OFF");
}
void handleSmartHomeClear() {
  smarthomeOverride = false;
  time_t nowSec = time(nullptr);
  if (nowSec < 1000000000UL)
    lightOn = true;
  else {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      int currentTotal = timeinfo.tm_hour * 60 + timeinfo.tm_min;
      int startTotal = startHour * 60 + startMinute;
      int endTotal = endHour * 60 + endMinute;
      if (startTotal <= endTotal)
        lightOn = (currentTotal >= startTotal && currentTotal < endTotal);
      else
        lightOn = (currentTotal >= startTotal || currentTotal < endTotal);
    }
  }
  server.send(200, "text/plain", "Smart Home Override: CLEARED");
}

// ------------------------- New HTTP Handler for Toggling Background Light Mode -------------------------
void handleToggleBackgroundMode() {
  // Toggle the background light mode state
  backgroundModeActive = !backgroundModeActive;
  server.sendHeader("Location", "/");
  server.send(303);
}

// ------------------------- Sensor Reading Function -------------------------
unsigned int readSensorData() {
#ifndef SIMULATE_SENSOR
  if (Serial1.available() < 7) return g_sensorDistance;
  if (Serial1.read() != SENSOR_HEADER) {
    while (Serial1.available()) Serial1.read();
    return g_sensorDistance;
  }
  if (Serial1.read() != SENSOR_HEADER) {
    while (Serial1.available()) Serial1.read();
    return g_sensorDistance;
  }
  byte buf[5];
  size_t bytesRead = Serial1.readBytes(buf, 5);
  if (bytesRead < 5) return g_sensorDistance;
  unsigned int distance = (buf[2] << 8) | buf[1];
  if (distance < MIN_DISTANCE || distance > MAX_DISTANCE) return g_sensorDistance;
  return distance;
#else
  static unsigned int simulatedDistance = MIN_DISTANCE;
  simulatedDistance += 10;
  if (simulatedDistance > MAX_DISTANCE) simulatedDistance = MIN_DISTANCE;
  return simulatedDistance;
#endif
}

// ------------------------- Sensor Task (no filtering) -------------------------
void sensorTask(void * parameter) {
  for (;;) {
    unsigned int newDistance = readSensorData();
    g_sensorDistance = newDistance;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ------------------------- LED Task -------------------------
void ledTask(void * parameter) {
  static unsigned int lastSensor = g_sensorDistance;
  static int lastMovementDirection = 0;
  static unsigned long lastMovementTime = millis();

  for (;;) {
    unsigned long currentMillis = millis();
    unsigned int currentDistance = g_sensorDistance;
    int diff = (int)currentDistance - (int)lastSensor;
    int absDiff = abs(diff);

    Serial.print("Distance: ");
    Serial.print(currentDistance);
    Serial.print(" | lastMovementTime: ");
    Serial.println(lastMovementTime);

    if (absDiff >= NOISE_THRESHOLD) {
      lastMovementTime = currentMillis;
      lastMovementDirection = (diff > 0) ? 1 : -1;
    }
    lastSensor = currentDistance;

    // Determine if we should draw the moving light beam based on the LED off delay
    bool drawMovingPart = (currentMillis - lastMovementTime <= ledOffDelay * 1000);

    // If light is off, clear the strip
    if (!lightOn) {
      fill_solid(leds, NUM_LEDS, CRGB::Black);
    }
    else {
      // If background mode is active, display the background glow regardless of motion
      if (backgroundModeActive) {
        fill_solid(leds, NUM_LEDS, CRGB((uint8_t)(baseColor.r * stationaryIntensity),
                                        (uint8_t)(baseColor.g * stationaryIntensity),
                                        (uint8_t)(baseColor.b * stationaryIntensity)));
        // Overlay the moving light beam if motion is detected
        if (drawMovingPart) {
          float prop = constrain((float)(currentDistance - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE), 0.0, 1.0);
          int ledPosition = (diff < 0) ? ceil(prop * (NUM_LEDS - movingLength)) : round(prop * (NUM_LEDS - movingLength));
          int centerLED = ledPosition + centerShift;
          centerLED = constrain(centerLED, 0, NUM_LEDS - 1);
          int halfLength = movingLength / 2;
          if (movingLength <= 1) {
            leds[centerLED] = CRGB((uint8_t)(baseColor.r * movingIntensity),
                                   (uint8_t)(baseColor.g * movingIntensity),
                                   (uint8_t)(baseColor.b * movingIntensity));
          } else {
            int fadeWidthMain = min(halfLength, 5);
            for (int offset = -halfLength; offset < halfLength; offset++) {
              int idx = (centerLED + offset + NUM_LEDS) % NUM_LEDS;
              int rIndex = offset + halfLength;
              float factor = 1.0;
              if (additionalLEDs == 0) {
                if (fadeWidthMain > 1 && rIndex < fadeWidthMain) {
                  factor = (float)rIndex / (fadeWidthMain - 1);
                } else if (fadeWidthMain > 1 && rIndex >= movingLength - fadeWidthMain) {
                  factor = (float)(movingLength - 1 - rIndex) / (fadeWidthMain - 1);
                }
              } else {
                if (lastMovementDirection > 0) { // Additional beam on the right: fade only on the left edge
                  if (fadeWidthMain > 1 && rIndex < fadeWidthMain) {
                    factor = (float)rIndex / (fadeWidthMain - 1);
                  }
                } else if (lastMovementDirection < 0) { // Additional beam on the left: fade only on the right edge
                  if (fadeWidthMain > 1 && rIndex >= movingLength - fadeWidthMain) {
                    factor = (float)(movingLength - 1 - rIndex) / (fadeWidthMain - 1);
                  }
                } else {
                  if (fadeWidthMain > 1 && rIndex < fadeWidthMain) {
                    factor = (float)rIndex / (fadeWidthMain - 1);
                  } else if (fadeWidthMain > 1 && rIndex >= movingLength - fadeWidthMain) {
                    factor = (float)(movingLength - 1 - rIndex) / (fadeWidthMain - 1);
                  }
                }
              }
              leds[idx] = CRGB((uint8_t)(baseColor.r * movingIntensity * factor),
                               (uint8_t)(baseColor.g * movingIntensity * factor),
                               (uint8_t)(baseColor.b * movingIntensity * factor));
            }
          }
          if (lastMovementDirection != 0 && additionalLEDs > 0) {
            int fadeWidthAdditional = min(additionalLEDs, 5);
            for (int i = 0; i < additionalLEDs; i++) {
              int idx = (lastMovementDirection > 0) ? centerLED + halfLength + i : centerLED - halfLength - i;
              if (idx < 0 || idx >= NUM_LEDS) break;
              float factor = 1.0;
              if (additionalLEDs > 1) {
                if (i >= additionalLEDs - fadeWidthAdditional) {
                  factor = (float)(additionalLEDs - 1 - i) / (fadeWidthAdditional - 1);
                }
              }
              leds[idx] = CRGB((uint8_t)(baseColor.r * movingIntensity * factor),
                               (uint8_t)(baseColor.g * movingIntensity * factor),
                               (uint8_t)(baseColor.b * movingIntensity * factor));
            }
          }
        }
      }
      else { 
        // If background mode is off, display the moving light beam on a black background
        if (drawMovingPart) {
          fill_solid(leds, NUM_LEDS, CRGB::Black);
          float prop = constrain((float)(currentDistance - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE), 0.0, 1.0);
          int ledPosition = (diff < 0) ? ceil(prop * (NUM_LEDS - movingLength)) : round(prop * (NUM_LEDS - movingLength));
          int centerLED = ledPosition + centerShift;
          centerLED = constrain(centerLED, 0, NUM_LEDS - 1);
          int halfLength = movingLength / 2;
          if (movingLength <= 1) {
            leds[centerLED] = CRGB((uint8_t)(baseColor.r * movingIntensity),
                                   (uint8_t)(baseColor.g * movingIntensity),
                                   (uint8_t)(baseColor.b * movingIntensity));
          } else {
            int fadeWidthMain = min(halfLength, 5);
            for (int offset = -halfLength; offset < halfLength; offset++) {
              int idx = (centerLED + offset + NUM_LEDS) % NUM_LEDS;
              int rIndex = offset + halfLength;
              float factor = 1.0;
              if (additionalLEDs == 0) {
                if (fadeWidthMain > 1 && rIndex < fadeWidthMain) {
                  factor = (float)rIndex / (fadeWidthMain - 1);
                } else if (fadeWidthMain > 1 && rIndex >= movingLength - fadeWidthMain) {
                  factor = (float)(movingLength - 1 - rIndex) / (fadeWidthMain - 1);
                }
              } else {
                if (lastMovementDirection > 0) {
                  if (fadeWidthMain > 1 && rIndex < fadeWidthMain) {
                    factor = (float)rIndex / (fadeWidthMain - 1);
                  }
                } else if (lastMovementDirection < 0) {
                  if (fadeWidthMain > 1 && rIndex >= movingLength - fadeWidthMain) {
                    factor = (float)(movingLength - 1 - rIndex) / (fadeWidthMain - 1);
                  }
                } else {
                  if (fadeWidthMain > 1 && rIndex < fadeWidthMain) {
                    factor = (float)rIndex / (fadeWidthMain - 1);
                  } else if (fadeWidthMain > 1 && rIndex >= movingLength - fadeWidthMain) {
                    factor = (float)(movingLength - 1 - rIndex) / (fadeWidthMain - 1);
                  }
                }
              }
              leds[idx] = CRGB((uint8_t)(baseColor.r * movingIntensity * factor),
                               (uint8_t)(baseColor.g * movingIntensity * factor),
                               (uint8_t)(baseColor.b * movingIntensity * factor));
            }
          }
          if (lastMovementDirection != 0 && additionalLEDs > 0) {
            int fadeWidthAdditional = min(additionalLEDs, 5);
            for (int i = 0; i < additionalLEDs; i++) {
              int idx = (lastMovementDirection > 0) ? centerLED + halfLength + i : centerLED - halfLength - i;
              if (idx < 0 || idx >= NUM_LEDS) break;
              float factor = 1.0;
              if (additionalLEDs > 1) {
                if (i >= additionalLEDs - fadeWidthAdditional) {
                  factor = (float)(additionalLEDs - 1 - i) / (fadeWidthAdditional - 1);
                }
              }
              leds[idx] = CRGB((uint8_t)(baseColor.r * movingIntensity * factor),
                               (uint8_t)(baseColor.g * movingIntensity * factor),
                               (uint8_t)(baseColor.b * movingIntensity * factor));
            }
          }
        }
        else {
          fill_solid(leds, NUM_LEDS, CRGB::Black);
        }
      }
    }

    FastLED.show();
    vTaskDelay(pdMS_TO_TICKS(updateInterval));
  }
}

// ------------------------- Debug Task -------------------------
void debugTask(void * parameter) {
  for (;;) {
    Serial.print("Sensor distance: ");
    Serial.println(g_sensorDistance);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ------------------------- Web Server Task -------------------------
void webServerTask(void * parameter) {
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ------------------------- Time Functions -------------------------
void handleSetTime() {
  if (server.hasArg("epoch")) {
    unsigned long epoch = server.arg("epoch").toInt();
    if (server.hasArg("tz")) {
      int tz = server.arg("tz").toInt();
      epoch += tz * 60;
    }
    if (epoch > 1000000000UL) {
      struct timeval tv;
      tv.tv_sec = epoch;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        int currentTotal = timeinfo.tm_hour * 60 + timeinfo.tm_min;
        int startTotal = startHour * 60 + startMinute;
        int endTotal = endHour * 60 + endMinute;
        if (startTotal <= endTotal)
          lightOn = (currentTotal >= startTotal && currentTotal < endTotal);
        else
          lightOn = (currentTotal >= startTotal || currentTotal < endTotal);
      }
    }
  }
  server.send(200, "text/plain", "OK");
}

void updateTime() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastTimeCheck >= 1000) {
    lastTimeCheck = currentMillis;
    time_t nowSec = time(nullptr);
    if (nowSec < 1000000000UL)
      lightOn = true;
    else {
      if (!smarthomeOverride) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          int currentTotal = timeinfo.tm_hour * 60 + timeinfo.tm_min;
          int startTotal = startHour * 60 + startMinute;
          int endTotal = endHour * 60 + endMinute;
          if (startTotal <= endTotal)
            lightOn = (currentTotal >= startTotal && currentTotal < endTotal);
          else
            lightOn = (currentTotal >= startTotal || currentTotal < endTotal);
        }
      }
    }
  }
}

void handleSetSchedule() {
  if (server.hasArg("startHour") && server.hasArg("startMinute") &&
      server.hasArg("endHour") && server.hasArg("endMinute")) {
    startHour = server.arg("startHour").toInt();
    startMinute = server.arg("startMinute").toInt();
    endHour = server.arg("endHour").toInt();
    endMinute = server.arg("endMinute").toInt();
    saveSettings();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

// ------------------------- WiFi Setup -------------------------
void setupWiFi() {
  WiFi.mode(WIFI_AP);
  IPAddress local_IP(192, 168, 4, 22);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  // Generate a unique access point name using the MAC address
  String macStr = String(ESP.getEfuseMac(), HEX);
  macStr.toUpperCase();
  String uniqueID = macStr.substring(macStr.length() - 6);
  String deviceName = "LightTrack " + uniqueID;
  WiFi.softAP(deviceName.c_str(), "12345678");

  // Reduce WiFi power by 30% (from 55 to ~39)
  esp_wifi_set_max_tx_power(21);

  Serial.println("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Register HTTP handlers
  server.on("/", handleRoot);
  server.on("/setInterval", handleSetInterval);
  server.on("/setLedOffDelay", handleSetLedOffDelay);
  server.on("/setBaseColor", handleSetBaseColor);
  server.on("/setMovingIntensity", handleSetMovingIntensity);
  server.on("/setStationaryIntensity", handleSetStationaryIntensity);
  server.on("/setMovingLength", handleSetMovingLength);
  server.on("/setAdditionalLEDs", handleSetAdditionalLEDs);
  server.on("/setCenterShift", handleSetCenterShift);
  server.on("/setTime", handleSetTime);
  server.on("/setSchedule", handleSetSchedule);
  server.on("/smarthome/on", handleSmartHomeOn);
  server.on("/smarthome/off", handleSmartHomeOff);
  server.on("/smarthome/clear", handleSmartHomeClear);
  server.on("/toggleNightMode", handleToggleBackgroundMode);
  server.on("/debug", handleDebugPage);
  server.on("/getSensorData", handleGetSensorData);
  server.onNotFound(handleNotFound);
  
  server.begin();
}

// ------------------------- Web Interface Handler -------------------------
void handleRoot() {
  char scheduleStartStr[6];
  sprintf(scheduleStartStr, "%02d:%02d", startHour, startMinute);
  char scheduleEndStr[6];
  sprintf(scheduleEndStr, "%02d:%02d", endHour, endMinute);

  String html = "<html><head><title>LED Control</title>"
    "<meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>"
    "<style>"
      "body { margin: 0; padding: 0; background-color: #333; color: white; font-family: Arial, sans-serif; }"
      ".container { text-align: center; width: 90%; max-width: 800px; margin: auto; padding-top: 20px; }"
      ".lighttrack { font-size: 1.4em; }"
      ".footer { font-size: 1em; margin-top: 20px; }"
      "input[type=range] { -webkit-appearance: none; width: 100%; height: 25px; background: transparent; }"
      "input[type=range]:focus { outline: none; }"
      "input[type=range]::-webkit-slider-runnable-track { height: 8px; background: #F5F5DC; border-radius: 4px; }"
      "input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; height: 25px; width: 25px; background: #fff; border: 2px solid #ccc; border-radius: 50%; margin-top: -9px; }"
      "input[type=range]::-moz-range-track { height: 8px; background: #F5F5DC; border-radius: 4px; }"
      "input[type=range]::-moz-range-thumb { height: 25px; width: 25px; background: #fff; border: 2px solid #ccc; border-radius: 50%; }"
      "input[type=color] { width: 100px; height: 100px; border: none; }"
      "input[type=time] { font-size: 1.2em; margin: 5px; }"
      "button { font-size: 1em; margin: 5px; padding: 10px; }"
    "</style>"
    "<script>"
      "function setDeviceTime() { " 
        "var now = new Date(); " 
        "var epoch = Math.floor(now.getTime()/1000); " 
        "var tz = -now.getTimezoneOffset(); " 
        "fetch('/setTime?epoch='+epoch+'&tz='+tz); " 
      "}"
      "function changeBaseColor(hex) { "
        "var r = parseInt(hex.substring(1,3),16); "
        "var g = parseInt(hex.substring(3,5),16); "
        "var b = parseInt(hex.substring(5,7),16); "
        "fetch('/setBaseColor?r=' + r + '&g=' + g + '&b=' + b);"
      "}"
      "function setMovingIntensity(val) { fetch('/setMovingIntensity?value=' + val); }"
      "function setMovingLength(val) { fetch('/setMovingLength?value=' + val); }"
      "function setAdditionalLEDs(val) { fetch('/setAdditionalLEDs?value=' + val); }"
      "function setCenterShift(val) { fetch('/setCenterShift?value=' + val); }"
      "function setIntervalVal(val) { fetch('/setInterval?value=' + val); }"
      "function setLedOffDelay(val) { fetch('/setLedOffDelay?value=' + val); }"
      "function setSchedule(startTime, endTime) { "
         "var sParts = startTime.split(':'); "
         "var eParts = endTime.split(':'); "
         "fetch('/setSchedule?startHour=' + sParts[0] + '&startMinute=' + sParts[1] + "
              "'&endHour=' + eParts[0] + '&endMinute=' + eParts[1]); "
      "}"
      "function setStationaryIntensityValue(val) { fetch('/setStationaryIntensity?value=' + val * 0.01); }"
      "function toggleBackgroundMode() {"
         "fetch('/toggleNightMode').then(()=>location.reload());"
      "}"
    "</script>"
    "</head><body onload='setDeviceTime()'>"
    "<div class='container'>"
      "<h1 class='lighttrack'>LED Control Panel</h1>"
      "<div style='display: flex; justify-content: center; margin-bottom:10px;'>"
        "<input type='color' id='baseColorPicker' value='#" 
          + String((baseColor.r < 16 ? "0" : "") + String(baseColor.r, HEX))
          + String((baseColor.g < 16 ? "0" : "") + String(baseColor.g, HEX))
          + String((baseColor.b < 16 ? "0" : "") + String(baseColor.b, HEX))
          + "' onchange='changeBaseColor(this.value)'>"
      "</div>"
      "<p>Moving Light Intensity: <span id='movingIntensityValue'>" + String(movingIntensity) + "</span></p>"
      "<input type='range' min='0' max='1' step='0.01' value='" + String(movingIntensity) + "' oninput='document.getElementById(\"movingIntensityValue\").innerText = this.value' onchange='setMovingIntensity(this.value)'>"
      "<p>Moving Light Length: <span id='movingLengthValue'>" + String(movingLength) + "</span></p>"
      "<input type='range' min='1' max='300' step='1' value='" + String(movingLength) + "' oninput='document.getElementById(\"movingLengthValue\").innerText = this.value' onchange='setMovingLength(this.value)'>"
      "<p>Additional LEDs (direction): <span id='additionalLEDsValue'>" + String(additionalLEDs) + "</span></p>"
      "<input type='range' min='0' max='100' step='1' value='" + String(additionalLEDs) + "' oninput='document.getElementById(\"additionalLEDsValue\").innerText = this.value' onchange='setAdditionalLEDs(this.value)'>"
      "<p>Center Shift (LEDs): <span id='centerShiftValue'>" + String(centerShift) + "</span></p>"
      "<input type='range' min='-100' max='100' step='1' value='" + String(centerShift) + "' oninput='document.getElementById(\"centerShiftValue\").innerText = this.value' onchange='setCenterShift(this.value)'>"
      "<p>LED Off Delay (seconds): <span id='ledOffDelayValue'>" + String(ledOffDelay) + "</span></p>"
      "<input type='range' min='1' max='60' step='1' value='" + String(ledOffDelay) + "' oninput='document.getElementById(\"ledOffDelayValue\").innerText = this.value' onchange='setLedOffDelay(this.value)'>"
      "<p>Background Light Mode:</p>"
      "<button onclick='toggleBackgroundMode()'>Toggle Background Light</button>"
      "<p>LED Light Intensity: <span id='stationaryIntensityValue'>" + String(stationaryIntensity * 100) + "</span></p>"
      "<input type='range' min='0' max='7' step='0.01' value='" + String(stationaryIntensity * 100) + "' oninput='document.getElementById(\"stationaryIntensityValue\").innerText = this.value' onchange='setStationaryIntensityValue(this.value)'>"
      "<hr style='border: none; height: 2px; background: #fff; margin: 20px 0;'>"
      "<p>Schedule Window (From - To):</p>"
      "<div style='display: flex; justify-content: center; gap: 15px;'>"
        "<input type='time' id='scheduleStartInput' value='" + String(scheduleStartStr) + "' onchange='setSchedule(this.value, document.getElementById(\"scheduleEndInput\").value)'>"
        "<input type='time' id='scheduleEndInput' value='" + String(scheduleEndStr) + "' onchange='setSchedule(document.getElementById(\"scheduleStartInput\").value, this.value)'>"
      "</div>"
      "<p><a href='/debug' style='color:#aaaaff'>Sensor Debug</a></p>"
      "<div class='footer'>DIY Yari</div>"
    "</div></body></html>";
  
  server.send(200, "text/html", html);
}

// ------------------------- HTTP Handlers -------------------------
void handleSetInterval() {
  if (server.hasArg("value")) { updateInterval = server.arg("value").toInt(); }
  saveSettings();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetLedOffDelay() {
  if (server.hasArg("value")) { ledOffDelay = server.arg("value").toInt(); }
  saveSettings();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetBaseColor() {
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    baseColor = CRGB(server.arg("r").toInt(), server.arg("g").toInt(), server.arg("b").toInt());
  }
  saveSettings();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetMovingIntensity() {
  if (server.hasArg("value")) { movingIntensity = server.arg("value").toFloat(); }
  saveSettings();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetStationaryIntensity() {
    if (server.hasArg("value")) {
      float val = server.arg("value").toFloat();
      if(val < 0.01) val = 0.0;
      // Limit the maximum intensity to 0.07 (adjust this value as needed)
      val = constrain(val, 0.0, 0.07);
      stationaryIntensity = val;
    }
    saveSettings();
    server.sendHeader("Location", "/");
    server.send(303);
  }

void handleSetMovingLength() {
  if (server.hasArg("value")) { movingLength = server.arg("value").toInt(); }
  saveSettings();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetAdditionalLEDs() {
  if (server.hasArg("value")) { additionalLEDs = server.arg("value").toInt(); }
  saveSettings();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetCenterShift() {
  if (server.hasArg("value")) { centerShift = server.arg("value").toInt(); }
  saveSettings();
  server.sendHeader("Location", "/");
  server.send(303);
}

// ------------------------- OTA Setup Function -------------------------
void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
    // In this version of ArduinoOTA, getFilename() is not available so we only output the update type.
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

// ------------------------- Setup -------------------------
void setup() {
  Serial.begin(115200);
  
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    return;
  }
  
  EEPROM.begin(EEPROM_SIZE);
  loadSettings();
  
  Serial1.begin(256000, SERIAL_8N1, 20, 21);
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();
  
  configTzTime("UTC0", "");
  
  setupWiFi();
  setupOTA();  // Setup OTA
  
  xTaskCreatePinnedToCore(sensorTask, "Sensor Task", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(ledTask, "LED Task", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(webServerTask, "WebServer Task", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(debugTask, "Debug Task", 2048, NULL, 1, NULL, 1);
}

// ------------------------- Loop -------------------------
void loop() {
  ArduinoOTA.handle();  // Handle OTA
  updateTime();
  vTaskDelay(pdMS_TO_TICKS(1000));
}
