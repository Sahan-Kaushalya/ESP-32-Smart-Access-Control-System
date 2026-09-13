#include <ESPmDNS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

/*
  =====================================================================
  JIAT/AI - ARTIFICIAL INTELLIGENCE - PRACTICAL ASSESSMENT
  ESP32 SMART ACCESS CONTROL SYSTEM
  =====================================================================
*/


// Wifi + Server Configuration

const char* SSID = "XXXXX-XXXXX-XXXXXXX";
const char* WIFI_PASSWORD = "XXXXXXX";
const char* HOST_NAME = "smartaccess";
const char* LOGS_URL = "https://script.google.com/macros/s/XXXXXXXXXXXXXXXXXXXXXXX-XXXXXXXXXXXXXXXX/exec";

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


void sendLog(String ip, String status, int attempts, String message, String locked) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient http;
    message.replace(" ", "%20");

    String url = String(LOGS_URL) + "?ip=" + ip + "&status=" + status + "&attempts=" + String(attempts) + "&message=" + message + "&locked=" + locked;

    http.begin(secureClient, url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.println("Log saved to Google Sheets. Code: " + String(httpCode));
    } else {
      Serial.println("Error saving log: " + http.errorToString(httpCode));
    }

    http.end();
  }
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
    width: 340px;
    text-align: center;
    transition: all 0.3s ease;
  }
  h1 {
    font-size: 20px;
    margin-bottom: 20px;
    letter-spacing: 1px;
  }
  input {
    width: 100%;
    padding: 12px;
    margin: 8px 0;
    border-radius: 6px;
    border: 1px solid #444;
    background: #1c1f26;
    color: #eee;
    box-sizing: border-box;
  }
  input:focus {
    outline: none;
    border-color: #3a7bd5;
  }
  input:disabled {
    opacity: 0.5;
  }
  button {
    width: 100%;
    padding: 12px;
    margin-top: 10px;
    border: none;
    border-radius: 6px;
    background: #3a7bd5;
    color: white;
    font-weight: bold;
    font-size: 15px;
    cursor: pointer;
    transition: background 0.3s;
  }
  button:hover { background: #2b5ca8; }
  button:disabled {
    background: #555;
    cursor: not-allowed;
  }
  
  /* Status Area Styles */
  #statusArea {
    margin-top: 18px;
    padding: 10px;
    border-radius: 6px;
    background: #1c1f26;
    font-size: 15px;
    font-weight: bold;
  }
  #attemptsArea {
    margin-top: 10px;
    font-size: 13px;
    color: #bbb;
  }

  /* Utility Class to Hide Elements */
  .hidden { display: none !important; }

  /* Success Dashboard Styles */
  #successSection {
    padding: 20px 0;
  }
  .success-icon {
    font-size: 60px;
    margin-bottom: 15px;
    color: #4ade80;
  }
  
  /* JSON Console Styles */
  .json-wrapper {
    margin-top: 25px;
    text-align: left;
    border-top: 1px solid #444;
    padding-top: 15px;
  }
  .json-title {
    font-size: 11px;
    color: #8b949e;
    margin-bottom: 8px;
    text-transform: uppercase;
    letter-spacing: 1px;
  }
  .json-console {
    background: #0d1117;
    color: #a5d6ff;
    padding: 15px;
    border-radius: 6px;
    font-family: 'Courier New', Courier, monospace;
    font-size: 10px;
    white-space: pre-wrap;
    word-wrap: break-word;
    border: 1px solid #30363d;
  }
</style>
</head>
<body>
  <div class="card">
    
    <!-- 1. LOGIN SECTION -->
    <div id="loginSection">
      <h1>SMART ACCESS CONTROL</h1>
      <input type="text" id="username" placeholder="Username" autocomplete="off">
      <input type="password" id="password" placeholder="Password" autocomplete="off">
      <button id="loginBtn" onclick="doLogin()">Login</button>
      <div id="statusArea" style="color: #eab308;">&#128993; System Ready</div>
      <div id="attemptsArea">Failed Attempts: 0 / 3</div>
    </div>

    <!-- 2. SUCCESS SECTION (Hidden by default) -->
    <div id="successSection" class="hidden">
      <div class="success-icon">&#10004;&#65039;</div>
      <h2 style="color: #4ade80; margin: 10px 0;">Access Granted</h2>
      <p style="color: #bbb; font-size: 14px;">Welcome to the secure dashboard.</p>
      <button onclick="window.location.href='/logout'" style="background: #444; margin-top: 20px;">&#128257; Logout / Reset</button>
    </div>

    <!-- 3. JSON RESPONSE CONSOLE (Hidden until first request) -->
    <div id="jsonWrapper" class="json-wrapper hidden">
      <div class="json-title">Server JSON Payload:</div>
      <div id="jsonOutput" class="json-console"></div>
    </div>

  </div>

