#include "home_assistant.h"
#include "config.h"
#include "storage.h"
#include "wifi_manager.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// MQTT client
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// MQTT settings
bool mqttEnabled = false;
char mqttClientId[50];
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastHADiscoveryTime = 0;

// MQTT topics
String baseTopic;
String stateTopic;
String commandTopic;
String availabilityTopic;

// Forward declarations
bool reconnectMqtt();
void sendHomeAssistantDiscovery();
void publishState();
void mqttCallback(char* topic, byte* payload, unsigned int length);

void initHomeAssistant() {
  // Create client ID from device name
  String deviceName = getDeviceName();
  deviceName.replace(" ", "_");
  strcpy(mqttClientId, deviceName.c_str());
  
  // Create base topic
  baseTopic = String(MQTT_NODE_ID) + "/" + deviceName;
  stateTopic = baseTopic + "/state";
  commandTopic = baseTopic + "/set";
  availabilityTopic = baseTopic + "/availability";
  
  // Используем сохраненные настройки MQTT или дефолтные
  // Для прямой работы с MQTT без WiFi можно закомментировать проверку
  if (hasMqttSettings()) {
    setMqttServer(getMqttServer());
  }
}

void handleHomeAssistant() {
  // Если не включен MQTT или нет настроек - просто выходим
  if (!mqttEnabled || !hasMqttSettings()) {
    return;
  }
  
  // В режиме AP подключение WiFi всегда будет не WL_CONNECTED, поэтому закомментируем эту проверку
  // if (WiFi.status() != WL_CONNECTED) return;
  
  // Handle reconnection if needed
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > MQTT_RECONNECT_DELAY) {
      lastMqttReconnectAttempt = now;
      if (reconnectMqtt()) {
        lastMqttReconnectAttempt = 0;
      }
    }
  } else {
    // MQTT client loop
    mqttClient.loop();
    
    // Send discovery information periodically
    unsigned long now = millis();
    if (now - lastHADiscoveryTime > HA_DISCOVERY_DELAY) {
      lastHADiscoveryTime = now;
      sendHomeAssistantDiscovery();
      publishState();
    }
  }
}

bool reconnectMqtt() {
  if (!hasMqttSettings()) {
    return false;
  }
  
  String mqttServer = getMqttServer();
  int mqttPort = getMqttPort();
  String mqttUser = getMqttUser();
  String mqttPassword = getMqttPassword();
  
  mqttClient.setServer(mqttServer.c_str(), mqttPort);
  mqttClient.setCallback(mqttCallback);
  
  Serial.print("Attempting MQTT connection to ");
  Serial.print(mqttServer);
  Serial.print("...");
  
  bool success = false;
  
  if (mqttUser.length() > 0) {
    success = mqttClient.connect(mqttClientId, mqttUser.c_str(), mqttPassword.c_str(), 
                           availabilityTopic.c_str(), 0, true, "offline");
  } else {
    success = mqttClient.connect(mqttClientId, availabilityTopic.c_str(), 0, true, "offline");
  }
  
  if (success) {
    Serial.println("connected");
    
    // Publish online status
    mqttClient.publish(availabilityTopic.c_str(), "online", true);
    
    // Subscribe to command topic
    mqttClient.subscribe(commandTopic.c_str());
    
    // Send discovery information
    sendHomeAssistantDiscovery();
    
    // Publish state
    publishState();
    
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqttClient.state());
    return false;
  }
}

void setMqttServer(String server) {
  mqttEnabled = server.length() > 0;
  
  if (mqttEnabled) {
    // Здесь мы убираем проверку WiFi.status() == WL_CONNECTED
    reconnectMqtt();
  }
}

