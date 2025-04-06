#include "web_server.h"
#include "config.h"
#include "storage.h"
#include "sensor_manager.h"
#include "wifi_manager.h"
#include "home_assistant.h"
#include <time.h>
#include <WiFi.h>
#include <stdio.h>

// Web Server
WebServer server(80);

// Smart home override flag
volatile bool smarthomeOverride = false;

// Forward declarations for HTTP handlers
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
void handleMqttSettings();
void handleMqttSave();

bool isSmartHomeOverride() {
  return smarthomeOverride;
}

void clearSmartHomeOverride() {
  smarthomeOverride = false;
}

void initWebServer() {
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
  
  // Настройки WiFi и MQTT
  server.on("/wifi", handleWiFiSettings);
  server.on("/savewifi", HTTP_POST, handleWiFiSave);
  server.on("/mqtt", handleMqttSettings);
  server.on("/savemqtt", HTTP_POST, handleMqttSave);
  
  server.onNotFound(handleNotFound);
  
  server.begin();
}

void webServerTask(void * parameter) {
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Returns a JSON with the current sensor value
void handleGetSensorData() {
  String json = "{\"current\":" + String(getSensorDistance()) +
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

// Smart Home Integration Endpoints
void handleSmartHomeOn() {
  setLightOn(true);
  smarthomeOverride = true;
  server.send(200, "text/plain", "Smart Home Override: ON");
}

void handleSmartHomeOff() {
  setLightOn(false);
  smarthomeOverride = true;
  server.send(200, "text/plain", "Smart Home Override: OFF");
}

void handleSmartHomeClear() {
  smarthomeOverride = false;
  time_t nowSec = time(nullptr);
  if (nowSec < 1000000000UL)
    setLightOn(true);
  else {
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
  server.send(200, "text/plain", "Smart Home Override: CLEARED");
}

// Toggle Background Light Mode Handler
void handleToggleBackgroundMode() {
  toggleBackgroundMode();
  server.sendHeader("Location", "/");
  server.send(303);
}

// Time Functions
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
        int startTotal = getStartHour() * 60 + getStartMinute();
        int endTotal = getEndHour() * 60 + getEndMinute();
        if (startTotal <= endTotal)
          setLightOn(currentTotal >= startTotal && currentTotal < endTotal);
        else
          setLightOn(currentTotal >= startTotal || currentTotal < endTotal);
      }
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleSetSchedule() {
  if (server.hasArg("startHour") && server.hasArg("startMinute") &&
      server.hasArg("endHour") && server.hasArg("endMinute")) {
    setStartHour(server.arg("startHour").toInt());
    setStartMinute(server.arg("startMinute").toInt());
    setEndHour(server.arg("endHour").toInt());
    setEndMinute(server.arg("endMinute").toInt());
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

void handleSetInterval() {
  if (server.hasArg("value")) {
    setUpdateInterval(server.arg("value").toInt());
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetLedOffDelay() {
  if (server.hasArg("value")) {
    setLedOffDelay(server.arg("value").toInt());
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetBaseColor() {
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    CRGB color = CRGB(
      server.arg("r").toInt(),
      server.arg("g").toInt(),
      server.arg("b").toInt()
    );
    setBaseColor(color);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetMovingIntensity() {
  if (server.hasArg("value")) {
    setMovingIntensity(server.arg("value").toFloat());
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetStationaryIntensity() {
  if (server.hasArg("value")) {
    setStationaryIntensity(server.arg("value").toFloat());
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetMovingLength() {
  if (server.hasArg("value")) {
    setMovingLength(server.arg("value").toInt());
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetAdditionalLEDs() {
  if (server.hasArg("value")) {
    setAdditionalLEDs(server.arg("value").toInt());
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetCenterShift() {
  if (server.hasArg("value")) {
    setCenterShift(server.arg("value").toInt());
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// Web Interface Handler
void handleRoot() {
  char scheduleStartStr[6];
  sprintf(scheduleStartStr, "%02d:%02d", getStartHour(), getStartMinute());
  
  char scheduleEndStr[6];
  sprintf(scheduleEndStr, "%02d:%02d", getEndHour(), getEndMinute());

  CRGB baseColor = getBaseColor();
  
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
      ".nav-links { margin: 20px 0; }"
      ".nav-links a { display: inline-block; margin: 0 10px; color: #aaaaff; text-decoration: none; }"
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
      "<p>Moving Light Intensity: <span id='movingIntensityValue'>" + String(getMovingIntensity()) + "</span></p>"
      "<input type='range' min='0' max='1' step='0.01' value='" + String(getMovingIntensity()) + "' oninput='document.getElementById(\"movingIntensityValue\").innerText = this.value' onchange='setMovingIntensity(this.value)'>"
      "<p>Moving Light Length: <span id='movingLengthValue'>" + String(getMovingLength()) + "</span></p>"
      "<input type='range' min='1' max='300' step='1' value='" + String(getMovingLength()) + "' oninput='document.getElementById(\"movingLengthValue\").innerText = this.value' onchange='setMovingLength(this.value)'>"
      "<p>Additional LEDs (direction): <span id='additionalLEDsValue'>" + String(getAdditionalLEDs()) + "</span></p>"
      "<input type='range' min='0' max='100' step='1' value='" + String(getAdditionalLEDs()) + "' oninput='document.getElementById(\"additionalLEDsValue\").innerText = this.value' onchange='setAdditionalLEDs(this.value)'>"
      "<p>Center Shift (LEDs): <span id='centerShiftValue'>" + String(getCenterShift()) + "</span></p>"
      "<input type='range' min='-100' max='100' step='1' value='" + String(getCenterShift()) + "' oninput='document.getElementById(\"centerShiftValue\").innerText = this.value' onchange='setCenterShift(this.value)'>"
      "<p>LED Off Delay (seconds): <span id='ledOffDelayValue'>" + String(getLedOffDelay()) + "</span></p>"
      "<input type='range' min='1' max='60' step='1' value='" + String(getLedOffDelay()) + "' oninput='document.getElementById(\"ledOffDelayValue\").innerText = this.value' onchange='setLedOffDelay(this.value)'>"
      "<p>Background Light Mode:</p>"
      "<button onclick='toggleBackgroundMode()'>" + String(isBackgroundModeActive() ? "Disable" : "Enable") + " Background Light</button>"
      "<p>LED Light Intensity: <span id='stationaryIntensityValue'>" + String(getStationaryIntensity() * 100) + "</span></p>"
      "<input type='range' min='0' max='7' step='0.01' value='" + String(getStationaryIntensity() * 100) + "' oninput='document.getElementById(\"stationaryIntensityValue\").innerText = this.value' onchange='setStationaryIntensityValue(this.value)'>"
      "<hr style='border: none; height: 2px; background: #fff; margin: 20px 0;'>"
      "<p>Schedule Window (From - To):</p>"
      "<div style='display: flex; justify-content: center; gap: 15px;'>"
        "<input type='time' id='scheduleStartInput' value='" + String(scheduleStartStr) + "' onchange='setSchedule(this.value, document.getElementById(\"scheduleEndInput\").value)'>"
        "<input type='time' id='scheduleEndInput' value='" + String(scheduleEndStr) + "' onchange='setSchedule(document.getElementById(\"scheduleStartInput\").value, this.value)'>"
      "</div>"
      "<div class='nav-links'>"
        "<a href='/debug'>Sensor Debug</a> | "
        "<a href='/wifi'>WiFi Settings</a> | "
        "<a href='/mqtt'>MQTT Settings</a>"
      "</div>";
      
  // Show connection status if connected to WiFi
  if (WiFi.status() == WL_CONNECTED) {
    html += "<p>WiFi: Connected to " + WiFi.SSID() + " (" + WiFi.localIP().toString() + ")</p>";
  }
      
  html += "<div class='footer'>DIY Yari</div>"
    "</div></body></html>";
  
  server.send(200, "text/html", html);
}

// MQTT Settings Page
void handleMqttSettings() {
  String html = "<html><head><title>MQTT Settings</title>"
    "<meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>"
    "<style>"
      "body { margin: 0; padding: 0; background-color: #333; color: white; font-family: Arial, sans-serif; }"
      ".container { text-align: center; width: 90%; max-width: 800px; margin: auto; padding-top: 20px; }"
      "input[type=text], input[type=password], input[type=number] { width: 80%; padding: 8px; margin: 5px; }"
      "input[type=submit] { padding: 10px 20px; margin: 15px; background-color: #4CAF50; color: white; border: none; cursor: pointer; }"
      "a { color: #aaaaff; text-decoration: none; }"
      ".note { color: #ffcc00; background-color: #333333; border: 1px solid #ffcc00; padding: 10px; margin: 15px 0; border-radius: 5px; }"
    "</style>"
    "</head><body>"
    "<div class='container'>"
      "<h1>MQTT Settings</h1>"
      "<div class='note'>"
        "<p>Устройство работает только в режиме точки доступа.</p>"
        "<p>Для подключения к MQTT-брокеру введите его IP-адрес.</p>"
        "<p>Важно: IP-адрес должен быть доступен напрямую из сети устройства.</p>"
      "</div>"
      "<form action='/savemqtt' method='post'>"
        "<p>MQTT Server:</p>"
        "<input type='text' name='server' value='" + getMqttServer() + "'>"
        "<p>Port:</p>"
        "<input type='number' name='port' value='" + String(getMqttPort()) + "'>"
        "<p>Username (if needed):</p>"
        "<input type='text' name='user' value='" + getMqttUser() + "'>"
        "<p>Password (if needed):</p>"
        "<input type='password' name='password' value='" + getMqttPassword() + "'>"
        "<br>"
        "<input type='submit' value='Save'>"
      "</form>"
      "<p><a href='/'>Back to main page</a></p>"
    "</div></body></html>";
  
  server.send(200, "text/html", html);
}

// MQTT Save Handler
void handleMqttSave() {
  if (server.hasArg("server")) {
    String mqttServer = server.arg("server");
    int mqttPort = server.hasArg("port") ? server.arg("port").toInt() : 1883;
    String mqttUser = server.hasArg("user") ? server.arg("user") : "";
    String mqttPassword = server.hasArg("password") ? server.arg("password") : "";
    
    saveMqttSettings(mqttServer.c_str(), mqttPort, mqttUser.c_str(), mqttPassword.c_str());
    
    // Connect to MQTT with new settings
    setMqttServer(mqttServer);
  }
  
  server.sendHeader("Location", "/mqtt");
  server.send(303);
}