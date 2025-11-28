#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// ==========================================================
//  MAPEOS DE PINES (según tu hardware)
// ==========================================================
// Motor 1 (izquierdo)
const int M1A  = 12;   // IN1
const int M1B  = 14;   // IN2
const int M1EN = 27;   // ENA (PWM)
// Motor 2 (derecho)
const int M2A  = 26;   // IN3
const int M2B  = 25;   // IN4
const int M2EN = 33;   // ENB (PWM)

// LEDs (controlados desde la web)
const int LED1 = 2;
const int LED2 = 0;    // ojo: GPIO0 es pin de arranque
const int LED3 = 16;
const int LED4 = 17;

// ==========================================================
//  WiFi (ajusta tus credenciales)
// ==========================================================
const char* WIFI_SSID = "Delga";
const char* WIFI_PASS = "Delga1213";

// ==========================================================
//  FreeRTOS: Tareas (no usamos punteros; solo los handles)
// ==========================================================
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

// Servidor HTTP
WebServer server(80);

// Estado compartido (simple y directo; sin semáforos)
String lastCommand = "—";

// PWM (ESP32: ledc)
const int PWM_FREQ = 15000;  // 15 kHz
const int PWM_RES  = 8;      // 8 bits (0..255)
const int CH_M1EN  = 0;
const int CH_M2EN  = 1;

// Velocidad actual (duty 0..255)
volatile uint8_t g_speedDuty = 200;

// -----------------------------------------------------------
//  Prototipos
// -----------------------------------------------------------
void Task1code(void *pvParameters);  // parámetro exigido por FreeRTOS (no se usa)
void Task2code(void *pvParameters);
void Task3code(void *pvParameters);

void setupPins();
void setupWiFi();
void setupHTTP();
void setupTasks();
void delayRTOS(uint32_t ms);

void robotStop();
void robotForward();
void robotBackward();
void robotLeft();
void robotRight();

void setLastCommand(const String& cmd);
String getLastCommand();

void handleRoot();
void handleCmd();    // /cmd?c=F|B|L|R|S
void handleLed();    // /led?n=1..4&v=0|1
void handleLast();   // /last
void handleSpeed();  // /speed[?v=0..255]

