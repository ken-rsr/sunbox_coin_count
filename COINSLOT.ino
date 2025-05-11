/**
 * ABOUT:
 *
 * The example to create the Firestore document.
 *
 * This example uses the UserAuth class for authentication.
 * See examples/App/AppInitialization for more authentication examples.
 *
 * The complete usage guidelines, please read README.md or visit https://github.com/mobizt/FirebaseClient
 *
 * SYNTAX:
 *
 * 1.------------------------
 *
 * Firestore::Documents::createDocument(<AsyncClient>, <Firestore::Parent>, <documentPath>, <DocumentMask>, <Document>, <AsyncResultCallback>, <uid>);
 *
 * Firestore::Documents::createDocument(<AsyncClient>, <Firestore::Parent>, <collectionId>, <documentId>, <DocumentMask>, <Document>, <AsyncResultCallback>, <uid>);
 *
 * <AsyncClient> - The async client.
 * <Firestore::Parent> - The Firestore::Parent object included project Id and database Id in its constructor.
 * <documentPath> - The relative path of document to create in the collection.
 * <DocumentMask> - The fields to return. If not set, returns all fields. Use comma (,) to separate between the field names.
 * <collectionId> - The relative path of document collection id to create the document.
 * <documentId> - The document id of document to be created.
 * <Document> - The Firestore document.
 * <AsyncResultCallback> - The async result callback (AsyncResultCallback).
 * <uid> - The user specified UID of async result (optional).
 *
 * The Firebase project Id should be only the name without the firebaseio.com.
 * The Firestore database id should be (default) or empty "".
 */

#include <Arduino.h>
#include <FirebaseClient.h>
#include "ExampleFunctions.h" // Provides the functions used in the examples.
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>                // Add this for time functions

#define WIFI_SSID "AttyGadon 2.5"
#define WIFI_PASSWORD "Kroi101!"

#define API_KEY "AIzaSyAotzXnCdaP9YxfK6lhQb7hD4ItgGfc73s"
#define USER_EMAIL "jonken.rosario@cvsu.edu.ph"
#define USER_PASSWORD "sunbox123"
#define FIREBASE_PROJECT_ID "sunbox-53ae6"

// NTP settings
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 28800     // Replace with your timezone offset in seconds (e.g. 28800 = GMT+8)
#define DAYLIGHT_OFFSET_SEC 0    // Daylight saving time offset in seconds (usually 0 or 3600)

// Coin detector pins
#define coinPin1 23
#define coinPin2 19
#define coinPin3 18

// LCD PINS, SDA = 21 , SCL = 22, POS NEG
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

void processData(AsyncResult &aResult);
void create_document_async(Document<Values::Value> &doc, const String &documentPath);
void create_document_async2(Document<Values::Value> &doc, const String &documentPath);
void create_document_await(Document<Values::Value> &doc, const String &documentPath);
String getTimestampString(uint64_t sec, uint32_t nano);
void handlePulse(int count, int slot);
void sendCoinDataToFirebase(int creditAmount, int slotNumber);

SSL_CLIENT ssl_client;

// This uses built-in core WiFi/Ethernet for network connection.
// See examples/App/NetworkInterfaces for more network examples.
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

// Use a longer expiration period (3500 seconds) to reduce token refresh frequency
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD, 3500 /* expire period in seconds (<3600) */);
FirebaseApp app;

Firestore::Documents Docs;

AsyncResult firestoreResult;

int data_count = 0;

unsigned long dataMillis = 0;

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

void setup()
{
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

    set_ssl_client_insecure_and_buffer(ssl_client);

    Serial.println("Initializing app...");
    
    // Initialize with a longer timeout for better chance of success
    // This will block until the app is initialized or the timeout is reached
    initializeApp(aClient, app, getAuth(user_auth), 20 * 1000, auth_debug_print);
    
    // Get the Firestore instance
    app.getApp<Firestore::Documents>(Docs);
    
    // Give a bit more time for everything to settle
    for (int i = 0; i < 10; i++) {
        app.loop();
        delay(100);
    }

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

    delay(1000);
    
    Serial.println("Timezone settings:");
    Serial.print("GMT offset (seconds): ");
    Serial.println(GMT_OFFSET_SEC);
    Serial.print("Daylight offset (seconds): ");
    Serial.println(DAYLIGHT_OFFSET_SEC);

    // Initialize NTP with more aggressive settings and multiple servers
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER, "time.google.com", "time.windows.com");
    
    // Wait for NTP sync with a proper timeout
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WAITING FOR TIME");
    
    // Wait up to 20 seconds for time sync
    time_t now = 0;
    int timeout = 20;
    bool timeOK = false;
    
    Serial.println("Waiting for NTP time sync...");
    
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
    
    // Get the time and display it
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
    
    // Display Firebase initialization status on LCD
    lcd.setCursor(0, 1);
    lcd.print("CONNECTING FB...  ");
    
    // Attempt to create a test document to verify Firebase connection
    // Give Firebase more time to initialize - this is often a key issue
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CONNECTING TO FB");
    
    // Wait much longer for Firebase to be fully ready
    for (int i = 0; i < 30; i++) {
        app.loop();  // Process Firebase tasks
        
        // Show countdown on LCD
        lcd.setCursor(0, 1);
        lcd.print("PLEASE WAIT ");
        lcd.print(30-i);
        lcd.print("s   ");
        
        // More frequent app loop calls with shorter delays
        delay(500);
        app.loop();
        delay(500);
    }
    
    if (app.ready()) {
        // Just show connection status without creating test documents
        lcd.setCursor(0, 1);
        lcd.print("FB CONNECTED OK! ");
        delay(2000);
    } else {
        // Not ready - show on LCD
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WAITING FOR FB");
        lcd.setCursor(0, 1);
        lcd.print("WILL RETRY LATER");
        delay(3000);
    }
    
    // After status message, show normal coin message
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("SUNBOX");
    lcd.setCursor(1, 1);
    lcd.print("INSERT A COINS");
}

