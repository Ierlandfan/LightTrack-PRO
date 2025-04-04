#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <FastLED.h>

// Initialize LED controller
void initLEDController();

// LED task function
void ledTask(void * parameter);

#endif // LED_CONTROLLER_H