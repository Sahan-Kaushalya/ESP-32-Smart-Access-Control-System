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

const char* SSID = "SLT-Fiber-2.4G_75f0";
const char* WIFI_PASSWORD = "sick3652";
const char* HOST_NAME = "smartaccess";
const char* LOGS_URL = "https://script.google.com/macros/s/AKfycbz4O50CYNT64cGtF233XQaKGGoHfAMh7XtRpuHKp69_--PngDAWWF-NSgfPE0wCbdIgeA/exec";

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
uint8_t RED_LED_STATE = LOW;

WiFiServer server(80);


void sendLog() {
}

void allRedOff() {
  digitalWrite(RED_LED_1, LOW);
  digitalWrite(RED_LED_2, LOW);
  digitalWrite(RED_LED_3, LOW);
}

void blinkAllRed() {
  digitalWrite(RED_LED_1, HIGH);
  digitalWrite(RED_LED_2, HIGH);
  digitalWrite(RED_LED_3, HIGH);
  delay(300);
  digitalWrite(RED_LED_1, LOW);
  digitalWrite(RED_LED_2, LOW);
  digitalWrite(RED_LED_3, LOW);
  delay(300);
}

void setReadyState() {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, HIGH);
  allRedOff();
  digitalWrite(BUZZER_PIN, LOW);
}

void setSuccessState() {
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(YELLOW_LED, LOW);
  allRedOff();
  digitalWrite(BUZZER_PIN, LOW);
  playSuccessTone();
}

void wifiConnetingState() {
  digitalWrite(YELLOW_LED, HIGH);
  delay(500);
  digitalWrite(YELLOW_LED, LOW);
  delay(500);
}

void playSuccessTone() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(100);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
}

void playFailedTone() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(600);
  digitalWrite(BUZZER_PIN, LOW);
}

void playLockedTone() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);
  digitalWrite(BUZZER_PIN, LOW);
  delay(300);
}

void setFailState(int attempts) {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);

  if (attempts == 1) {
    digitalWrite(RED_LED_1, HIGH);
    digitalWrite(RED_LED_2, LOW);
    digitalWrite(RED_LED_3, LOW);
  } else if (attempts == 2) {
    digitalWrite(RED_LED_1, HIGH);
    digitalWrite(RED_LED_2, HIGH);
    digitalWrite(RED_LED_3, LOW);
  }
  playFailedTone();
}

void setLockedState() {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  blinkAllRed();
  playLockedTone();
}

void handleLockedAlarm() {
  unsigned long currentMillis = millis();
  if (currentMillis - PREVIOUS_MILLIS >= INTERVAL) {
    PREVIOUS_MILLIS = currentMillis;

    RED_LED_STATE = (RED_LED_STATE == LOW) ? HIGH : LOW;

    digitalWrite(RED_LED_1, RED_LED_STATE);
    digitalWrite(RED_LED_2, RED_LED_STATE);
    digitalWrite(RED_LED_3, RED_LED_STATE);

    playLockedTone();
  }
}

String getValue(String data, String key) {
  int startIndex = data.indexOf(key);
  if (startIndex == -1) return "";
  startIndex += key.length();

  int ampIndex = data.indexOf('&', startIndex);
  int spaceIndex = data.indexOf(' ', startIndex);

  int endIndex;
  if (ampIndex == -1) {
    endIndex = spaceIndex;
  } else if (spaceIndex == -1) {
    endIndex = ampIndex;
  } else {
    endIndex = min(ampIndex, spaceIndex);
  }
  if (endIndex == -1) endIndex = data.length();

  return data.substring(startIndex, endIndex);
}

String urlDecode(String input) {
  String decoded = "";
  char hexBuf[3] = { 0, 0, 0 };
  unsigned int len = input.length();
  unsigned int i = 0;

  while (i < len) {
    char c = input.charAt(i);
    if (c == '+') {
      decoded += ' ';
      i++;
    } else if (c == '%' && i + 2 < len) {
      hexBuf[0] = input.charAt(i + 1);
      hexBuf[1] = input.charAt(i + 2);
      decoded += (char)strtol(hexBuf, NULL, 16);
      i += 3;
    } else {
      decoded += c;
      i++;
    }
  }
  return decoded;
}

String buildJsonResponse(bool success, const String& status, const String& message, int attempts, bool locked) {
  String json = "{";
  json += "\"success\":" + String(success ? "true" : "false") + ",";
  json += "\"status\":\"" + status + "\",";
  json += "\"message\":\"" + message + "\",";
  json += "\"attempts\":" + String(attempts) + ",";
  json += "\"locked\":" + String(locked ? "true" : "false");
  json += "}";
  return json;
}

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Smart Access Control System</title>
<style>
  body {
    font-family: Arial, Helvetica, sans-serif;
    background: #1c1f26;
    color: #eee;
    display: flex;
    justify-content: center;
    align-items: center;
    min-height: 100vh;
    margin: 0;
  }
  .card {
    background: #262b36;
    padding: 30px 35px;
    border-radius: 12px;
    box-shadow: 0 8px 24px rgba(0,0,0,0.4);
    width: 320px;
    text-align: center;
  }
  h1 {
    font-size: 20px;
    margin-bottom: 20px;
    letter-spacing: 1px;
  }
  input {
    width: 100%;
    padding: 10px;
    margin: 8px 0;
    border-radius: 6px;
    border: 1px solid #444;
    background: #1c1f26;
    color: #eee;
    box-sizing: border-box;
  }
  input:disabled {
    opacity: 0.5;
  }
  button {
    width: 100%;
    padding: 10px;
    margin-top: 10px;
    border: none;
    border-radius: 6px;
    background: #3a7bd5;
    color: white;
    font-weight: bold;
    cursor: pointer;
  }
  button:disabled {
    background: #555;
    cursor: not-allowed;
  }
  #statusArea {
    margin-top: 18px;
    padding: 10px;
    border-radius: 6px;
    background: #1c1f26;
    font-size: 15px;
  }
  #attemptsArea {
    margin-top: 10px;
    font-size: 13px;
    color: #bbb;
  }