// No need for a separate collection creation function - 
// The collection will be created automatically when the first real coin data is sent

void loop()
{
    // To maintain the authentication and async tasks
    app.loop();
    
    // No need to create collection at startup - it will be created automatically
    static bool collectionInitialized = false;
    if (!collectionInitialized && app.ready()) {
        collectionInitialized = true;
    }

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

    // Periodic Firebase health check and time sync
    if (millis() - dataMillis > 60000 || dataMillis == 0)
    {
        dataMillis = millis();
        
        // Call app.loop() more frequently to maintain the connection
        app.loop();
        
        if (app.ready()) {
            // Just run a quick connection check without logging anything
            // This helps keep the connection alive
            app.loop();
            
            // Also periodically resync time (every 15 minutes)
            static unsigned long lastTimeSync = 0;
            if (millis() - lastTimeSync > 900000 || lastTimeSync == 0) { // 15 minutes
                lastTimeSync = millis();
                
                // Force a time sync
                configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
                
                // Debug output to serial
                time_t now;
                time(&now);
                struct tm* timeinfo = localtime(&now);
                char time_str[30];
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
                Serial.print("Time synchronized: ");
                Serial.println(time_str);
                
                // Briefly show time on LCD
                lcd.setCursor(15, 0);
                lcd.print("T");
                delay(300);
                lcd.setCursor(15, 0);
                lcd.print(" ");
            }
        }
    }

    // For async call with AsyncResult.
    processData(firestoreResult);
    
    // Aggressively monitor Firebase connection (every 30 seconds)
    static unsigned long lastStatusCheck = 0;
    static int reconnectAttempts = 0;
    
    if (millis() - lastStatusCheck > 30000) {  // 30 seconds
        lastStatusCheck = millis();
        
        // Run app.loop() regardless to maintain connection
        app.loop();
        
        // Just check if Firebase is ready
        if (app.ready()) {
            // Reset reconnect attempts counter when we confirm connection is good
            reconnectAttempts = 0;
            
            // Just briefly show a connection check indicator
            lcd.setCursor(15, 0);
            lcd.print("*");
            delay(300);
            lcd.setCursor(15, 0);
            lcd.print(" ");
        }
        
        // If Firebase is not ready, try to reconnect
        if (!app.ready()) {
            reconnectAttempts++;
            
            // Show reconnection attempt on LCD
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("FB DISCONNECTED");
            lcd.setCursor(0, 1);
            lcd.print("RECONNECT #");
            lcd.print(reconnectAttempts);
            
            // Process auth tasks more aggressively
            for (int i = 0; i < 5; i++) {
                app.loop();
                delay(200);
            }
            
            // Return to normal display
            lcd.clear();
            lcd.setCursor(2, 0);
            lcd.print("SUNBOX");
            lcd.setCursor(0, 1);
            lcd.print("INSERT A COINS");
        }
    }
}

// Handle a coin insertion pulse
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

        // Send the coin data to Firebase
        sendCoinDataToFirebase(creditToAdd, slot);
    }
}

