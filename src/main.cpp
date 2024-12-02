#include <Arduino.h>
#include <SoftwareSerial.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <ESP_Google_Sheet_Client.h>
#include "DHT.h"
#include "time.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>
#include "secrets.h"

// === Pin and Hardware Configuration ===
#define DHT_PIN 15
#define DHT_TYPE DHT11
#define SDA_PIN 18
#define SCL_PIN 19
#define RXp2 22
#define TXp2 23
#define LED 2
#define ledPIN  5 // pin for LED control
#define NUM_PIXELS  70           // The number of pixels

// // === WiFi Configuration ===
// #define WIFI_SSID 
// #define WIFI_PASSWORD 

// // === Firebase Configuration ===
// #define API_KEY 
// #define USER_EMAIL 
// #define USER_PASSWORD 
// #define DATABASE_URL 

// // === Google Sheets Configuration ===
// const char spreadsheetId[] = 
// #define PROJECT_ID 
// #define CLIENT_EMAIL 
// const char PRIVATE_KEY[] PROGMEM = 

// === NTP Server ===
const char* ntpServer = "pool.ntp.org";

// === Global Variables ===
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);
SoftwareSerial mySerial(RXp2, TXp2);
DefaultNetwork network;
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client, getNetwork(network));
RealtimeDatabase Database;
AsyncResult aResult_no_callback;
Adafruit_NeoPixel pixels(NUM_PIXELS, ledPIN, NEO_RGB + NEO_KHZ800);
unsigned long lastUpdateTime = 0;

// === Function Prototypes ===
void setupWiFi();
void setupFirebase();
void setupGoogleSheet();
void setupLCD();
void setupTimeSync();
void setupNeoPixel();
unsigned long getCurrentEpochTime();
void updateSensors();
void tokenStatusCallback(TokenInfo info);
void printResult(AsyncResult &aResult);
void appendToGoogleSheet(float temperature, float humidity, int soilMoisture, int light, int pumpStatus);

// === Setup Function ===
void setup() {
    Serial.begin(115200);
    mySerial.begin(9600);

    pinMode(LED, OUTPUT);
    setupWiFi();
    setupFirebase();
    setupGoogleSheet();
    setupLCD();
    setupTimeSync();
    setupNeoPixel();
    dht.begin();

    Serial.println("Setup complete.");
}

float Temperature, Humidity;
unsigned long epochTime; 

// === Loop Function ===
void loop() {
    app.loop();
    Database.loop();
    if (millis() - lastUpdateTime > 10000 || lastUpdateTime == 0) {
        lastUpdateTime = millis();
        updateSensors();
    }
    printResult(aResult_no_callback);
}

// === Function Implementations ===

// Connect to WiFi
void setupWiFi() {
    Serial.print("Connecting to Wi-Fi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(300);
    }
    Serial.println("\nConnected to Wi-Fi. IP: " + WiFi.localIP().toString());
}

// Initialize Firebase
void setupFirebase() {
    Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
    Serial.println("Initializing app...");
    ssl_client.setInsecure();
    initializeApp(aClient, app, getAuth(user_auth), aResult_no_callback);
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
}

void setupGoogleSheet() {
    GSheet.printf("ESP Google Sheet Client v%s\n\n", ESP_GOOGLE_SHEET_CLIENT_VERSION);
    GSheet.setTokenCallback(tokenStatusCallback);
    GSheet.setPrerefreshSeconds(10 * 60);
    GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
}

// Initialize LCD
void setupLCD() {
    Wire.begin(SDA_PIN, SCL_PIN);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 1);
}

// Sync Time with NTP
void setupTimeSync() {
    configTime(0, 0, ntpServer);
    Serial.println("Time synchronized.");
}

void setupNeoPixel() {
    pixels.begin();
    pixels.setBrightness(100);
    pixels.show();
}

// Get Current Epoch Time
unsigned long getCurrentEpochTime() {
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return 0;
    }
    time(&now);
    return now;
}