</style>
</head>
<body>
  <div class="card">
    <h1>SMART ACCESS CONTROL SYSTEM</h1>
    <input type="text" id="username" placeholder="Username" autocomplete="off">
    <input type="password" id="password" placeholder="Password" autocomplete="off">
    <button id="loginBtn" onclick="doLogin()">Login</button>
    <div id="statusArea">&#128993; System Ready</div>
    <div id="attemptsArea">Failed Attempts: 0 / 3</div>
  </div>

<script>
function doLogin() {
  var user = document.getElementById('username').value.trim();
  var pass = document.getElementById('password').value.trim();

  if (user === "" || pass === "") {
    document.getElementById('statusArea').innerHTML = "&#9888; Please enter both username and password";
    return;
  }

  var url = "/login?username=" + encodeURIComponent(user) + "&password=" + encodeURIComponent(pass);

  fetch(url)
    .then(function(res) { return res.json(); })
    .then(function(data) { updateUI(data); })
    .catch(function(err) {
      document.getElementById('statusArea').innerHTML = "&#9888; Connection error";
    });
}

function updateUI(data) {
  var statusArea = document.getElementById('statusArea');
  var attemptsArea = document.getElementById('attemptsArea');

  attemptsArea.innerHTML = "Failed Attempts: " + data.attempts + " / 3";

  if (data.locked) {
    statusArea.innerHTML = "&#128274; SYSTEM LOCKED";
    attemptsArea.innerHTML = "Failed Attempts: 3 / 3";
    document.getElementById('username').disabled = true;
    document.getElementById('password').disabled = true;
    document.getElementById('loginBtn').disabled = true;
  } else if (data.success) {
    statusArea.innerHTML = "&#128994; Access Granted";
  } else {
    statusArea.innerHTML = "&#128308; Access Denied";
  }
}
</script>
</body>
</html>
)rawliteral";

const char lockedHtmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>System Locked</title>
<style>
  body {
    font-family: Arial, Helvetica, sans-serif;
    background: #1c1f26;
    color: #eee;
    display: flex;
    justify-content: center;
    align-items: center;
    min-height: 100vh;
    margin: 0;
  }
  .card {
    background: #262b36;
    padding: 40px 35px;
    border-radius: 12px;
    box-shadow: 0 8px 30px rgba(220, 53, 69, 0.3); /* රතු පැහැති shadow එකක් */
    width: 320px;
    text-align: center;
    border: 2px solid #dc3545;
  }
  .icon {
    font-size: 55px;
    margin-bottom: 15px;
  }
  h1 {
    font-size: 22px;
    color: #dc3545;
    margin-bottom: 10px;
    letter-spacing: 1px;
  }
  p {
    color: #bbb;
    font-size: 14px;
    line-height: 1.6;
  }
  .highlight {
    color: #fff;
    font-weight: bold;
  }
</style>
</head>
<body>
  <div class="card">
    <div class="icon">&#128274;</div>
    <h1>SYSTEM LOCKED</h1>
    <p>Maximum failed login attempts reached.</p>
    <p>Please <span class="highlight">Restart the ESP32</span> to restore access.</p>
  </div>
</body>
</html>
)rawliteral";

void handleRootRequest(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.print(htmlPage);
}

void handleLoginRequest(WiFiClient& client, String request) {
  String json;

  if (IS_LOCKED) {
    client.print(lockedHtmlPage);
    json = buildJsonResponse(false, "SYSTEM_LOCKED", "System is locked. Restart ESP32 to reset.", FAILED_ATTEMPTS, true);

  } else {

    String username = urlDecode(getValue(request, "username="));
    String password = urlDecode(getValue(request, "password="));

    if (username.equals(VALID_USERNAME) && password.equals(VALID_PASSWORD)) {

      // Successful Login

      FAILED_ATTEMPTS = 0;
      setSuccessState();
      json = buildJsonResponse(true, "ACCESS_GRANTED", "Access Granted", 0, false);

    } else {

      // Failed Login

      FAILED_ATTEMPTS++;

      if (FAILED_ATTEMPTS >= MAX_ATTEMPTS) {
        IS_LOCKED = true;
        setLockedState();
        json = buildJsonResponse(false, "SYSTEM_LOCKED", "Too many failed attempts. System locked.", FAILED_ATTEMPTS, true);

      } else {
        setFailState(FAILED_ATTEMPTS);
        json = buildJsonResponse(false, "ACCESS_DENIED", "Access Denied", FAILED_ATTEMPTS, false);
      }
    }
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.print(json);
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

  WiFiClient client = server.available();

  if (client) {

    String request = client.readStringUntil('\r');

    while (client.available()) {
      client.read();
    }

    if (request.indexOf("GET /login") != -1) {
      handleLoginRequest(client, request);
    } else {
      handleRootRequest(client);
    }

    delay(1);
    client.stop();
  }

  if (IS_LOCKED) {
    handleLockedAlarm();
  }
}
