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
<html lang="pt-br">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<style>
.container {
  display: flex;
  align-items: center;
  gap: 10px;
  width: 90%;
}

button {
  width: 40px;
  height: 40px;
  font-size: 20px;
}

#warning {
  color: red;
  font-weight: bold;
  margin-top: 10px;
}

#maximo {
  color: green;
  font-weight: bold;
  margin-top: 10px;
}

</style>

</head>
<body>

<h2>Gerador de Frequências</h2>

<!-- Frequência -->
Frequência: <span id="freqValue">10 Hz</span><br>

<div class="container">
  <button onclick="changeFreq(-1)">-</button>
  <input type="range" min="10" max="800" value="10" id="freq" style="width:100%">
  <button onclick="changeFreq(1)">+</button>
</div>

<br><br>

<!-- Amplitude -->
Amplitude: <span id="ampValue">39.4%</span><br>

<div class="container">
  <button onclick="changeAmp(-1)">-</button>
  <input type="range" min="0" max="127" value="50" id="amp" style="width:100%">
  <button onclick="changeAmp(1)">+</button>
</div>

<br><br>

<div id="warning"></div>
<div id="maximo"></div>

<script>

let freq = document.getElementById("freq");
let amp = document.getElementById("amp");

// Slider direto
freq.oninput = function(){
  updateFreq(this.value);
}

amp.oninput = function(){
  updateAmp(this.value);
}

// Atualiza frequência
function updateFreq(value){
  freq.value = value;
  document.getElementById("freqValue").innerHTML = value + " Hz";
  fetch("/set?freq=" + value);
}

// Atualiza amplitude (modo osciloscópio)
function updateAmp(value){
  amp.value = value;

  let percent = (value / 127) * 100;
  percent = percent.toFixed(1);

  // Tensão aproximada (DAC 0–3.3V)
/*
  let voltage = (value / 127) * 3.3;
  voltage = voltage.toFixed(2);
  
  document.getElementById("ampValue").innerHTML =
  value + " (" + percent + "% | " + voltage + " V)";
*/
  
  document.getElementById("ampValue").innerHTML = percent + "%";
  
  // Mensagem de aviso
  if(value == 0){
    document.getElementById("warning").innerHTML = "Tensão de saída em 0V, amplitude zerada. Impossível gerar frequência.";
    document.getElementById("maximo").innerHTML = "";
  } 
  else {
    if(value == 127){
      document.getElementById("maximo").innerHTML = "Tensão de saída em 3,3V. Amplitude máxima atingida.";
      document.getElementById("warning").innerHTML = "";
    }
    else {
      document.getElementById("warning").innerHTML = "";
      document.getElementById("maximo").innerHTML = "";
    }
  }
  
  fetch("/set?amp=" + value);
}

// Botões
function changeFreq(step){
  let newValue = parseInt(freq.value) + step;
  if(newValue >= freq.min && newValue <= freq.max){
    updateFreq(newValue);
  }
}

function changeAmp(step){
  let newValue = parseInt(amp.value) + step;
  if(newValue >= amp.min && newValue <= amp.max){
    updateAmp(newValue);
  }
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