// Send coin insertion data to Firebase
void sendCoinDataToFirebase(int creditAmount, int slotNumber) {
    if (!app.ready()) {
        Serial.println("Firebase not ready yet, cannot send coin data");
        return;
    }

    // Check if time is synchronized
    time_t now;
    time(&now);
    
    // If time is not synchronized properly, now will be close to 0
    if (now < 1609459200) { // January 1, 2021 as a sanity check
        Serial.println("Time not synchronized yet! Waiting for NTP...");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("TIME SYNC ERROR");
        lcd.setCursor(0, 1);
        lcd.print("RETRYING...");
        
        // Try to force a more robust time sync with multiple servers
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER, "time.google.com", "time.windows.com");
        
        // Wait for sync with a proper timeout
        int timeout = 10;
        while (timeout > 0) {
            delay(1000);
            time(&now);
            if (now > 1609459200) {
                Serial.println("Time sync successful!");
                break;
            }
            timeout--;
            Serial.print("Waiting for time sync... ");
            Serial.println(timeout);
        }
        
        if (now < 1609459200) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("TIME SYNC FAIL!");
            lcd.setCursor(0, 1);
            lcd.print("CANNOT SEND DATA");
            delay(2000);
            
            Serial.println("Failed to get current time. Cannot send data.");
            return;
        }
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("TIME SYNCED OK!");
        delay(1000);
    }
    
    // Format date as YYYY-MM-DD for document ID
    struct tm* timeinfo = localtime(&now);
    char date_str[11];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", timeinfo);
    String dateString = String(date_str);
    
    // Add full timestamp for debugging
    char full_time_str[30];
    strftime(full_time_str, sizeof(full_time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
    Serial.print("Current local time: ");
    Serial.println(full_time_str);
    
    // Create timestamp string for the entry
    Values::TimestampValue tsV(getTimestampString(now, 0));
    
    // Store the exact credit amount (5 or 10) directly
    Values::IntegerValue coinValueV(creditAmount);
    
    // Path structure:
    // Collection: COIN
    // Document: current date (YYYY-MM-DD)
    // Field: array of coin values
    
    // Use a simpler approach - ONE collection with ONE document for each coin
    // This approach is most reliable and less likely to cause issues
    
    // Generate a unique ID for each coin
    String coinID = dateString + "_" + String(millis()) + "_" + String(random(1000));
    
    // Very simple path structure: just collection/document
    String coinDocPath = "COIN/" + coinID;
    
    Serial.println("Sending coin data to Firebase...");
    Serial.print("Document path: ");
    Serial.println(coinDocPath);
    Serial.print("Date: ");
    Serial.println(dateString);
    Serial.print("Coin value: ");
    Serial.println(creditAmount);
    
    // Create the document with the required fields
    // Use a simpler structure to avoid problems
    Document<Values::Value> doc("coin_value", Values::Value(coinValueV));
    doc.add("slot", Values::Value(Values::IntegerValue(slotNumber)));
    doc.add("timestamp", Values::Value(tsV));
    doc.add("date", Values::Value(Values::StringValue(dateString)));
    
    // Show sending status on LCD
    lcd.setCursor(0, 1);
    lcd.print("SENDING DATA...  ");
    
    // Add the coin data to the database 
    // Use a more unique ID for the document path
    create_document_async(doc, coinDocPath);
    
    // Wait briefly to show sending message
    delay(500);
    
    // Return to showing credits
    lcd.setCursor(0, 1);
    lcd.print("CREDITS: ");
    lcd.print(coinCount);
    lcd.print("      ");
}

void processData(AsyncResult &aResult)
{
    // Exits when no result available when calling from the loop.
    if (!aResult.isResult())
        return;

    // Save current LCD state
    static int lastCoinCount = 0;
    if (coinCount != lastCoinCount) {
        lastCoinCount = coinCount;
    }
    
    // Only show status messages for createDocumentTask
    if (aResult.uid() == "createDocumentTask") {
        if (aResult.isError())
        {
            // Common error codes and their handling:
            // 409: Document already exists - data was likely saved with a different ID
            // 401/403: Authentication issues
            // 429: Too many requests
            // 500-599: Server errors
            
            int errorCode = aResult.error().code();
            
            if (errorCode == 409) {
                // Document already exists error (409), but data was likely saved
                // This is not actually an error in our case
                lcd.setCursor(0, 1);
                lcd.print("DATA SAVED OK!  ");
                delay(1000);
            } 
            else if (errorCode == 401 || errorCode == 403) {
                // Auth errors - try to refresh auth
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("AUTH ERROR: ");
                lcd.print(errorCode);
                lcd.setCursor(0, 1);
                lcd.print("REFRESHING AUTH");
                
                // Try to refresh auth by running loop
                for (int i = 0; i < 5; i++) {
                    app.loop();
                    delay(100);
                }
                
                delay(1000);
            }
            else {
                // Show error on LCD briefly
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("FB ERROR: ");
                lcd.print(errorCode);
                lcd.setCursor(0, 1);
                lcd.print("DATA NOT SAVED!");
                delay(2000);
            }
        }
        else if (aResult.available())
        {
            // Show success on LCD very briefly
            lcd.setCursor(0, 1);
            lcd.print("DATA SAVED OK!  ");
            delay(1000);
        }
        
        // Return to normal display
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("SUNBOX");
        lcd.setCursor(0, 1);
        lcd.print("CREDITS: ");
        lcd.print(coinCount);
        lcd.print("      ");
    }
}

void create_document_async(Document<Values::Value> &doc, const String &documentPath)
{
    Serial.println("Creating a document... ");

    // Make sure Firebase is ready before trying to create a document
    if (!app.ready()) {
        // Try to refresh connection with multiple loops
        for (int i = 0; i < 5; i++) {
            app.loop();
            delay(100);
        }
        
        // If still not ready, show message and continue anyway
        // Sometimes operations work even when app.ready() returns false
        if (!app.ready()) {
            Serial.println("Warning: Firebase may not be fully ready");
            lcd.setCursor(0, 1);
            lcd.print("FB CONNECTING... ");
            delay(500);
        }
    }

    // Async call with callback function.
    // We'll send the document regardless of ready state as it sometimes works anyway
    Docs.createDocument(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask(), doc, processData, "createDocumentTask");
}

void create_document_async2(Document<Values::Value> &doc, const String &documentPath)
{
    Serial.println("Creating a document... ");

    // Async call with AsyncResult for returning result.
    Docs.createDocument(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask(), doc, firestoreResult);
}

void create_document_await(Document<Values::Value> &doc, const String &documentPath)
{
    Serial.println("Creating a document... ");

    // Sync call which waits until the payload was received.
    String payload = Docs.createDocument(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask(), doc);
    if (aClient.lastError().code() == 0)
        Serial.println(payload);
    else
        Firebase.printf("Error, msg: %s, code: %d\n", aClient.lastError().message().c_str(), aClient.lastError().code());
}

String getTimestampString(uint64_t sec, uint32_t nano)
{
    // Check if we got an epoch or near-epoch timestamp and force a fresh time sync
    if (sec < 1609459200) { // Before Jan 1, 2021 - likely incorrect
        Serial.println("Warning: Timestamp appears to be incorrect (epoch or near-epoch date)");
        Serial.println("Forcing a time resynchronization...");
        
        // Try to force a robust time sync with multiple servers and longer timeout
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.google.com", "time.windows.com");
        
        // Wait for sync with timeout
        int timeout = 10; // seconds
        time_t now_check;
        while (timeout > 0) {
            time(&now_check);
            if (now_check > 1609459200) { // After Jan 1, 2021
                // We got a valid time
                sec = now_check;
                Serial.println("Time successfully synchronized!");
                break;
            }
            delay(1000);
            timeout--;
            Serial.print("Waiting for time sync... ");
            Serial.println(timeout);
        }
        
        if (timeout <= 0) {
            Serial.println("Failed to synchronize time. Using device time anyway.");
        }
    }

    if (sec > 0x3afff4417f)
        sec = 0x3afff4417f;

    if (nano > 0x3b9ac9ff)
        nano = 0x3b9ac9ff;

    time_t now = sec;
    struct tm ts;
    char buf[80];
    
    // Manual timezone adjustment for Philippines timezone (UTC+8)
    // The configTime() function sometimes doesn't apply the timezone correctly
    // Manually subtract 8 hours (28800 seconds) to correct the timestamp
    now -= GMT_OFFSET_SEC; // This actually REMOVES the timezone offset since Firestore expects UTC time
    
    // Add debug info
    Serial.print("Original timestamp seconds: ");
    Serial.println(sec);
    Serial.print("Adjusted timestamp seconds: ");
    Serial.println(now);
    
    // Convert to GMT/UTC time for Firestore (it expects UTC)
    // Use proper casting from uint64_t to time_t for ESP32
    time_t now_time_t = (time_t)now;
    ts = *gmtime(&now_time_t);
    
    // Debug info for the timestamp - using proper type casting for ESP32
    char debug_time_local[30];
    time_t sec_time_t = (time_t)sec; // Proper casting from uint64_t to time_t
    struct tm *local_ts = localtime(&sec_time_t); // Original time with timezone
    strftime(debug_time_local, sizeof(debug_time_local), "%Y-%m-%d %H:%M:%S", local_ts);
    Serial.print("Formatted timestamp (local): ");
    Serial.println(debug_time_local);
    
    char debug_time_utc[30];
    strftime(debug_time_utc, sizeof(debug_time_utc), "%Y-%m-%d %H:%M:%S", &ts);
    Serial.print("Formatted timestamp (UTC for Firestore): ");
    Serial.println(debug_time_utc);

    // Format for Firestore (it expects UTC time with Z suffix)
    String format = "%Y-%m-%dT%H:%M:%S";

    if (nano > 0)
    {
        String fraction = String(double(nano) / 1000000000.0f, 9);
        fraction.remove(0, 1);
        format += fraction;
    }
    format += "Z";

    strftime(buf, sizeof(buf), format.c_str(), &ts);
    return buf;
}