#include <ESPmDNS.h>
#include <WiFi.h>
#include <HTTPClient.h>

/*
  =====================================================================
  JIAT/AI - ARTIFICIAL INTELLIGENCE - PRACTICAL ASSESSMENT
  ESP32 SMART ACCESS CONTROL SYSTEM
  =====================================================================
*/


// Wifi + Server Configuration

const char* SSID = "XXXXXXXXXXXXXXXXXXXXXXX";
const char* WIFI_PASSWORD = "XXXXXXXXXXXXXXXXXXX";
const char* HOST_NAME = "smartaccess";
const char* LOGS_URL = "https://script.google.com/macros/s/XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX/exec";

// Valid Login Credentials

const char* VALID_USERNAME = "admin";
const char* VALID_PASSWORD = "1234";

// Hardware Configuration

const uint8_t GREEN_LED = 33;
const uint8_t YELLOW_LED = 32;
const uint8_t RED_LED_1 = 13;  // lights on 1st failed attempt
const uint8_t RED_LED_2 = 27;  // lights on 2nd failed attempt
const uint8_t RED_LED_3 = 14;  // lights on 3rd failed attempt
const uint8_t BUZZER_PIN = 25;

// System Status

int FAILED_ATTEMPTS = 0;
const int MAX_ATTEMPTS = 3;
bool IS_LOCKED = false;
unsigned long PREVIOUS_MILLIS = 0;
const long INTERVAL = 500;  // blink speed

WiFiServer server(80);

void sendLog() {
}

void allRedOff() {
  digitalWrite(RED_LED_1, LOW);
  digitalWrite(RED_LED_2, LOW);
  digitalWrite(RED_LED_3, LOW);
}

void setReadyState() {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, HIGH);
  allRedOff();
  digitalWrite(BUZZER_PIN, LOW);
}

void setSuccessState() {
}

void wifiConnetingState() {
  digitalWrite(YELLOW_LED, HIGH);
  delay(500);
  digitalWrite(YELLOW_LED, LOW);
  delay(500);
}

void setup() {

  Serial.begin(115200);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED_1, OUTPUT);
  pinMode(RED_LED_2, OUTPUT);
  pinMode(RED_LED_3, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Connect to WiFi Network

  WiFi.begin(SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi Network ");

  while (WiFi.status() != WL_CONNECTED) {
    wifiConnetingState();
    Serial.print(".");
  }

  setReadyState();

  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // Start mDNS

  if (MDNS.begin(HOST_NAME)) {
    Serial.print("mDNS responder started: http://");
    Serial.print(HOST_NAME);
    Serial.println(".local");
  } else {
    Serial.println("Error setting up mDNS responder!");
  }

  server.begin();
  Serial.println("HTTP server started on port 80");

}


void loop() {
}
