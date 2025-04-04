#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>

// Initialize the sensor
void initSensor();

// Get current sensor distance
unsigned int getSensorDistance();

// Sensor task function
void sensorTask(void * parameter);

// Read raw sensor data
unsigned int readSensorData();

#endif // SENSOR_MANAGER_H