// -----------------------------------------------------------
//  HTML con SLIDER PWM
// -----------------------------------------------------------
const char* PAGE_HTML = R"HTML(
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ESP32 Robot Control</title>
<style>
  body{font-family:system-ui,Segoe UI,Arial,sans-serif;margin:18px;}
  h1{font-size:1.2rem;margin:0 0 12px}
  .grid{display:grid;gap:10px;grid-template-columns:repeat(3, minmax(90px,1fr));max-width:520px}
  button{padding:12px 10px;font-size:1rem;cursor:pointer;border-radius:10px;border:1px solid #ccc}
  .row{display:flex;gap:10px;margin-top:10px;flex-wrap:wrap}
  .status{margin-top:14px;padding:10px;border:1px solid #ddd;border-radius:8px}
  .card{margin-top:14px;padding:12px;border:1px solid #ddd;border-radius:8px;max-width:520px}
  input[type=range]{width:100%}
  .right{margin-left:auto}
</style>
</head>
<body>
  <h1>Control del Robot (ESP32)</h1>

  <!-- Bloque: Control de velocidad PWM -->
  <div class="card">
    <b>Velocidad (PWM):</b>
    <div style="display:flex;gap:10px;align-items:center">
      <input id="slider" type="range" min="0" max="255" step="1" value="200"
             oninput="onSlide(this.value)"/>
      <span id="speedLabel" class="right">~78%</span>
    </div>
    <small>Consejo: baja la velocidad al 40–60% para giros más controlados.</small>
  </div>

  <div class="grid" style="margin-top:12px">
    <span></span>
    <button onclick="sendCmd('F')">⬆️ Adelante</button>
    <span></span>

    <button onclick="sendCmd('L')">⬅️ Izquierda</button>
    <button onclick="sendCmd('S')">⛔ STOP</button>
    <button onclick="sendCmd('R')">➡️ Derecha</button>

    <span></span>
    <button onclick="sendCmd('B')">⬇️ Atrás</button>
    <span></span>
  </div>

  <div class="row">
    <button onclick="setLed(1,1)">LED1 ON</button>
    <button onclick="setLed(1,0)">LED1 OFF</button>
    <button onclick="setLed(2,1)">LED2 ON</button>
    <button onclick="setLed(2,0)">LED2 OFF</button>
    <button onclick="setLed(3,1)">LED3 ON</button>
    <button onclick="setLed(3,0)">LED3 OFF</button>
    <button onclick="setLed(4,1)">LED4 ON</button>
    <button onclick="setLed(4,0)">LED4 OFF</button>
  </div>

  <div class="status">
    <b>Último comando:</b>
    <span id="last">—</span>
  </div>

<script>
function pctFromDuty(d){ return Math.round((d/255)*100); }

async function sendCmd(c){
  try { await fetch('/cmd?c='+encodeURIComponent(c)); refreshLast(); }
  catch(e){ console.log(e); }
}
async function setLed(n,v){
  try { await fetch('/led?n='+n+'&v='+v); }
  catch(e){ console.log(e); }
}
async function refreshLast(){
  try{
    const r = await fetch('/last');
    const t = await r.text();
    document.getElementById('last').textContent = t || '—';
  }catch(e){ console.log(e); }
}
async function refreshSpeed(){
  try{
    const r = await fetch('/speed');
    const t = await r.text();
    const d = parseInt(t)||0;
    document.getElementById('slider').value = d;
    document.getElementById('speedLabel').textContent = pctFromDuty(d)+'%';
  }catch(e){ console.log(e); }
}
let slideTimer=null;
function onSlide(v){
  document.getElementById('speedLabel').textContent = pctFromDuty(v)+'%';
  if (slideTimer) clearTimeout(slideTimer);
  slideTimer = setTimeout(async ()=>{
    try{ await fetch('/speed?v='+v); }catch(e){ console.log(e); }
  }, 120);
}
setInterval(refreshLast, 1000);
refreshLast();
refreshSpeed();
</script>
</body>
</html>
)HTML";

// ========================== SETUP ==========================
void setup()
{
  Serial.begin(115200);
  delay(500);

  setupPins();
  setupWiFi();
  setupHTTP();
  setupTasks();
}

void loop(){}

// -----------------------------------------------------------
//  Tareas (signatura con puntero exigida por FreeRTOS; no usada)
// -----------------------------------------------------------
void setupTasks()
{
  xTaskCreatePinnedToCore(Task1code, "Tarea_Principal", 5000, NULL, 1, &Task1, 0);
  delay(200);
  xTaskCreatePinnedToCore(Task2code, "Tarea_HTTP",     6000, NULL, 1, &Task2, 1);
  delay(200);
  xTaskCreatePinnedToCore(Task3code, "Tarea_Log",      3000, NULL, 1, &Task3, 1);
  delay(200);
}

// -----------------------------------------------------------
//  Pines y PWM
// -----------------------------------------------------------
void setupPins()
{
  // Direcciones motor
  pinMode(M1A, OUTPUT);
  pinMode(M1B, OUTPUT);
  pinMode(M2A, OUTPUT);
  pinMode(M2B, OUTPUT);

  // Habilitadores (PWM)
  ledcSetup(CH_M1EN, PWM_FREQ, PWM_RES);
  ledcAttachPin(M1EN, CH_M1EN);
  ledcSetup(CH_M2EN, PWM_FREQ, PWM_RES);
  ledcAttachPin(M2EN, CH_M2EN);

  // LEDs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);

  // Estado seguro al inicio
  robotStop();
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
}

// -----------------------------------------------------------
//  WiFi
// -----------------------------------------------------------
void setupWiFi()
{
  Serial.println("\nConectando a WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(250);
    Serial.print(".");
    retries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Conectado. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Fallo STA. Activando AP 'ESP32-ROBOT' (sin clave)...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-ROBOT");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

// -----------------------------------------------------------
//  HTTP routing
// -----------------------------------------------------------
void setupHTTP()
{
  server.on("/",       HTTP_GET, handleRoot);
  server.on("/cmd",    HTTP_GET, handleCmd);
  server.on("/led",    HTTP_GET, handleLed);
  server.on("/last",   HTTP_GET, handleLast);
  server.on("/speed",  HTTP_GET, handleSpeed);
  server.begin();
  Serial.println("Servidor HTTP listo.");
}

void handleRoot()  { server.send(200, "text/html; charset=utf-8", PAGE_HTML); }

void handleCmd()
{
  if (!server.hasArg("c")) { server.send(400, "text/plain", "Falta parametro c"); return; }
  String c = server.arg("c"); c.trim(); c.toUpperCase();

  if      (c == "F") { robotForward();  setLastCommand("Adelante");  }
  else if (c == "B") { robotBackward(); setLastCommand("Atrás");     }
  else if (c == "L") { robotLeft();     setLastCommand("Izquierda"); }
  else if (c == "R") { robotRight();    setLastCommand("Derecha");   }
  else if (c == "S") { robotStop();     setLastCommand("STOP");      }
  else { server.send(400, "text/plain", "Comando invalido"); return; }

  server.send(200, "text/plain", "OK");
}

void handleLed()
{
  if (!server.hasArg("n") || !server.hasArg("v")) {
    server.send(400, "text/plain", "Faltan parametros n y/o v");
    return;
  }
  int n = server.arg("n").toInt();
  int v = server.arg("v").toInt();
  int pin = -1;
  if      (n == 1) pin = LED1;
  else if (n == 2) pin = LED2;
  else if (n == 3) pin = LED3;
  else if (n == 4) pin = LED4;

  if (pin < 0 || (v != 0 && v != 1)) { server.send(400, "text/plain", "Parametros invalidos"); return; }
  digitalWrite(pin, v ? HIGH : LOW);
  server.send(200, "text/plain", "OK");
}

void handleLast() { server.send(200, "text/plain; charset=utf-8", getLastCommand()); }

// /speed           -> devuelve duty actual (0..255)
// /speed?v=0..255  -> fija nuevo duty para ambos motores
void handleSpeed()
{
  if (!server.hasArg("v")) { server.send(200, "text/plain", String(g_speedDuty)); return; }
  int v = server.arg("v").toInt();
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  g_speedDuty = (uint8_t)v;
  server.send(200, "text/plain", "OK");
}

// -----------------------------------------------------------
//  Movimiento (usa g_speedDuty en EN)
// -----------------------------------------------------------
static inline void enableMotors(uint8_t m1_duty, uint8_t m2_duty)
{
  ledcWrite(CH_M1EN, m1_duty);
  ledcWrite(CH_M2EN, m2_duty);
}

void robotStop()
{
  enableMotors(0, 0);
  digitalWrite(M1A, LOW); digitalWrite(M1B, LOW);
  digitalWrite(M2A, LOW); digitalWrite(M2B, LOW);
}

void robotForward()
{
  digitalWrite(M1A, HIGH); digitalWrite(M1B, LOW);   // M1 adelante
  digitalWrite(M2A, HIGH); digitalWrite(M2B, LOW);   // M2 adelante
  enableMotors(g_speedDuty, g_speedDuty);
}

void robotBackward()
{
  digitalWrite(M1A, LOW);  digitalWrite(M1B, HIGH);  // M1 atrás
  digitalWrite(M2A, LOW);  digitalWrite(M2B, HIGH);  // M2 atrás
  enableMotors(g_speedDuty, g_speedDuty);
}

void robotRight()
{
  // Giro en sitio a la izquierda: M1 atrás, M2 adelante
  digitalWrite(M1A, LOW);  digitalWrite(M1B, HIGH);  // M1 atrás
  digitalWrite(M2A, HIGH); digitalWrite(M2B, LOW);   // M2 adelante
  enableMotors(g_speedDuty, g_speedDuty);
}

void robotLeft()
{
  // Giro en sitio a la derecha: M1 adelante, M2 atrás
  digitalWrite(M1A, HIGH); digitalWrite(M1B, LOW);   // M1 adelante
  digitalWrite(M2A, LOW);  digitalWrite(M2B, HIGH);  // M2 atrás
  enableMotors(g_speedDuty, g_speedDuty);
}


// -----------------------------------------------------------
//  Último comando (sin semáforos; simple y didáctico)
// -----------------------------------------------------------
void setLastCommand(const String& cmd){ lastCommand = cmd; }
String getLastCommand(){ return String(lastCommand); }

// -----------------------------------------------------------
//  Utilidad FreeRTOS (cede CPU)
// -----------------------------------------------------------
void delayRTOS(uint32_t ms){ vTaskDelay(ms / portTICK_PERIOD_MS); }

// ======================= TAREA 1 ==========================
//  Timeout de seguridad: se detiene si no hay comandos nuevos
// ==========================================================
void Task1code(void *pvParameters)
{
  Serial.print("Tarea 1 corriendo en el core ");
  Serial.println(xPortGetCoreID());

  const uint32_t TIMEOUT_MS = 1000; // 3 s (ajusta a gusto)
  uint32_t lastSeen = millis();
  String   prev = getLastCommand();

  for(;;){
    String nowCmd = getLastCommand();
    if (nowCmd != prev) { prev = nowCmd; lastSeen = millis(); }

    if (millis() - lastSeen > TIMEOUT_MS && nowCmd != "STOP") {
      robotStop();
      setLastCommand("STOP (timeout)");
      prev = "STOP (timeout)";
      lastSeen = millis();
      Serial.println("[Guardia] Timeout: robot detenido.");
    }
    delayRTOS(50);
  }
}

// ======================= TAREA 2 ==========================
//  Atiende el servidor HTTP
// ==========================================================
void Task2code(void *pvParameters)
{
  Serial.print("Tarea 2 corriendo en el core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    server.handleClient();
    delayRTOS(1);
  }
}

// ======================= TAREA 3 ==========================
//  Solo logs eventuales (sin latido)
// ==========================================================
void Task3code(void *pvParameters)
{
  Serial.print("Tarea 3 corriendo en el core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    static uint32_t t0 = millis();
    if (millis() - t0 > 5000) {
      t0 = millis();
      Serial.print("[Estado] Último comando: ");
      Serial.println(getLastCommand());
      Serial.print("Velocidad (duty): ");
      Serial.println(g_speedDuty);
      Serial.print("IP: "); Serial.println((WiFi.getMode()==WIFI_AP)? WiFi.softAPIP() : WiFi.localIP());
    }
    delayRTOS(500);
  }
}
