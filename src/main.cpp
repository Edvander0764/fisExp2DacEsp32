#include <WiFi.h>
#include <WebServer.h>
#include <driver/dac.h>

const char* ssid = "FisExp_2";
const char* password = "12345678";

WebServer server(80);

#define DAC_PIN 25

volatile float frequency = 1000;
volatile int amplitude = 127;

const int samples = 100;
uint8_t sineTable[samples];

hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile int indexSine = 0;

void generateSineTable() {
  for (int i = 0; i < samples; i++) {
    float angle = (2 * PI * i) / samples;
    sineTable[i] = 127 + 127 * sin(angle);
  }
}

void IRAM_ATTR onTimer() {

  portENTER_CRITICAL_ISR(&timerMux);

  uint8_t value = sineTable[indexSine];
  value = (value - 127) * amplitude / 127 + 127;

  dac_output_voltage(DAC_CHANNEL_1, value);

  indexSine++;
  if (indexSine >= samples) indexSine = 0;

  portEXIT_CRITICAL_ISR(&timerMux);
}

void updateTimer() {

  float sampleRate = frequency * samples;
  float timerPeriod = 1000000.0 / sampleRate;

  timerAlarmWrite(timer, timerPeriod, true);
}

void handleRoot() {

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>

<h2>Gerador de Seno - ESP32</h2>

Frequência: <span id="freqValue">1000</span> Hz<br>
<input type="range" min="10" max="20000" value="1000" id="freq">

<br><br>

Amplitude: <span id="ampValue">127</span><br>
<input type="range" min="0" max="255" value="127" id="amp">

<script>

let freq = document.getElementById("freq");
let amp = document.getElementById("amp");

freq.oninput = function(){
document.getElementById("freqValue").innerHTML = this.value;
fetch("/set?freq=" + this.value);
}

amp.oninput = function(){
document.getElementById("ampValue").innerHTML = this.value;
fetch("/set?amp=" + this.value);
}

</script>

</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleSet() {

  if (server.hasArg("freq")) {
    frequency = server.arg("freq").toFloat();
    updateTimer();
  }

  if (server.hasArg("amp")) {
    amplitude = server.arg("amp").toInt();
  }

  server.send(200, "text/plain", "OK");
}

void setup() {

  dac_output_enable(DAC_CHANNEL_1);

  generateSineTable();

  WiFi.softAP(ssid, password);

  server.on("/", handleRoot);
  server.on("/set", handleSet);

  server.begin();

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);

  updateTimer();

  timerAlarmEnable(timer);
}

void loop() {
  server.handleClient();
}