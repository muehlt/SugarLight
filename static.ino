/*  
 *   
 *  ███████╗██╗   ██╗ ██████╗  █████╗ ██████╗ ██╗     ██╗ ██████╗ ██╗  ██╗████████╗
 *  ██╔════╝██║   ██║██╔════╝ ██╔══██╗██╔══██╗██║     ██║██╔════╝ ██║  ██║╚══██╔══╝
 *  ███████╗██║   ██║██║  ███╗███████║██████╔╝██║     ██║██║  ███╗███████║   ██║   
 *  ╚════██║██║   ██║██║   ██║██╔══██║██╔══██╗██║     ██║██║   ██║██╔══██║   ██║   
 *  ███████║╚██████╔╝╚██████╔╝██║  ██║██║  ██║███████╗██║╚██████╔╝██║  ██║   ██║   
 *  ╚══════╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝                                                                           
 *  
 *  static.ino - version 0.0.1
 *  
 *  Github Repo: https://github.com/Schimpi/SugarLight
 *  License: https://github.com/Schimpi/SugarLight/blob/master/LICENSE.md
 *  
 *  USE THIS CODE AT YOUR OWN RISK AND ONLY FOR EDUCATIONAL PURPOSE,
 *  NEVER USE FOR TREATMENT DECISIONS!
 *  
 *  CURRENTLY THIS CODE IS ESPECIALLY VULNERABLE AND EXPERIMENTAL BECAUSE
 *  IT IS VERY NEW AND THERE ARE NO TESTS OVER A LONGER PERIOD OF TIME.
 *  
 *  CURRENTLY THIS PROJECT ONLY SUPPORTS NIGHTSCOUT PAGES SET TO MG/DL UNIT!
 * 
 */

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// +-----------------+
// |  WIFI SETTINGS  |
// +-----------------+

#define WIFI_NAME "YourWifiName"
#define WIFI_PASSWORD "YourWifiPassword"

// +------------------+
// |  NIGHTSCOUT API  |
// +------------------+

// don't forget the https:// if you are using ssl (recommended), there is no slash (/) at the end
#define NIGHTSCOUT_URL "https://nightscout-site.herokuapp.com"
#define NIGHTSCOUT_TOKEN "yourname-sometoken"
#define NIGHTSCOUT_USE_SSL true
#define NIGHTSCOUT_SHA1 "PUT YOUR SHA1 FINGERPRINT HERE"

// +------------+
// |  LED PINS  |
// +------------+

#define PIN_R D0
#define PIN_G D1
#define PIN_B D2

// +----------------+
// |  RANGE COLORS  |
// +----------------+
                     
int COLOR_LOW[] = {1023,0,0};
int COLOR_IN_RANGE[] = {0,1023,0};
int COLOR_HIGH[] = {0,0,1023};
#define DIM_FACTOR 1 // choose between 0.05 and 1; 0 would be completely off, 1 is the brightest setting

// +----------------+
// |  VALUE LIMITS  |
// +----------------+

#define LIMIT_HIGH 140 //TODO: to documentation: be sure not to overlap the ranges, else LOW will have highest and IN RANGE will have lowest priority
#define LIMIT_LOW 80
#define USE_DELTA true
#define DELTA_THRESHOLD 10

// +--------------------+
// |  TECHNICAL VALUES  |
// +--------------------+

#define BLINK_SPEED 4 // choose between 1 and 10
#define REFRESH_RATE 20000 // in ms
#define JSON_DOC_SIZE 1000
#define OLD_VALUE_THRESHOLD 420 // in s (default: 7 min)
#define NS_UPDATE_CYCLE 315000 // in ms (default: 5 min 15 sec)

// +------------+
// |  TIME API  |
// +------------+

#define TIME_API_URL "http://worldtimeapi.org/api/timezone/Etc/GMT"
#define TIME_API_USE_SSL true
#define TIME_API_SHA1 "62 75 37 20 2F 5B F4 5E 16 B3 DF 89 73 FA 63 C0 0F 81 08 55"






//---------------------------------------------------------------------------//
//---------------------------------------------------------------------------//
//--------------------------------- PROGRAM ---------------------------------//
//---------------------------------------------------------------------------//
//---------------------------------------------------------------------------//

int CLEAR[] = {1023,1023,1023};
int OFF[] = {0,0,0};
unsigned long last_api_call = -REFRESH_RATE;
unsigned long last_sgv_timestamp = millis() - NS_UPDATE_CYCLE;
bool led_blinking = false;
bool led_on = false;
int led_rgb[3] = {0,0,0};