// Update Sensor Data and Send to Firebase and Google Sheets
void updateSensors() {
    digitalWrite(LED, HIGH);
        lcd.clear();
        Temperature = dht.readTemperature();
        Serial.println("Temperature:" + String(int(Temperature)));
        Humidity = dht.readHumidity();
        Serial.println("Humidity:"+String(int(Humidity)));

        lcd.setCursor(0, 0);
        lcd.print("Temp:"+String(int(Temperature))+" Humi:"+String(int(Humidity)));

        object_t ts_json;
        JsonWriter writer;
        FirebaseJson valueRange;
        FirebaseJson response;
        writer.create(ts_json, ".sv", "timestamp"); // -> {".sv": "timestamp"}
        epochTime = getCurrentEpochTime();
        while (mySerial.available() > 0) {
            String key = mySerial.readStringUntil('\n');
            key.trim();
            Serial.println(key);

            if (key == "=") {
                String soil_moisture_str = mySerial.readStringUntil('\n');
                soil_moisture_str.trim();

                String ldr_str = mySerial.readStringUntil('\n');
                ldr_str.trim();

                String motor_str = mySerial.readStringUntil('\n');
                motor_str.trim();
                
                valueRange.add("majorDimension", "COLUMNS");
                if (!soil_moisture_str.isEmpty() && soil_moisture_str.toInt() > 0) {
                    int soil_moisture = 100 - soil_moisture_str.toInt();
                    Serial.println("soil_moisture: " + String(int(soil_moisture)));
                    Database.set<int>(aClient, "/ESP32-Gateway1/plant1/soil_moisture", soil_moisture, aResult_no_callback);
                    Database.set<object_t>(aClient, "/ESP32-Gateway1/plant1/timestamp", ts_json);
                    valueRange.set("values/[0]/[0]", epochTime);
                    valueRange.set("values/[1]/[0]", soil_moisture);
                } else {
                    Serial.println("Invalid soil moisture data: " + soil_moisture_str);
                }
                if (!ldr_str.isEmpty() && ldr_str.toInt() > 0) {
                    int ldr = 1000 - ldr_str.toInt();
                    if (ldr < 500)
                    {
                        for ( uint16_t i=0; i < NUM_PIXELS; i++ ) {
                        // Set the color value of the i pixel.
                        pixels.setPixelColor( i, 0x005998); // GRB
                        }

                    }else
                    {
                        for ( uint16_t i=0; i < NUM_PIXELS; i++ ) {
                        // Set the color value of the i pixel.
                        pixels.setPixelColor( i, 0x000000); // GRB
                        }
                    }
                    pixels.show();
                    Serial.println("ldr: " + String(ldr));
                    Database.set<int>(aClient, "/ESP32-Gateway1/plant1/light", ldr, aResult_no_callback);
                    valueRange.set("values/[2]/[0]", ldr);
                } else {
                    Serial.println("Invalid LDR data: " + ldr_str);
                }
                if (!motor_str.isEmpty() && motor_str.toInt() >= 0) {
                    int motor = motor_str.toInt();
                    Serial.println("motor: " + String(motor));
                    Database.set<bool>(aClient, "/ESP32-Gateway1/plant1/pump", motor, aResult_no_callback);
                    valueRange.set("values/[3]/[0]", motor);
                } else {
                    Serial.println("Invalid motor data: " + motor_str);
                }
                lcd.setCursor(0, 1);
                lcd.print("soil:" + String(100 - soil_moisture_str.toInt()) + " LDR:" + String(1000 - ldr_str.toInt()));
                bool success = GSheet.values.append(&response /* returned response */, spreadsheetId /* spreadsheet Id to append */, "plant1!A2" /* range to append */, &valueRange /* data range to append */);
                if (success){
                    response.toString(Serial, true);
                    valueRange.clear();
                }
                else{
                    Serial.println(GSheet.errorReason());
                }
            } else {
                Serial.println("Unexpected key: " + key);
            }
        }
        valueRange = {};
        Database.set<object_t>(aClient, "/ESP32-Gateway1/timestamp", ts_json);
        Database.set<number_t>(aClient, "/ESP32-Gateway1/temperature", number_t(Temperature, 2), aResult_no_callback);
        Database.set<number_t>(aClient, "/ESP32-Gateway1/humidity", number_t(Humidity, 2), aResult_no_callback);
        Serial.println("\nAppend spreadsheet values...");
        valueRange.add("majorDimension", "COLUMNS");
        valueRange.set("values/[0]/[0]", epochTime);
        valueRange.set("values/[1]/[0]", Temperature);
        valueRange.set("values/[2]/[0]", Humidity);
        // Append values to the spreadsheet
        bool success = GSheet.values.append(&response /* returned response */, spreadsheetId /* spreadsheet Id to append */, "ESP32-Gateway1!A2" /* range to append */, &valueRange /* data range to append */);
        if (success){
            response.toString(Serial, true);
            valueRange.clear();
        }
        else{
            Serial.println(GSheet.errorReason());
        }
        digitalWrite(LED,LOW);
}

void printResult(AsyncResult &aResult)
{
    if (aResult.isEvent())
    {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
    }

    if (aResult.isDebug())
    {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    if (aResult.available())
    {
        Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
    }
}

void tokenStatusCallback(TokenInfo info){
    if (info.status == token_status_error){
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
        GSheet.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
    }
    else{
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
    }
}