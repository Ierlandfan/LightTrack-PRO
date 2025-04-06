#include "sensor_manager.h"
#include "config.h"

// Global sensor distance variable
volatile unsigned int g_sensorDistance = DEFAULT_DISTANCE;

void initSensor() {
  Serial1.begin(SENSOR_BAUD_RATE, SERIAL_8N1, 20, 21);
}

unsigned int getSensorDistance() {
  return g_sensorDistance;
}

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

void sensorTask(void * parameter) {
  for (;;) {
    unsigned int newDistance = readSensorData();
    g_sensorDistance = newDistance;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}