void setup() {
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  showColor(OFF);

  // STARTING SERIAL MONITOR
  Serial.begin(115200);

  wifiSetup();
}

void loop() {
  if (led_rgb == OFF) { 
    led_blinking = false;
    led_on = false;
  }

  unsigned long current_mills = millis();

  if ((current_mills - last_api_call) >= REFRESH_RATE && abs(current_mills - last_sgv_timestamp) >= NS_UPDATE_CYCLE) { // abs to handle overflow
    last_api_call = current_mills;
    callAPI();
  }

  if (led_blinking) {
    if (led_on) {
      showColor(OFF);
      led_on = false;
    } else {
      showColor(led_rgb);
      led_on = true;
    }
  } else {
    showColor(led_rgb);
  }
  
  delay(1000 / BLINK_SPEED);
}

void callAPI() {
  if (WiFi.status() == WL_CONNECTED) 
  {
    HTTPClient http;
    String http_url = String(NIGHTSCOUT_URL) + "/api/v1/entries/current.json?token=" + String(NIGHTSCOUT_TOKEN);
    NIGHTSCOUT_USE_SSL ? http.begin(http_url, NIGHTSCOUT_SHA1) : http.begin(http_url);
    
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) 
    {
      String response = http.getString();

      StaticJsonDocument<JSON_DOC_SIZE> doc;
      DeserializationError error = deserializeJson(doc, response);

      if (!error) {
        int sgv = doc[0]["sgv"];
        int delta = doc[0]["delta"];
        unsigned long long entry_time = doc[0]["date"];
        Serial.println("Glucose Value:");
        Serial.println(sgv);
        Serial.println("Delta:");
        Serial.println(delta);
        http.end();
        TIME_API_USE_SSL ? http.begin(TIME_API_URL, TIME_API_SHA1) : http.begin(TIME_API_URL);

        httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
          
          response = http.getString();
          DeserializationError error = deserializeJson(doc, response);
    
          if (!error) {
            unsigned long long current_time = doc["unixtime"];
            unsigned long time_ago = current_time - (entry_time / 1000);

            Serial.println("Seconds ago:");
            Serial.println(time_ago);

            last_sgv_timestamp = millis() - (time_ago * 1000);
            
            if (time_ago < OLD_VALUE_THRESHOLD) {
              if (sgv > 0) {
                handleAPIData(sgv, delta);
              } else {
                memcpy(led_rgb, OFF, sizeof(led_rgb));
                Serial.println("Not trusting glucose value.");
              }
            } else {
              memcpy(led_rgb, OFF, sizeof(led_rgb));
              Serial.println("Nightscout entry old.");
            }
          } else {
            memcpy(led_rgb, OFF, sizeof(led_rgb));
            Serial.println("JSON parse error (Time Data)");
            Serial.println(error.c_str());
          }
          
        } else {
          memcpy(led_rgb, OFF, sizeof(led_rgb));
          Serial.println("Time API Error");
        }
      } else {
        memcpy(led_rgb, OFF, sizeof(led_rgb));
        Serial.println("JSON parse error (NS Data)");
        Serial.println(error.c_str());
      }
    } else {
      memcpy(led_rgb, OFF, sizeof(led_rgb));
      Serial.println("API error");
    }
    http.end();
  } else {
    memcpy(led_rgb, OFF, sizeof(led_rgb));
    Serial.println("WIFI not connected");
    wifiSetup();
  }
}

void handleAPIData(int sgv, int delta) {
  if (sgv <= LIMIT_LOW) {
    memcpy(led_rgb, COLOR_LOW, sizeof(led_rgb));
  } else if (sgv >= LIMIT_HIGH) {
    memcpy(led_rgb, COLOR_HIGH, sizeof(led_rgb));
  } else {
    memcpy(led_rgb, COLOR_IN_RANGE, sizeof(led_rgb));
  }

  if ((delta >= abs(DELTA_THRESHOLD) || delta <= -abs(DELTA_THRESHOLD)) && USE_DELTA)
    led_blinking = true;
  else
    led_blinking = false;
  
}

void showColor(int color[]) {
  analogWrite(PIN_R, (1023- (int) (color[0] * DIM_FACTOR)));
  analogWrite(PIN_G, (1023- (int) (color[1] * DIM_FACTOR)));
  analogWrite(PIN_B, (1023- (int) (color[2] * DIM_FACTOR)));
}

void wifiSetup() {
  // SETTING UP CONNECTION
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    showColor(CLEAR);
    delay(200);
    showColor(OFF);
    delay(300);
  }
  showColor(OFF);
}
