#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <time.h>
#include <ArduinoJson.h>  // For JSON formatting

#define WIFI_SSID "PAPARAPAKYAW"
#define WIFI_PASSWORD "123456789"

// NTP settings
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 28800     // GMT+8 (Philippines)
#define DAYLIGHT_OFFSET_SEC 0

// Supabase API settings
#define SUPABASE_URL "https://mtcqikpjonxvarfhzquw.supabase.co"
#define SUPABASE_API_KEY "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im10Y3Fpa3Bqb254dmFyZmh6cXV3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDY5MzU2NzksImV4cCI6MjA2MjUxMTY3OX0.6rL1nTePcPQdYrdCWLh1x8eBCuHyZDwMIcm_f0YWGcs"
#define SUPABASE_TABLE "coin_logs"

// Coin detector pins
#define coinPin1 23
#define coinPin2 19
#define coinPin3 18
#define coinPin4 5
#define coinPin5 17
#define coinPin6 16

// Coin detector variables
volatile int pulseCount1 = 0;
volatile int pulseCount2 = 0;
volatile int pulseCount3 = 0;
volatile int pulseCount4 = 0;
volatile int pulseCount5 = 0;
volatile int pulseCount6 = 0;

volatile bool pulseDetected1 = false;
volatile bool pulseDetected2 = false;
volatile bool pulseDetected3 = false;
volatile bool pulseDetected4 = false;
volatile bool pulseDetected5 = false;
volatile bool pulseDetected6 = false;

unsigned long lastPulseTime1 = 0;
unsigned long lastPulseTime2 = 0;
unsigned long lastPulseTime3 = 0;
unsigned long lastPulseTime4 = 0;
unsigned long lastPulseTime5 = 0;
unsigned long lastPulseTime6 = 0;

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

// ISR for coin slot 4
void IRAM_ATTR ISR_Coin4() {
    if (digitalRead(coinPin4) == LOW) {
        pulseCount4++;
        lastPulseTime4 = millis();
        pulseDetected4 = true;
    }
}

// ISR for coin slot 5
void IRAM_ATTR ISR_Coin5() {
    if (digitalRead(coinPin5) == LOW) {
        pulseCount5++;
        lastPulseTime5 = millis();
        pulseDetected5 = true;
    }
}

// ISR for coin slot 6
void IRAM_ATTR ISR_Coin6() {
    if (digitalRead(coinPin6) == LOW) {
        pulseCount6++;
        lastPulseTime6 = millis();
        pulseDetected6 = true;
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
    pinMode(coinPin4, INPUT_PULLUP);
    pinMode(coinPin5, INPUT_PULLUP);
    pinMode(coinPin6, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(coinPin1), ISR_Coin1, FALLING);
    attachInterrupt(digitalPinToInterrupt(coinPin2), ISR_Coin2, FALLING);
    attachInterrupt(digitalPinToInterrupt(coinPin3), ISR_Coin3, FALLING);
    attachInterrupt(digitalPinToInterrupt(coinPin4), ISR_Coin4, FALLING);
    attachInterrupt(digitalPinToInterrupt(coinPin5), ISR_Coin5, FALLING);
    attachInterrupt(digitalPinToInterrupt(coinPin6), ISR_Coin6, FALLING);

    Serial.println("===============================");
    Serial.println("          SUNBOX");
    Serial.println("        INITIALIZING...");
    Serial.println("===============================");

    // Initialize NTP
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER, "time.google.com");
    
    // Wait for time sync
    time_t now = 0;
    int timeout = 20;
    bool timeOK = false;
    
    Serial.println("WAITING FOR TIME SYNC");
    
    while (timeout > 0 && !timeOK) {
        time(&now);
        if (now > 1609459200) { // After January 1, 2021
            timeOK = true;
            break;
        }
        Serial.print("SYNC WAIT: ");
        Serial.print(timeout);
        Serial.println("s");
        delay(1000);
        timeout--;
    }
    
    // Display time status
    struct tm* timeinfo = localtime(&now);
    char time_str[30];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    if (timeOK) {
        Serial.println("TIME SYNCED OK!");
        Serial.print("NTP time synchronized: ");
        Serial.println(time_str);
    } else {
        Serial.println("TIME SYNC FAIL!");
        Serial.println("NTP time sync failed!");
    }
    Serial.println(time_str);
    
    // Ready to use
    Serial.println("===============================");
    Serial.println("          SUNBOX");
    Serial.println("      INSERT A COINS");
    Serial.println("===============================");
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
    
    // Process coin slot 4
    if (pulseDetected4 && (now - lastPulseTime4) > pulseTimeout && pulseCount4 > 0) {
        handlePulse(pulseCount4, 4);
        pulseCount4 = 0;
        pulseDetected4 = false;
    }
    
    // Process coin slot 5
    if (pulseDetected5 && (now - lastPulseTime5) > pulseTimeout && pulseCount5 > 0) {
        handlePulse(pulseCount5, 5);
        pulseCount5 = 0;
        pulseDetected5 = false;
    }
    
    // Process coin slot 6
    if (pulseDetected6 && (now - lastPulseTime6) > pulseTimeout && pulseCount6 > 0) {
        handlePulse(pulseCount6, 6);
        pulseCount6 = 0;
        pulseDetected6 = false;
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
        
        // Check if it's midnight and reset the daily total if needed
        checkDailyReset();
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
        
        Serial.println("===============================");
        Serial.println("          SUNBOX");
        Serial.print("CREDITS: ");
        Serial.println(coinCount);
        Serial.println("===============================");

        Serial.print("Slot ");
        Serial.print(slot);
        Serial.print(" added ");
        Serial.print(creditToAdd);
        Serial.println(" credits.");

        // Send data to Supabase
        sendCoinDataToSupabase(creditToAdd, slot);
    }
}

