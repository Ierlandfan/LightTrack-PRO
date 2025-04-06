#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WebServer.h>

// Сервер для использования в других модулях
extern WebServer server;

// Initialize web server
void initWebServer();

// Web server task function
void webServerTask(void * parameter);

// Smart home controls
bool isSmartHomeOverride();
void clearSmartHomeOverride();

// MQTT settings handlers
void handleMqttSettings();
void handleMqttSave();

#endif // WEB_SERVER_H