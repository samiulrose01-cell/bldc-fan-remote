/*
  BLDC Fan ESP32 — MQTT + Local Home Wi-Fi Remote
  ------------------------------------------------
  MQTT command topic: bldc-fan/7Kx92LmP/command
  MQTT state topic:   bldc-fan/7Kx92LmP/state

  Local remote: http://fan-esp32.local/

  IMPORTANT:
  - Put your real Wi-Fi/MQTT credentials below.
  - Do NOT connect GPIO15 directly to the fan board's 4.3/5V line.
  - Keep the IR transmitter circuit that was already working.
  - This sketch never sends ON automatically during boot.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

// ================= USER CONFIG =================
const char* WIFI_SSID = "YOUR_HOME_WIFI";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* MQTT_HOST = "0f361fa0238547348bd1826499d0e794.s1.eu.hivemq.cloud";
const uint16_t MQTT_PORT = 8883;
const char* MQTT_USER = "YOUR_MQTT_USERNAME";
const char* MQTT_PASSWORD = "YOUR_MQTT_PASSWORD";

const char* DEVICE_NAME = "fan-esp32";
const char* CMD_TOPIC = "bldc-fan/7Kx92LmP/command";
const char* STATE_TOPIC = "bldc-fan/7Kx92LmP/state";

// Use the GPIO from your already-working IR transmitter circuit.
const uint16_t IR_LED_PIN = 15;

// NEC commands from your working fan remote.
const uint8_t NEC_ADDRESS = 0x00;
const uint8_t CMD_ON   = 0x43;
const uint8_t CMD_OFF  = 0x0D;
const uint8_t CMD_FULL = 0x1C;
const uint8_t CMD_S1   = 0x41;
const uint8_t CMD_S2   = 0x42;

// These are placeholders for speeds whose exact NEC commands must match
// your previously captured remote. Replace them if your fan uses different
// commands. Do not guess commands for an unknown fan.
const uint8_t CMD_S3 = 0x40;
const uint8_t CMD_S4 = 0x3F;
const uint8_t CMD_S5 = 0x3E;
const uint8_t CMD_S6 = 0x3D;
// =================================================

WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);
WebServer server(80);
IRsend irsend(IR_LED_PIN);

String fanState = "OFF";
unsigned long lastMqttAttempt = 0;
unsigned long lastWifiCheck = 0;

void publishState() {
  if (mqtt.connected()) mqtt.publish(STATE_TOPIC, fanState.c_str(), true);
}

void sendNEC(uint8_t command) {
  irsend.sendNEC((uint16_t)NEC_ADDRESS, command, 2);
  delay(60);
}

void applyCommand(String c, bool publish = true) {
  c.trim();
  c.toUpperCase();

  if (c == "ON") {
    sendNEC(CMD_ON);
    fanState = "ON";
  } else if (c == "OFF") {
    sendNEC(CMD_OFF);
    fanState = "OFF";
  } else if (c == "FULL") {
    sendNEC(CMD_FULL);
    fanState = "FULL";
  } else if (c == "S1") {
    sendNEC(CMD_S1);
    fanState = "SPEED 1";
  } else if (c == "S2") {
    sendNEC(CMD_S2);
    fanState = "SPEED 2";
  } else if (c == "S3") {
    sendNEC(CMD_S3);
    fanState = "SPEED 3";
  } else if (c == "S4") {
    sendNEC(CMD_S4);
    fanState = "SPEED 4";
  } else if (c == "S5") {
    sendNEC(CMD_S5);
    fanState = "SPEED 5";
  } else if (c == "S6") {
    sendNEC(CMD_S6);
    fanState = "SPEED 6";
  } else {
    return;
  }

  if (publish) publishState();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String c;
  for (unsigned int i = 0; i < length; i++) c += (char)payload[i];
  if (String(topic) == CMD_TOPIC) applyCommand(c, true);
}

bool connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (mqtt.connected()) return true;

  if (millis() - lastMqttAttempt < 5000) return false;
  lastMqttAttempt = millis();

  String clientId = String(DEVICE_NAME) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
    mqtt.subscribe(CMD_TOPIC);
    publishState();
    return true;
  }
  return false;
}

const char HOME_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html><head>
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#07111f">
<title>BLDC Fan — Home</title>
<style>
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 50% 0,#17345b,#07111f 45%,#030914);color:#fff;font-family:system-ui,sans-serif}
main{max-width:460px;margin:auto;padding:18px 15px}h1{text-align:center;margin:4px 0}p{text-align:center;color:#91a0b8}
.card{background:#101d30ee;border:1px solid #263957;border-radius:25px;padding:18px;margin:12px 0}
.status{padding:12px;border-radius:14px;text-align:center;background:#0d2b21;color:#bbf7d0;font-weight:800}
.fan{text-align:center;font-size:110px;margin:10px}.run{animation:spin .8s linear infinite}@keyframes spin{to{transform:rotate(360deg)}}
.state{text-align:center;font-size:27px;font-weight:900}.grid2{display:grid;grid-template-columns:1fr 1fr;gap:11px}.grid3{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
button{border:0;min-height:62px;border-radius:18px;color:#fff;font-size:18px;font-weight:900;margin-top:10px}.on{background:#16a34a}.off{background:#dc2626}.sp{background:#2563eb}.full{background:#ea580c;width:100%}
</style></head><body><main>
<h1>🌀 BLDC Fan</h1><p>Home Wi-Fi Remote</p>
<div class="card"><div class="status">🏠 LOCAL • ESP32</div></div>
<div class="card"><div id="fan" class="fan">✣</div><div id="state" class="state">OFF</div></div>
<div class="card"><div class="grid2"><button class="on" onclick="go('ON')">ON</button><button class="off" onclick="go('OFF')">OFF</button></div></div>
<div class="card"><div class="grid3">
<button class="sp" onclick="go('S1')">1</button><button class="sp" onclick="go('S2')">2</button><button class="sp" onclick="go('S3')">3</button>
<button class="sp" onclick="go('S4')">4</button><button class="sp" onclick="go('S5')">5</button><button class="sp" onclick="go('S6')">6</button>
</div><button class="full" onclick="go('FULL')">FULL POWER</button></div>
<script>
let state="OFF";
function draw(s){state=s;document.getElementById("state").textContent=s;document.getElementById("fan").className="fan"+(s==="OFF"?"":" run")}
async function go(c){
 try{
   let r=await fetch("/api/command",{method:"POST",headers:{"Content-Type":"text/plain"},body:c});
   let j=await r.json(); if(j.state) draw(j.state);
 }catch(e){alert("ESP32 not reachable")}
}
async function poll(){
 try{let r=await fetch("/api/state");let j=await r.json();if(j.state)draw(j.state)}catch(e){}
}
setInterval(poll,1500);poll();
</script></main></body></html>
)HTML";

void handleHome() {
  server.send_P(200, "text/html", HOME_PAGE);
}

void handleState() {
  String json = "{\"state\":\"" + fanState + "\"}";
  server.send(200, "application/json", json);
}

void handleCommand() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"missing command\"}");
    return;
  }
  String c = server.arg("plain");
  String before = fanState;
  applyCommand(c, true);
  if (before == fanState && c != "OFF" && c != "ON" && c != "FULL" &&
      !c.startsWith("S")) {
    server.send(400, "application/json", "{\"error\":\"invalid command\"}");
    return;
  }
  server.send(200, "application/json", "{\"state\":\"" + fanState + "\"}");
}

void setupWeb() {
  server.on("/", HTTP_GET, handleHome);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/command", HTTP_POST, handleCommand);
  server.begin();
}

void setup() {
  Serial.begin(115200);

  // Critical: initialize the IR transmitter but DO NOT send an ON command.
  irsend.begin();

  // Initial logical state is OFF only as a safe UI assumption.
  // This does not transmit IR and does not physically force the fan off.
  fanState = "OFF";

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_NAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Wi-Fi connecting");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin(DEVICE_NAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS: http://fan-esp32.local/");
    }
  }

  // HiveMQ Cloud uses TLS.
  secureClient.setInsecure();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(256);

  setupWeb();
}

void loop() {
  server.handleClient();

  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) connectMQTT();
    if (mqtt.connected()) mqtt.loop();
  }

  if (millis() - lastWifiCheck > 10000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
}