<script>
function doLogin() {
  var user = document.getElementById('username').value.trim();
  var pass = document.getElementById('password').value.trim();
  var statusArea = document.getElementById('statusArea');

  if (user === "" || pass === "") {
    statusArea.innerHTML = "&#9888; Enter username & password";
    statusArea.style.color = "#facc15";
    return;
  }

  statusArea.innerHTML = "&#8987; Authenticating...";
  statusArea.style.color = "#bbb";

  var url = "/login?username=" + encodeURIComponent(user) + "&password=" + encodeURIComponent(pass);

  fetch(url)
    .then(function(res) { return res.json(); })
    .then(function(data) { updateUI(data); })
    .catch(function(err) {
      statusArea.innerHTML = "&#9888; Connection Error";
      statusArea.style.color = "#ef4444";
    });
}

function updateUI(data) {
  var loginSection = document.getElementById('loginSection');
  var successSection = document.getElementById('successSection');
  var statusArea = document.getElementById('statusArea');
  var attemptsArea = document.getElementById('attemptsArea');
  var jsonWrapper = document.getElementById('jsonWrapper');
  var jsonOutput = document.getElementById('jsonOutput');

  // Print Raw JSON to the UI 
  jsonWrapper.classList.remove('hidden');
  jsonOutput.textContent = JSON.stringify(data, null, 2);

  // Handle Logic based on JSON response
  if (data.success) {
    // Hide login, Show success dashboard
    loginSection.classList.add('hidden');
    successSection.classList.remove('hidden');
  } 
  else {
    // Update attempts counter
    attemptsArea.innerHTML = "Failed Attempts: " + data.attempts + " / 3";

    if (data.locked) {
      statusArea.innerHTML = "&#128274; SYSTEM LOCKED";
      statusArea.style.color = "#ef4444";
      document.getElementById('username').disabled = true;
      document.getElementById('password').disabled = true;
      document.getElementById('loginBtn').disabled = true;
    } else {
      statusArea.innerHTML = "&#128308; Access Denied";
      statusArea.style.color = "#ef4444";
    }
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
    padding: 30px 35px;
    border-radius: 12px;
    box-shadow: 0 8px 30px rgba(239, 68, 68, 0.25);
    width: 340px;
    text-align: center;
    border: 1px solid #ef4444;
  }
  .icon {
    font-size: 60px;
    margin-bottom: 15px;
  }
  h1 {
    font-size: 20px;
    color: #ef4444;
    margin-bottom: 15px;
    letter-spacing: 1px;
  }
  p {
    color: #bbb;
    font-size: 14px;
    line-height: 1.6;
    margin: 8px 0;
  }
  .highlight {
    color: #fff;
    font-weight: bold;
  }
  
  .json-wrapper {
    margin-top: 25px;
    text-align: left;
    border-top: 1px solid #444;
    padding-top: 15px;
  }
  .json-title {
    font-size: 11px;
    color: #8b949e;
    margin-bottom: 8px;
    text-transform: uppercase;
    letter-spacing: 1px;
  }
  .json-console {
    background: #0d1117;
    color: #a5d6ff;
    padding: 15px;
    border-radius: 6px;
    font-family: 'Courier New', Courier, monospace;
    font-size: 10px;
    white-space: pre-wrap;
    word-wrap: break-word;
    border: 1px solid #30363d;
  }
</style>
</head>
<body>
  <div class="card">
    <div class="icon">&#128274;</div>
    <h1>SYSTEM LOCKED</h1>
    <p>Maximum failed login attempts reached.</p>
    <p>Please <span class="highlight">Restart the ESP32</span> to restore access.</p>
    
    <!-- JSON RESPONSE CONSOLE -->
    <div class="json-wrapper">
      <div class="json-title">Server JSON Payload:</div>
      <div class="json-console">{
  "success": false,
  "status": "SYSTEM_LOCKED",
  "message": "System is locked. Restart ESP32 to reset",
  "attempts": 3,
  "locked": true
}</div>
    </div>

  </div>
</body>
</html>
)rawliteral";

