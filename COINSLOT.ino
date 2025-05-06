#include "Arduino.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define coinPin1 23
#define coinPin2 19
#define coinPin3 18

// LCD PINS, SDA = 21 , SCL = 22, POS NEG

volatile int pulseCount1 = 0;
volatile int pulseCount2 = 0;
volatile int pulseCount3 = 0;

volatile bool pulseDetected1 = false;
volatile bool pulseDetected2 = false;
volatile bool pulseDetected3 = false;

unsigned long lastPulseTime1 = 0;
unsigned long lastPulseTime2 = 0;
unsigned long lastPulseTime3 = 0;

const unsigned long pulseTimeout = 300; // ms

int coinCount = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ISR for coin slot 1
void IRAM_ATTR ISR_Coin1() {
  if (digitalRead(coinPin1) == LOW) {
    pulseCount1++;
    lastPulseTime1 = millis();
    pulseDetected1 = true;
  }
}

// ISR for coin slot 2
void IRAM_ATTR ISR_Coin2() {
  if (digitalRead(coinPin2) == LOW) {
    pulseCount2++;
    lastPulseTime2 = millis();
    pulseDetected2 = true;
  }
}

// ISR for coin slot 3
void IRAM_ATTR ISR_Coin3() {
  if (digitalRead(coinPin3) == LOW) {
    pulseCount3++;
    lastPulseTime3 = millis();
    pulseDetected3 = true;
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(coinPin1, INPUT_PULLUP);
  pinMode(coinPin2, INPUT_PULLUP);
  pinMode(coinPin3, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(coinPin1), ISR_Coin1, FALLING);
  attachInterrupt(digitalPinToInterrupt(coinPin2), ISR_Coin2, FALLING);
  attachInterrupt(digitalPinToInterrupt(coinPin3), ISR_Coin3, FALLING);

  lcd.init();
  lcd.backlight();  
  lcd.setCursor(2, 0);
  lcd.print("SUNBOX");
  lcd.setCursor(1, 1);
  lcd.print("INSERT A COINS");

   delay(1000);
}

void loop() {
  unsigned long now = millis();

  // Process coin slot 1
  if (pulseDetected1 && (now - lastPulseTime1) > pulseTimeout && pulseCount1 > 0) {
    handlePulse(pulseCount1, 1);
    pulseCount1 = 0;
    pulseDetected1 = false;
  }

  // Process coin slot 2
  if (pulseDetected2 && (now - lastPulseTime2) > pulseTimeout && pulseCount2 > 0) {
    handlePulse(pulseCount2, 2);
    pulseCount2 = 0;
    pulseDetected2 = false;
  }

  // Process coin slot 3
  if (pulseDetected3 && (now - lastPulseTime3) > pulseTimeout && pulseCount3 > 0) {
    handlePulse(pulseCount3, 3);
    pulseCount3 = 0;
    pulseDetected3 = false;
  }
}

void handlePulse(int count, int slot) {
  int creditToAdd = 0;

  if (count == 3) {
    creditToAdd = 5;
  } else if (count == 6) {
    creditToAdd = 10;
  } else {
    Serial.print("Unknown pulse count from Slot ");
    Serial.print(slot);
    Serial.print(": ");
    Serial.println(count);
  }

  if (creditToAdd > 0) {
    coinCount += creditToAdd;
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("SUNBOX");
    lcd.setCursor(0, 1);
    lcd.print("CREDITS: ");
    lcd.print(coinCount);

    Serial.print("Slot ");
    Serial.print(slot);
    Serial.print(" added ");
    Serial.print(creditToAdd);
    Serial.println(" credits.");
  }
}

