#include "storage.h"
#include "config.h"
#include <EEPROM.h>
#include <Preferences.h>

Preferences preferences;

// Display Parameters
static int updateInterval = DEFAULT_UPDATE_INTERVAL;
static float movingIntensity = DEFAULT_MOVING_INTENSITY;
static float stationaryIntensity = DEFAULT_STATIONARY_INTENSITY;
static int movingLength = DEFAULT_MOVING_LENGTH;
static int centerShift = DEFAULT_CENTER_SHIFT;
static int additionalLEDs = DEFAULT_ADDITIONAL_LEDS;
static CRGB baseColor = DEFAULT_BASE_COLOR;
static float speedMultiplier = DEFAULT_SPEED_MULTIPLIER;
static int ledOffDelay = DEFAULT_LED_OFF_DELAY;

// Time and Schedule Parameters
static int startHour = DEFAULT_START_HOUR;
static int startMinute = DEFAULT_START_MINUTE;
static int endHour = DEFAULT_END_HOUR;
static int endMinute = DEFAULT_END_MINUTE;
static bool lightOn = true;

// Background Light Mode
static bool backgroundModeActive = false;

// WiFi settings
static String wifi_ssid = "";
static String wifi_password = "";

// MQTT settings
static String mqtt_server = "";
static int mqtt_port = 1883;
static String mqtt_user = "";
static String mqtt_password = "";

void initStorage() {
  EEPROM.begin(EEPROM_SIZE);
  preferences.begin("lighttrack", false);
  loadSettings();
  
  // Load WiFi settings from preferences
  wifi_ssid = preferences.getString("wifi_ssid", "");
  wifi_password = preferences.getString("wifi_password", "");
  
  // Load MQTT settings from preferences
  mqtt_server = preferences.getString("mqtt_server", "");
  mqtt_port = preferences.getInt("mqtt_port", 1883);
  mqtt_user = preferences.getString("mqtt_user", "");
  mqtt_password = preferences.getString("mqtt_password", "");
}

// Load settings from EEPROM
void loadSettings() {
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
}

// Save settings to EEPROM
void saveSettings() {
  EEPROM.begin(EEPROM_SIZE);
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

// Getters
int getUpdateInterval() { return updateInterval; }
int getLedOffDelay() { return ledOffDelay; }
float getMovingIntensity() { return movingIntensity; }
float getStationaryIntensity() { return stationaryIntensity; }
int getMovingLength() { return movingLength; }
int getCenterShift() { return centerShift; }
int getAdditionalLEDs() { return additionalLEDs; }
CRGB getBaseColor() { return baseColor; }
float getSpeedMultiplier() { return speedMultiplier; }
int getStartHour() { return startHour; }
int getStartMinute() { return startMinute; }
int getEndHour() { return endHour; }
int getEndMinute() { return endMinute; }
bool isLightOn() { return lightOn; }
bool isBackgroundModeActive() { return backgroundModeActive; }

// Setters
void setUpdateInterval(int value) { 
  updateInterval = value; 
  saveSettings();
}

void setLedOffDelay(int value) { 
  ledOffDelay = value; 
  saveSettings();
}

void setMovingIntensity(float value) { 
  movingIntensity = value; 
  saveSettings();
}

void setStationaryIntensity(float value) {
  if(value < 0.01) value = 0.0;
  // Limit the maximum intensity to 0.07
  value = constrain(value, 0.0, 0.07);
  stationaryIntensity = value;
  saveSettings();
}

void setMovingLength(int value) { 
  movingLength = value; 
  saveSettings();
}

void setCenterShift(int value) { 
  centerShift = value; 
  saveSettings();
}

void setAdditionalLEDs(int value) { 
  additionalLEDs = value; 
  saveSettings();
}

void setBaseColor(CRGB color) { 
  baseColor = color; 
  saveSettings();
}

void setSpeedMultiplier(float value) { 
  speedMultiplier = value; 
  saveSettings();
}

void setStartHour(int value) { 
  startHour = value; 
  saveSettings();
}

void setStartMinute(int value) { 
  startMinute = value; 
  saveSettings();
}

void setEndHour(int value) { 
  endHour = value; 
  saveSettings();
}

void setEndMinute(int value) { 
  endMinute = value; 
  saveSettings();
}

void setLightOn(bool value) { 
  lightOn = value; 
}

void setBackgroundModeActive(bool value) {
  backgroundModeActive = value;
}

void toggleBackgroundMode() {
  backgroundModeActive = !backgroundModeActive;
}

// WiFi settings
void saveWiFiSettings(const char* ssid, const char* password) {
  wifi_ssid = String(ssid);
  wifi_password = String(password);
  
  // Save to preferences
  preferences.putString("wifi_ssid", wifi_ssid);
  preferences.putString("wifi_password", wifi_password);
}

String getWiFiSSID() {
  return wifi_ssid;
}

String getWiFiPassword() {
  return wifi_password;
}

bool hasWiFiSettings() {
  return wifi_ssid.length() > 0;
}

// MQTT settings
void saveMqttSettings(const char* server, int port, const char* user, const char* password) {
  mqtt_server = String(server);
  mqtt_port = port;
  mqtt_user = String(user);
  mqtt_password = String(password);
  
  // Save to preferences
  preferences.putString("mqtt_server", mqtt_server);
  preferences.putInt("mqtt_port", mqtt_port);
  preferences.putString("mqtt_user", mqtt_user);
  preferences.putString("mqtt_password", mqtt_password);
}

String getMqttServer() {
  return mqtt_server;
}

int getMqttPort() {
  return mqtt_port;
}

String getMqttUser() {
  return mqtt_user;
}

String getMqttPassword() {
  return mqtt_password;
}

bool hasMqttSettings() {
  return mqtt_server.length() > 0;
}