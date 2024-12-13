
#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#include <HttpClient.h>
#include <WiFi.h>
#include <inttypes.h>
#include <stdio.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

/* Photoresistor Variables */
const int photoResistorPin = 32; 
const int ledPin = 27;            
int minLightValue = 4095; 
int maxLightValue = 0;
unsigned long calibrationTime = 10000; // 10 seconds
unsigned long startTime;

/* RFID Variables */
#define RST_PIN 33   // Reset pin
#define SS_PIN 21    // Slave Select pin (SDA)
MFRC522 rfid(SS_PIN, RST_PIN);


/* WIFI Connection */
char ssid[50]; // your network SSID (name)
char pass[50]; // your network password (use for WPA, or use
               // as key for WEP)

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30 * 1000;

// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;


void blinkLED();
void serverCode(String url);
void nvs_access();

void setup() {
  Serial.begin(9600);
  delay(1000);

  // WIFI -----------------------------------------
  nvs_access();
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());
  // -------------------------------------------------
  delay(2000);



  SPI.begin(2, 15, 22, 21); // SCK, MISO, MOSI, SDA

  pinMode(photoResistorPin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  delay(1000);

  

  rfid.PCD_Init();     
  Serial.println("RFID Reader initialized. Scan an RFID card or fob...\n\n");
  delay(1000);

  // Start calibration
  startTime = millis();
  Serial.println("Calibration Phase: Move the sensor to minimum and maximum light.");

}


void loop() {
  unsigned long currentTime = millis();
  int lightValue = analogRead(photoResistorPin);
  bool inCalibration = ((currentTime - startTime) <= calibrationTime);

  // Calibration For 10 Sec
  if (inCalibration) {
    blinkLED();
    
    // Saves the min and max light values
    if (lightValue > maxLightValue) {
      maxLightValue = lightValue;
    }
    
    if (lightValue < minLightValue) {
      minLightValue = lightValue;
    }
    
    Serial.print("Light Value: ");
    Serial.println(lightValue);

    delay(100);
  }

  if(!inCalibration) {
    // Check for new card
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      // Save the UID as a string
        String uid = "";
        for (byte i = 0; i < rfid.uid.size; i++) {
            uid += String(rfid.uid.uidByte[i], HEX);
        }
        Serial.println("RFID UID: " + uid);

        String url = String("/rfid_scan?pet_id=") + uid;
        serverCode(url);
    }
    int brightness = map(lightValue, minLightValue, maxLightValue, 255, 0);
    analogWrite(ledPin, brightness); // Set the LED brightness
  }

}


void blinkLED() {
    // Toggle the LED state
    int ledState = digitalRead(ledPin);
    digitalWrite(ledPin, !ledState); 
    delay(400);
}


void nvs_access() {
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  // Open
  Serial.printf("\n");
  Serial.printf("Opening Non-Volatile Storage (NVS) handle... ");
  nvs_handle_t my_handle;
  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    Serial.printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  } else {
    Serial.printf("Done\n");
    Serial.printf("Retrieving SSID/PASSWD\n");
    size_t ssid_len;
    size_t pass_len;
    err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
    err |= nvs_get_str(my_handle, "pass", pass, &pass_len);
    switch (err) {
      case ESP_OK:
      Serial.printf("Done\n");
      break;
      case ESP_ERR_NVS_NOT_FOUND:
      Serial.printf("The value is not initialized yet!\n");
      break;
      default:
      Serial.printf("Error (%s) reading!\n", esp_err_to_name(err));
    }
  }

  // Close
  nvs_close(my_handle);
}



void serverCode(String url) {
  int err = 0;
  WiFiClient c;
  HttpClient http(c);

  // Convert String to const char* 
  const char* urlCStr = url.c_str();

  err = http.get("13.59.144.115", 5000, urlCStr, NULL);
  if(err == 0) {
    // Serial.println("StartedRequest ok");
    err = http.responseStatusCode();
    if(err >= 0) {
      Serial.print("Got status code: ");
      Serial.print(err);

      err = http.skipResponseHeaders();
      if(err >= 0) {
        int bodyLen = http.contentLength();
        Serial.print("Content length is: ");
        Serial.println(bodyLen);
        Serial.println();
        Serial.println("Body returned follows: ");

        unsigned long timeoutStart = millis();
        char c;
        // Whilst we haven't timed out and haven't reached the end of the body
        while(http.connected() || http.available() && ((millis() - timeoutStart) < kNetworkTimeout)) {
          if(http.available() ) {
            c = http.read();
            // Print out the char
            Serial.print(c);

            bodyLen--;
            timeoutStart = millis();
          } 
          else {
            delay(kNetworkDelay);
          }
        }
      }
      else {
        Serial.print("Failed to skip response heade3rs: ");
        Serial.println(err);
      }
    }
    else {
      Serial.print("Getting response failed: ");
      Serial.println(err);
    }
  }
  else {
    Serial.print("Conect failed: ");
    Serial.println(err);
  }
}