void createNumberEntity(JsonDocument& deviceDoc, String name, String field, float min, float max, float step) {
  String deviceName = getDeviceName();
  deviceName.replace(" ", "_");
  
  DynamicJsonDocument entityDoc(512);
  entityDoc["name"] = getDeviceName() + " " + name;
  entityDoc["unique_id"] = deviceName + "_" + field;
  entityDoc["state_topic"] = stateTopic;
  entityDoc["value_template"] = "{{ value_json." + field + " }}";
  entityDoc["command_topic"] = commandTopic;
  entityDoc["command_template"] = "{\"" + field + "\":{{ value }}}";
  entityDoc["min"] = min;
  entityDoc["max"] = max;
  entityDoc["step"] = step;
  entityDoc["availability_topic"] = availabilityTopic;
  entityDoc["device"] = deviceDoc;
  
  String entityJson;
  serializeJson(entityDoc, entityJson);
  
  String entityTopic = String(MQTT_DISCOVERY_PREFIX) + "/number/" + deviceName + "_" + field + "/config";
  mqttClient.publish(entityTopic.c_str(), entityJson.c_str(), true);
}

void sendHomeAssistantDiscovery() {
  if (!mqttClient.connected()) {
    return;
  }
  
  String deviceName = getDeviceName();
  deviceName.replace(" ", "_");
  
  Serial.println("Sending HomeAssistant discovery information...");
  
  DynamicJsonDocument deviceDoc(256);
  deviceDoc["identifiers"] = deviceName;
  deviceDoc["name"] = getDeviceName();
  deviceDoc["model"] = "LightTrack";
  deviceDoc["manufacturer"] = "DIY Yari";
  deviceDoc["sw_version"] = "1.0";
  
  // Main light entity
  DynamicJsonDocument lightDoc(1024);
  lightDoc["name"] = getDeviceName() + " Light";
  lightDoc["unique_id"] = deviceName + "_light";
  lightDoc["state_topic"] = stateTopic;
  lightDoc["command_topic"] = commandTopic;
  lightDoc["schema"] = "json";
  lightDoc["brightness"] = true;
  lightDoc["rgb"] = true;
  lightDoc["availability_topic"] = availabilityTopic;
  lightDoc["device"] = deviceDoc;
  
  String lightJson;
  serializeJson(lightDoc, lightJson);
  
  String discoveryTopic = String(MQTT_DISCOVERY_PREFIX) + "/light/" + deviceName + "/config";
  mqttClient.publish(discoveryTopic.c_str(), lightJson.c_str(), true);
  
  // Background mode switch
  DynamicJsonDocument backgroundDoc(512);
  backgroundDoc["name"] = getDeviceName() + " Background Mode";
  backgroundDoc["unique_id"] = deviceName + "_background";
  backgroundDoc["state_topic"] = stateTopic;
  backgroundDoc["value_template"] = "{{ value_json.background_mode }}";
  backgroundDoc["command_topic"] = commandTopic;
  backgroundDoc["payload_on"] = "{\"background_mode\":\"ON\"}";
  backgroundDoc["payload_off"] = "{\"background_mode\":\"OFF\"}";
  backgroundDoc["availability_topic"] = availabilityTopic;
  backgroundDoc["device"] = deviceDoc;
  
  String backgroundJson;
  serializeJson(backgroundDoc, backgroundJson);
  
  String backgroundTopic = String(MQTT_DISCOVERY_PREFIX) + "/switch/" + deviceName + "_background/config";
  mqttClient.publish(backgroundTopic.c_str(), backgroundJson.c_str(), true);
  
  // Create number entities for other parameters
  createNumberEntity(deviceDoc, "Moving Length", "moving_length", 1, 300, 1);
  createNumberEntity(deviceDoc, "Center Shift", "center_shift", -100, 100, 1);
  createNumberEntity(deviceDoc, "Additional LEDs", "additional_leds", 0, 100, 1);
  createNumberEntity(deviceDoc, "LED Off Delay", "led_off_delay", 1, 60, 1);
  createNumberEntity(deviceDoc, "Update Interval", "update_interval", 5, 100, 1);
  createNumberEntity(deviceDoc, "Moving Intensity", "moving_intensity", 0, 1, 0.01);
  createNumberEntity(deviceDoc, "Background Intensity", "stationary_intensity", 0, 0.07, 0.001);
}

