#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

#define WIFI_SSID "AttyGadon 2.5"
#define WIFI_PASSWORD "Kroi101!"

// NTP settings
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 28800     // GMT+8 (Philippines)
#define DAYLIGHT_OFFSET_SEC 0

// Google Script ID - you'll get this when you deploy your Google Apps Script
#define GOOGLE_SCRIPT_ID "AKfycbxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

// Coin detector pins
#define coinPin1 23
#define coinPin2 19
#define coinPin3 18

// LCD PINS, SDA = 21 , SCL = 22
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Coin detector variables
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
int dailyTotal = 0;

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
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());

    // Setup coin detector pins
    pinMode(coinPin1, INPUT_PULLUP);
    pinMode(coinPin2, INPUT_PULLUP);
    pinMode(coinPin3, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(coinPin1), ISR_Coin1, FALLING);
    attachInterrupt(digitalPinToInterrupt(coinPin2), ISR_Coin2, FALLING);
    attachInterrupt(digitalPinToInterrupt(coinPin3), ISR_Coin3, FALLING);

    // Initialize LCD
    lcd.init();
    lcd.backlight();
    lcd.setCursor(2, 0);
    lcd.print("SUNBOX");
    lcd.setCursor(1, 1);
    lcd.print("INITIALIZING...");

    // Initialize NTP
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER, "time.google.com");
    
    // Wait for time sync
    time_t now = 0;
    int timeout = 20;
    bool timeOK = false;
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WAITING FOR TIME");
    
    while (timeout > 0 && !timeOK) {
        time(&now);
        if (now > 1609459200) { // After January 1, 2021
            timeOK = true;
            break;
        }
        lcd.setCursor(0, 1);
        lcd.print("SYNC WAIT: ");
        lcd.print(timeout);
        lcd.print("s  ");
        delay(1000);
        timeout--;
    }
    
    // Display time status
    struct tm* timeinfo = localtime(&now);
    char time_str[30];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    if (timeOK) {
        lcd.print("TIME SYNCED OK!");
        Serial.print("NTP time synchronized: ");
        Serial.println(time_str);
    } else {
        lcd.print("TIME SYNC FAIL!");
        Serial.println("NTP time sync failed!");
    }
    lcd.setCursor(0, 1);
    lcd.print(time_str);
    delay(2000);
    
    // Ready to use
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("SUNBOX");
    lcd.setCursor(1, 1);
    lcd.print("INSERT A COINS");
}

void loop() {
    // Process coin insertions
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

    // Periodic time sync (every hour)
    static unsigned long lastTimeSync = 0;
    if (millis() - lastTimeSync > 3600000 || lastTimeSync == 0) { // 1 hour
        lastTimeSync = millis();
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
        
        time_t now;
        time(&now);
        struct tm* timeinfo = localtime(&now);
        char time_str[30];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
        Serial.print("Time synchronized: ");
        Serial.println(time_str);
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
        dailyTotal += creditToAdd;
        
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

        // Send data to Google Sheets
        sendCoinDataToGoogleSheets(creditToAdd, slot);
    }
}

void sendCoinDataToGoogleSheets(int creditAmount, int slotNumber) {
    // Check if WiFi is still connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return;
    }

    // Get current time
    time_t now;
    time(&now);
    
    if (now < 1609459200) { // January 1, 2021 as a sanity check
        Serial.println("Time not synchronized");
        return;
    }
    
    struct tm* timeinfo = localtime(&now);
    char datetime_str[30];
    strftime(datetime_str, sizeof(datetime_str), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    // Format date as YYYY-MM-DD for Google Sheets tab
    char date_str[11];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", timeinfo);
    
    // Show sending status on LCD
    lcd.setCursor(0, 1);
    lcd.print("SENDING DATA...  ");
    
    // Prepare data for Google Sheets
    String urlFinal = "https://script.google.com/macros/s/" + String(GOOGLE_SCRIPT_ID) + "/exec";
    String payload = "date=" + String(date_str) + 
                     "&time=" + String(datetime_str) + 
                     "&coin=" + String(creditAmount) + 
                     "&slot=" + String(slotNumber) +
                     "&total=" + String(dailyTotal);
                     
    Serial.println("Sending data to Google Sheets...");
    Serial.println(payload);
    
    HTTPClient http;
    http.begin(urlFinal);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    int httpCode = http.POST(payload);
    String response = http.getString();
    
    if (httpCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpCode);
        Serial.println(response);
        
        lcd.setCursor(0, 1);
        lcd.print("DATA SAVED OK!  ");
    } else {
        Serial.print("Error sending data. Code: ");
        Serial.println(httpCode);
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("DATA ERROR!");
        lcd.setCursor(0, 1);
        lcd.print("CODE: ");
        lcd.print(httpCode);
    }
    
    http.end();
    
    // Wait briefly to show sending message
    delay(500);
    
    // Return to showing credits
    lcd.setCursor(0, 1);
    lcd.print("CREDITS: ");
    lcd.print(coinCount);
    lcd.print("      ");
}
