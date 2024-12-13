
#include <Arduino.h>
#include <HttpClient.h>
#include <WiFi.h>
#include <inttypes.h>
#include <stdio.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"


#include <Wire.h>
#include "string.h"
#include <Servo.h>

#include <SPI.h>
#include <MFRC522.h>
#include "HX711.h"
#include "soc/rtc.h"


void serverCode();
void nvs_access();
void attemptFeed();


/* Weight Sensor Variable */
const int LOADCELL_DOUT_PIN = 22;
const int LOADCELL_SCK_PIN = 17;
bool calibrateTotalFeed = true;
float desiredFoodWeight;

HX711 scale;


/* Servo Variables */
const int SERVO_PIN = 16;
const int SERVO_OPEN_POS = 87;
const int SERVO_CLOSE_POS = 7;
bool dispenseFood = false;

Servo myServo;

/* WIFI Connection */
char ssid[50]; // your network SSID (name)
char pass[50]; // your network password (use for WPA, or use
               // as key for WEP)

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30 * 1000;

// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

/* Feeding variables */
unsigned long lastFeedTime = 0;
const unsigned long feedInterval = 1 * 20 * 1000;  // shortened for demo purposes

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


  // Weight Sensor -----------------------------------------
  rtc_cpu_freq_config_t config;
  rtc_clk_cpu_freq_get_config(&config);
  rtc_clk_cpu_freq_to_config(RTC_CPU_FREQ_80M, &config);
  rtc_clk_cpu_freq_set_config_fast(&config);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
            
  scale.set_scale(332.3238); // obtained by calibrating the scale with known weights
  scale.tare();               
  // -------------------------------------------------

  // Servo -----------------------------------------
  myServo.attach(SERVO_PIN);
  // start with servo closed
  myServo.write(SERVO_CLOSE_POS);
  // -------------------------------------------------

  delay(1000);
}

void loop() {
  if(calibrateTotalFeed) {
    Serial.print("\nPlace a full portion of food on the scale.\n");
    delay(2000);
    Serial.print("Your food weighs: ");
    desiredFoodWeight = scale.get_units();
    Serial.println(scale.get_units(), 1);
    calibrateTotalFeed = false;
    delay(4000);
  } 

   if(!calibrateTotalFeed) {
      unsigned long currentTime = millis();
      Serial.print("Current Time: ");
      Serial.println(currentTime);

      // Check if it's time to feed
      if (currentTime - lastFeedTime >= feedInterval) {
        Serial.println("Time to feed!");
        attemptFeed();
        lastFeedTime = currentTime;  // Reset the feeding timer
      }
  }
  delay(1000);
}


void attemptFeed() {
  bool updatedServer = false;
  while (true) {
    float currentWeight = scale.get_units();
    Serial.print("Scale reading: ");
    Serial.println(currentWeight, 1);

    scale.power_down();
    delay(500);
    scale.power_up();

    // allow for a small margin of error when determining
    //   if there is enough food in the bowl
    if (currentWeight + 5 < desiredFoodWeight) {
      if(!updatedServer) {
        serverCode();
        updatedServer = true;
      }
      Serial.println("The servo will dispense food now!\n");
      
      // Open the servo to dispense food
      myServo.write(SERVO_OPEN_POS);
      delay(500);  // Adjust this delay based on dispensing mechanism

      // Close the servo
      myServo.write(SERVO_CLOSE_POS);
    } else {
      Serial.println("Desired food weight reached. Feeding complete.\n");
      break;  // Exit the loop when the desired weight is reached
    }
  }
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



void serverCode() {
  // Server -----------------------------------------
  int err = 0;
  WiFiClient c;
  HttpClient http(c);

  err = http.get("13.59.144.115", 5000, "/feeding", NULL);
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
  // -------------------------------------------------
}