void publishState() {
  if (!mqttClient.connected()) {
    return;
  }
  
  CRGB baseColor = getBaseColor();
  
  DynamicJsonDocument stateDoc(512);
  stateDoc["state"] = isLightOn() ? "ON" : "OFF";
  stateDoc["brightness"] = int(getMovingIntensity() * 255);
  
  JsonArray rgb = stateDoc.createNestedArray("rgb");
  rgb.add(baseColor.r);
  rgb.add(baseColor.g);
  rgb.add(baseColor.b);
  
  stateDoc["background_mode"] = isBackgroundModeActive() ? "ON" : "OFF";
  stateDoc["moving_length"] = getMovingLength();
  stateDoc["center_shift"] = getCenterShift();
  stateDoc["additional_leds"] = getAdditionalLEDs();
  stateDoc["led_off_delay"] = getLedOffDelay();
  stateDoc["update_interval"] = getUpdateInterval();
  stateDoc["moving_intensity"] = getMovingIntensity();
  stateDoc["stationary_intensity"] = getStationaryIntensity();
  
  String stateJson;
  serializeJson(stateDoc, stateJson);
  
  mqttClient.publish(stateTopic.c_str(), stateJson.c_str(), true);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Create buffer for payload
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(message);
  
  // Process command
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  bool stateChanged = false;
  
  // Process state
  if (doc.containsKey("state")) {
    String state = doc["state"].as<String>();
    if (state == "ON") {
      setLightOn(true);
      stateChanged = true;
    } else if (state == "OFF") {
      setLightOn(false);
      stateChanged = true;
    }
  }
  
  // Process RGB
  if (doc.containsKey("rgb")) {
    JsonArray rgb = doc["rgb"].as<JsonArray>();
    if (rgb.size() == 3) {
      CRGB color = CRGB(rgb[0], rgb[1], rgb[2]);
      setBaseColor(color);
      stateChanged = true;
    }
  }
  
  // Process brightness
  if (doc.containsKey("brightness")) {
    int brightness = doc["brightness"];
    setMovingIntensity(brightness / 255.0);
    stateChanged = true;
  }
  
  // Process background mode
  if (doc.containsKey("background_mode")) {
    String mode = doc["background_mode"].as<String>();
    setBackgroundModeActive(mode == "ON");
    stateChanged = true;
  }
  
  // Process moving_length
  if (doc.containsKey("moving_length")) {
    int length = doc["moving_length"];
    setMovingLength(length);
    stateChanged = true;
  }
  
  // Process center_shift
  if (doc.containsKey("center_shift")) {
    int shift = doc["center_shift"];
    setCenterShift(shift);
    stateChanged = true;
  }
  
  // Process additional_leds
  if (doc.containsKey("additional_leds")) {
    int leds = doc["additional_leds"];
    setAdditionalLEDs(leds);
    stateChanged = true;
  }
  
  // Process led_off_delay
  if (doc.containsKey("led_off_delay")) {
    int delay = doc["led_off_delay"];
    setLedOffDelay(delay);
    stateChanged = true;
  }
  
  // Process update_interval
  if (doc.containsKey("update_interval")) {
    int interval = doc["update_interval"];
    setUpdateInterval(interval);
    stateChanged = true;
  }
  
  // Process moving_intensity
  if (doc.containsKey("moving_intensity")) {
    float intensity = doc["moving_intensity"];
    setMovingIntensity(intensity);
    stateChanged = true;
  }
  
  // Process stationary_intensity
  if (doc.containsKey("stationary_intensity")) {
    float intensity = doc["stationary_intensity"];
    setStationaryIntensity(intensity);
    stateChanged = true;
  }
  
  // Publish updated state if anything changed
  if (stateChanged) {
    publishState();
  }
}