void handleRootRequest(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  if (IS_LOCKED) {
    client.print(lockedHtmlPage);
  } else {
    client.print(htmlPage);
  }
}

void handleLoginRequest(WiFiClient& client, String request) {

  String json;
  String clientIP = client.remoteIP().toString();
  String logStatus, logMessage, logLocked;
  int logAttempts = FAILED_ATTEMPTS;

  if (IS_LOCKED) {
    json = buildJsonResponse(false, "SYSTEM_LOCKED", "System is locked. Restart ESP32 to reset.", FAILED_ATTEMPTS, true);
    logStatus = "SYSTEM_LOCKED";
    logMessage = "Account is currently locked";
    logLocked = "True";
    Serial.println("System: SYSTEM LOCKED - Account is currently locked.");
  } else {

    String username = urlDecode(getValue(request, "username="));
    String password = urlDecode(getValue(request, "password="));

    if (username.equals(VALID_USERNAME) && password.equals(VALID_PASSWORD)) {

      // Successful Login

      FAILED_ATTEMPTS = 0;
      setSuccessState();
      json = buildJsonResponse(true, "ACCESS_GRANTED", "Access Granted", 0, false);
      logStatus = "ACCESS_GRANTED";
      logAttempts = 0;
      logMessage = "Successful login";
      logLocked = "False";
      Serial.println("System: ACCESS GRANTED - Login successful.");

    } else {

      // Failed Login

      FAILED_ATTEMPTS++;
      logAttempts = FAILED_ATTEMPTS;

      if (FAILED_ATTEMPTS >= MAX_ATTEMPTS) {
        IS_LOCKED = true;
        setLockedState();
        json = buildJsonResponse(false, "SYSTEM_LOCKED", "Too many failed attempts. System locked.", FAILED_ATTEMPTS, true);
        logStatus = "SYSTEM_LOCKED";
        logMessage = "Max attempts reached";
        logLocked = "True";
        Serial.println("System: SYSTEM LOCKED - Too many failed attempts. System locked.");

      } else {
        setFailState(FAILED_ATTEMPTS);
        json = buildJsonResponse(false, "ACCESS_DENIED", "Access Denied", FAILED_ATTEMPTS, false);
        logStatus = "ACCESS_DENIED";
        logMessage = "Invalid credentials used";
        logLocked = "False";
        Serial.println("System: ACCESS DENIED - Invalid credentials used");
      }
    }
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.print(json);

  client.stop();
  sendLog(clientIP, logStatus, logAttempts, logMessage, logLocked);
}

void handleLogoutRequest(WiFiClient& client) {
  String json;
  String clientIP = client.remoteIP().toString();
  String logStatus, logMessage, logLocked;
  int logAttempts = FAILED_ATTEMPTS;


  if (IS_LOCKED) {
    json = buildJsonResponse(false, "SYSTEM_LOCKED", "System is locked. Restart ESP32 to reset.", FAILED_ATTEMPTS, true);
    logStatus = "SYSTEM_LOCKED";
    logMessage = "Account is currently locked";
    logLocked = "True";
    Serial.println("System: System is locked. Restart ESP32 to reset.");

  } else {

    FAILED_ATTEMPTS = 0;

    setReadyState();
    json = buildJsonResponse(true, "LOGOUT", "Secure logout successful", FAILED_ATTEMPTS, false);
    logStatus = "LOGOUT";
    logMessage = "Secure logout successful";
    logLocked = "False";
    Serial.println("System: LOGOUT - Secure logout successful");
    Serial.println("System: Ready to proceed");
  }

  client.println("HTTP/1.1 302 Found");
  client.println("Location: /");
  client.println("Connection: close");
  client.print(json);

  client.stop();
  sendLog(clientIP, logStatus, logAttempts, logMessage, logLocked);
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
    } else if (request.indexOf("GET /logout") != -1) {
      handleLogoutRequest(client);
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
