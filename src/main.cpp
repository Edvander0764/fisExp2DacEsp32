#include <WiFi.h>
#include <WebServer.h>
#include <driver/dac.h>

const char* ssid = "FisExp_2";
const char* password = "12345678";

WebServer server(80);

#define DAC_PIN 25 // DAC
#define LED_PIN 2 //LED

volatile float frequency = 10;
volatile int amplitude = 50;

const int samples = 100;
uint8_t sineTable[samples];
int valueFinal = 0;

hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile int indexSine = 0;

void generateSineTable() {
  for (int i = 0; i < samples; i++) {
    float angle = (2 * PI * i) / samples;
    sineTable[i] = 127 + 127 * sin(angle);
    //Serial.println(sineTable[i]);
  }
}

void IRAM_ATTR onTimer() {

  portENTER_CRITICAL_ISR(&timerMux);

  uint8_t value = sineTable[indexSine];
  value = ((value - 127) * amplitude / 127) + 127;
  valueFinal = value + 127;
  //Serial.println(value);

  dac_output_voltage(DAC_CHANNEL_1, value);

  indexSine++;
  if (indexSine >= samples) {
    indexSine = 0;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // alterna LED
  }

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
<html lang = "pt-br">
<head>
<meta charset = "utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>

<h2>Gerador de Onda - ESP32</h2>

Frequência: <span id="freqValue">10</span> Hz<br>
<input type="range" min="10" max="800" value="10" id="freq" style="width:90%">

<br><br>

Amplitude: <span id="ampValue">50</span><br>
<input type="range" min="0" max="127" value="50" id="amp" style="width:90%">

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

  //Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT); //LED
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