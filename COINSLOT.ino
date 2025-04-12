//=========================================================================================================================================================================
//                                                                              PISOTECH DIY 
//=========================================================================================================================================================================
#include "Arduino.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define coinPin 23
volatile int pulseCount = 0;
volatile bool pulseDetected = false;
unsigned long lastPulseTime = 0;
const unsigned long pulseTimeout = 300; // ms

int coinCount = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Interrupt Service Routine (ISR)
void IRAM_ATTR ITR() {
  if (digitalRead(coinPin) == LOW) {
    pulseCount++;
    lastPulseTime = millis();
    pulseDetected = true;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(coinPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(coinPin), ITR, FALLING);

  lcd.init();
  lcd.backlight();  
  lcd.setCursor(2, 0);
  lcd.print("PISOTECH DIY");
  lcd.setCursor(1, 1);
  lcd.print("INSERT A COINS");
}

void loop() {
  if (pulseDetected) {
    unsigned long now = millis();

    // Wait until pulse "burst" finishes
    if ((now - lastPulseTime) > pulseTimeout && pulseCount > 0) {
      int creditToAdd = 0;

      // Match pulse count to credits
      if (pulseCount == 3) {
        creditToAdd = 5;
      } else if (pulseCount == 6) {
        creditToAdd = 10;
      } else if (pulseCount == 1) {
        creditToAdd = 1;
      } else {
        Serial.print("Unknown pulse count: ");
        Serial.println(pulseCount);
      }

      if (creditToAdd > 0) {
        coinCount += creditToAdd;
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("PISOTECH DIY");
        lcd.setCursor(0, 1);
        lcd.print("CREDITS: ");
        lcd.print(coinCount);
      }

      // Reset pulse counter
      pulseCount = 0;
      pulseDetected = false;
    }
  }
}
