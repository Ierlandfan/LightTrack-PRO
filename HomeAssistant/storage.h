#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <FastLED.h>

// Initialize EEPROM storage
void initStorage();

// Load settings from EEPROM
void loadSettings();

// Save settings to EEPROM
void saveSettings();

// Getters for settings
int getUpdateInterval();
int getLedOffDelay();
float getMovingIntensity();
float getStationaryIntensity();
int getMovingLength();
int getCenterShift();
int getAdditionalLEDs();
CRGB getBaseColor();
float getSpeedMultiplier();
int getStartHour();
int getStartMinute();
int getEndHour();
int getEndMinute();
bool isLightOn();
bool isBackgroundModeActive();

// Setters for settings
void setUpdateInterval(int value);
void setLedOffDelay(int value);
void setMovingIntensity(float value);
void setStationaryIntensity(float value);
void setMovingLength(int value);
void setCenterShift(int value);
void setAdditionalLEDs(int value);
void setBaseColor(CRGB color);
void setSpeedMultiplier(float value);
void setStartHour(int value);
void setStartMinute(int value);
void setEndHour(int value);
void setEndMinute(int value);
void setLightOn(bool value);
void setBackgroundModeActive(bool value);
void toggleBackgroundMode();

// WiFi настройки
void saveWiFiSettings(const char* ssid, const char* password);
String getWiFiSSID();
String getWiFiPassword();
bool hasWiFiSettings();

// MQTT настройки
void saveMqttSettings(const char* server, int port, const char* user, const char* password);
String getMqttServer();
int getMqttPort();
String getMqttUser();
String getMqttPassword();
bool hasMqttSettings();

#endif // STORAGE_H