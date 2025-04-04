#include "led_controller.h"
#include "config.h"
#include "storage.h"
#include "sensor_manager.h"

// Define LED array
CRGB leds[NUM_LEDS];

void initLEDController() {
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();
}

void ledTask(void * parameter) {
  static unsigned int lastSensor = getSensorDistance();
  static int lastMovementDirection = 0;
  static unsigned long lastMovementTime = millis();

  for (;;) {
    unsigned long currentMillis = millis();
    unsigned int currentDistance = getSensorDistance();
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
    bool drawMovingPart = (currentMillis - lastMovementTime <= getLedOffDelay() * 1000);

    // If light is off, clear the strip
    if (!isLightOn()) {
      fill_solid(leds, NUM_LEDS, CRGB::Black);
    }
    else {
      // If background mode is active, display the background glow regardless of motion
      if (isBackgroundModeActive()) {
        CRGB baseColor = getBaseColor();
        float stationaryIntensity = getStationaryIntensity();
        
        fill_solid(leds, NUM_LEDS, CRGB(
          (uint8_t)(baseColor.r * stationaryIntensity),
          (uint8_t)(baseColor.g * stationaryIntensity),
          (uint8_t)(baseColor.b * stationaryIntensity)
        ));
        
        // Overlay the moving light beam if motion is detected
        if (drawMovingPart) {
          float movingIntensity = getMovingIntensity();
          int movingLength = getMovingLength();
          int centerShift = getCenterShift();
          int additionalLEDs = getAdditionalLEDs();
          
          float prop = constrain((float)(currentDistance - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE), 0.0, 1.0);
          int ledPosition = (diff < 0) ? ceil(prop * (NUM_LEDS - movingLength)) : round(prop * (NUM_LEDS - movingLength));
          int centerLED = ledPosition + centerShift;
          centerLED = constrain(centerLED, 0, NUM_LEDS - 1);
          int halfLength = movingLength / 2;
          
          if (movingLength <= 1) {
            leds[centerLED] = CRGB(
              (uint8_t)(baseColor.r * movingIntensity),
              (uint8_t)(baseColor.g * movingIntensity),
              (uint8_t)(baseColor.b * movingIntensity)
            );
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
              
              leds[idx] = CRGB(
                (uint8_t)(baseColor.r * movingIntensity * factor),
                (uint8_t)(baseColor.g * movingIntensity * factor),
                (uint8_t)(baseColor.b * movingIntensity * factor)
              );
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
              
              leds[idx] = CRGB(
                (uint8_t)(baseColor.r * movingIntensity * factor),
                (uint8_t)(baseColor.g * movingIntensity * factor),
                (uint8_t)(baseColor.b * movingIntensity * factor)
              );
            }
          }
        }
      }
      else { 
        // If background mode is off, display the moving light beam on a black background
        if (drawMovingPart) {
          fill_solid(leds, NUM_LEDS, CRGB::Black);
          
          CRGB baseColor = getBaseColor();
          float movingIntensity = getMovingIntensity();
          int movingLength = getMovingLength();
          int centerShift = getCenterShift();
          int additionalLEDs = getAdditionalLEDs();
          
          float prop = constrain((float)(currentDistance - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE), 0.0, 1.0);
          int ledPosition = (diff < 0) ? ceil(prop * (NUM_LEDS - movingLength)) : round(prop * (NUM_LEDS - movingLength));
          int centerLED = ledPosition + centerShift;
          centerLED = constrain(centerLED, 0, NUM_LEDS - 1);
          int halfLength = movingLength / 2;
          
          if (movingLength <= 1) {
            leds[centerLED] = CRGB(
              (uint8_t)(baseColor.r * movingIntensity),
              (uint8_t)(baseColor.g * movingIntensity),
              (uint8_t)(baseColor.b * movingIntensity)
            );
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
              
              leds[idx] = CRGB(
                (uint8_t)(baseColor.r * movingIntensity * factor),
                (uint8_t)(baseColor.g * movingIntensity * factor),
                (uint8_t)(baseColor.b * movingIntensity * factor)
              );
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
              
              leds[idx] = CRGB(
                (uint8_t)(baseColor.r * movingIntensity * factor),
                (uint8_t)(baseColor.g * movingIntensity * factor),
                (uint8_t)(baseColor.b * movingIntensity * factor)
              );
            }
          }
        }
        else {
          fill_solid(leds, NUM_LEDS, CRGB::Black);
        }
      }
    }

    FastLED.show();
    vTaskDelay(pdMS_TO_TICKS(getUpdateInterval()));
  }
}