void sendCoinDataToSupabase(int creditAmount, int slotNumber) {
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
    
    Serial.println("SENDING DATA...");
    
    // Create JSON payload for Supabase
    DynamicJsonDocument doc(256);
    doc["timestamp"] = String(datetime_str);  // Store timestamp
    doc["coin_value"] = creditAmount;         // Store coin value
    doc["daily_total"] = dailyTotal;          // Store dynamically updating daily total
    doc["slot_number"] = slotNumber;          // Additional info - which slot was used
    
    String jsonPayload;
    serializeJson(doc, jsonPayload);
                     
    Serial.println("Sending data to Supabase...");
    Serial.println(jsonPayload);
    
    HTTPClient http;
    // Create URL for Supabase REST API
    String urlFinal = String(SUPABASE_URL) + "/rest/v1/" + String(SUPABASE_TABLE);
    
    http.begin(urlFinal);
    
    // Set required Supabase headers
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_API_KEY));
    http.addHeader("Prefer", "return=minimal");  // Don't return data to save bandwidth
    
    int httpCode = http.POST(jsonPayload);
    String response = http.getString();
    
    if (httpCode == 201) {  // 201 is the success code for POST creation
        Serial.print("HTTP Success! Response code: ");
        Serial.println(httpCode);
        Serial.println(response);
        Serial.println("DATA SAVED OK!");
    } else {
        Serial.print("Error sending data. Code: ");
        Serial.println(httpCode);
        Serial.println(response);
        Serial.println("DATA ERROR!");
        Serial.print("CODE: ");
        Serial.println(httpCode);
    }
    
    http.end();
    
    // Wait briefly to show sending message
    delay(500);
    
    // Return to showing credits
    Serial.println("===============================");
    Serial.print("CREDITS: ");
    Serial.println(coinCount);
    Serial.println("===============================");
}

void checkDailyReset() {
    static int lastDate = -1;
    
    time_t now;
    time(&now);
    
    struct tm* timeinfo = localtime(&now);
    int currentDate = timeinfo->tm_mday;
    
    // If this is the first run or date has changed
    if (lastDate != currentDate) {
        if (lastDate != -1) {  // Not the first run
            Serial.println("Date changed! Resetting daily total");
            
            // Reset daily total
            dailyTotal = 0;
            
            Serial.println("===============================");
            Serial.println("DAILY RESET");
            Serial.println("NEW DAY STARTED");
            Serial.println("===============================");
            delay(2000);
            
            // Return to standard view
            Serial.println("===============================");
            Serial.println("          SUNBOX");
            Serial.println("      INSERT A COINS");
            Serial.println("===============================");
        }
        
        // Update lastDate
        lastDate = currentDate;
    }
}
