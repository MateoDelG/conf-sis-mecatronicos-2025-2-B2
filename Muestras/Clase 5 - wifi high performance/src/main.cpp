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
//  FreeRTOS: Tareas (solo handles para referenciarlas)
// ==========================================================
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

// Servidor HTTP
WebServer server(80);

// Estado simple y didáctico
String lastCommand = "—";

// PWM (ESP32: ledc)
const int PWM_FREQ = 15000;  // 15 kHz (silencioso para muchos drivers)
const int PWM_RES  = 8;      // 8 bits (0..255)
const int CH_M1EN  = 0;
const int CH_M2EN  = 1;

// Velocidad actual (duty 0..255)
uint8_t g_speedDuty = 200;

// -----------------------------------------------------------
//  Prototipos
// -----------------------------------------------------------
void Task1code(void *pvParameters);
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
void handleCmd();        // /cmd?c=F|B|L|R|S
void handleSpeed();      // /speed[?v=0..255]
void handleLedToggle();  // /ledToggle?n=1..4     -> alterna y devuelve 0/1
void handleLedStates();  // /ledStates            -> JSON con estados 4 LEDs
void handleLast();       // /last

// -----------------------------------------------------------
//  HTML + JS
//   - Botón único por LED (toggle con color/estado)
//   - Movimiento "hold-to-move": presionar -> F/B/L/R, soltar -> S
// -----------------------------------------------------------
const char* PAGE_HTML = R"HTML(
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ESP32 Robot Control</title>
<style>
  :root{
    --ok:#0a7; --off:#999; --btn:#f7f7f7; --bd:#ddd;
    --btn-size:64px;      /* tamaño base botones control */
    --btn-round:14px;     /* redondeo */
    --icon-size:36px;     /* tamaño icono en botones */
  }
  *{ user-select:none; -webkit-user-select:none; -ms-user-select:none; }
  body{font-family:system-ui,Segoe UI,Arial,sans-serif;margin:18px}
  h1{font-size:1.15rem;margin:0 0 12px}
  .row{display:flex;gap:10px;margin-top:10px;flex-wrap:wrap}
  .status,.card{margin-top:14px;padding:12px;border:1px solid var(--bd);border-radius:12px;max-width:760px}
  input[type=range]{width:100%}
  .right{margin-left:auto}
  .tag{display:inline-block;padding:2px 8px;border-radius:999px;border:1px solid var(--bd);font-size:.9rem}

  /* --------- Zona de controles (layout horizontal) --------- */
  .controls{
    display:flex;
    flex-wrap:wrap;
    gap:18px;
    justify-content:space-between;
    align-items:flex-start;
  }
  .ctrl-col{
    display:flex;
    flex-direction:column;
    gap:10px;
    min-width:150px;
  }
  .ctrl-title{
    font-weight:600;
    font-size:.95rem;
  }
  .ctrl-vertical{
    display:flex;
    flex-direction:column;
    gap:10px;
    align-items:center;
  }
  .ctrl-horizontal{
    display:flex;
    gap:10px;
    align-items:center;
    justify-content:center;
  }

  /* --------- Botones de control (sin texto dentro) --------- */
  .ctrl-btn{
    width:var(--btn-size);
    height:var(--btn-size);
    cursor:pointer;
    border-radius:var(--btn-round);
    border:1px solid var(--bd);
    background:#fff;
    display:inline-flex;
    align-items:center;
    justify-content:center;
    padding:0;
    outline:none;
    background-repeat:no-repeat;
    background-position:center;
    background-size:var(--icon-size);
  }
  .ctrl-btn:active{ transform:scale(0.97) }

  /* Flecha arriba (adelante) */
  .icon-up{
    background-image:url("data:image/svg+xml;utf8,\
      <svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='%23000'>\
        <path d='M4 14l8-8 8 8H4z'/>\
      </svg>");
  }
  /* Flecha abajo (atrás) */
  .icon-down{
    background-image:url("data:image/svg+xml;utf8,\
      <svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='%23000'>\
        <path d='M4 10l8 8 8-8H4z'/>\
      </svg>");
  }
  /* Flecha izquierda */
  .icon-left{
    background-image:url("data:image/svg+xml;utf8,\
      <svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='%23000'>\
        <path d='M14 4l-8 8 8 8V4z'/>\
      </svg>");
  }
  /* Flecha derecha */
  .icon-right{
    background-image:url("data:image/svg+xml;utf8,\
      <svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='%23000'>\
        <path d='M10 4l8 8-8 8V4z'/>\
      </svg>");
  }
  /* STOP (círculo + cuadrado) */
  .icon-stop{
    background-image:url("data:image/svg+xml;utf8,\
      <svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>\
        <circle cx='12' cy='12' r='9' fill='none' stroke='%23000' stroke-width='2'/>\
        <rect x='8' y='8' width='8' height='8' fill='%23000'/>\
      </svg>");
  }

  /* --------- LEDs (botón sin texto interno) --------- */
  .led{
    width:56px; height:56px;
    position:relative; background:linear-gradient(#fafafa,#f2f2f2);
    border:1px solid var(--bd); border-radius:var(--btn-round); cursor:pointer;
  }
  .led::after{
    content:attr(data-label);
    position:absolute; inset:auto 0 6px 0; text-align:center;
    font-size:.8rem; color:#333;
  }
  .led.led-on{
    background:radial-gradient(circle at 50% 40%, #c8ffe6 0%, #eafff7 60%, #f2fdf9 100%);
    border-color:#bde7d7;
    box-shadow:0 0 0 2px #c9f3e4 inset, 0 0 10px rgba(0,160,120,.25);
  }
</style>
</head>
<body oncontextmenu="return false">
  <h1>Control del Robot (ESP32)</h1>

  <!-- Velocidad PWM -->
  <div class="card">
    <b>Velocidad (PWM):</b>
    <div style="display:flex;gap:10px;align-items:center">
      <input id="slider" type="range" min="0" max="255" step="1" value="200" oninput="onSlide(this.value)"/>
      <span id="speedLabel" class="right">~78%</span>
    </div>
    <small>Consejo: usa 40–60% para giros más finos.</small>
  </div>

  <!-- Controles: Adelante/Atrás a la IZQUIERDA, Izq/Der a la DERECHA -->
  <div class="card controls">
    <!-- Columna izquierda: Adelante / Atrás -->
    <div class="ctrl-col">
      <div class="ctrl-title">Adelante / Atrás</div>
      <div class="ctrl-vertical">
        <button id="btnF" class="ctrl-btn icon-up"   data-cmd="F" aria-label="Adelante"  title="Adelante"></button>
        <button id="btnB" class="ctrl-btn icon-down" data-cmd="B" aria-label="Atrás"     title="Atrás"></button>
      </div>
    </div>

    <!-- Columna derecha: Izquierda / STOP / Derecha -->
    <div class="ctrl-col">
      <div class="ctrl-title">Izquierda / Derecha</div>
      <div class="ctrl-horizontal">
        <button id="btnL" class="ctrl-btn icon-left"  data-cmd="L" aria-label="Izquierda" title="Izquierda"></button>
        <button id="btnS" class="ctrl-btn icon-stop"        onclick="sendCmd('S')" aria-label="Stop"      title="Stop"></button>
        <button id="btnR" class="ctrl-btn icon-right" data-cmd="R" aria-label="Derecha"   title="Derecha"></button>
      </div>
    </div>
  </div>

  <!-- LEDs: un botón por LED (toggle) -->
  <div class="row">
    <button id="led1" class="led" data-label="LED 1" onclick="toggleLed(1)" aria-pressed="false" aria-label="LED1"></button>
    <button id="led2" class="led" data-label="LED 2" onclick="toggleLed(2)" aria-pressed="false" aria-label="LED2"></button>
    <button id="led3" class="led" data-label="LED 3" onclick="toggleLed(3)" aria-pressed="false" aria-label="LED3"></button>
    <button id="led4" class="led" data-label="LED 4" onclick="toggleLed(4)" aria-pressed="false" aria-label="LED4"></button>
    <span class="tag">Toca para alternar ON/OFF</span>
  </div>

  <div class="status">
    <b>Último comando:</b>
    <span id="last">—</span>
  </div>

<script>
/* ==================== UTIL ==================== */
const pctFromDuty = d => Math.round((d/255)*100);
const $ = id => document.getElementById(id);

/* ==================== COMANDOS ==================== */
async function sendCmd(c){
  try { await fetch('/cmd?c='+encodeURIComponent(c)); refreshLast(); }
  catch(e){ console.log(e); }
}

/* Hold-to-move:
   - pointerdown -> envía F/B/L/R
   - pointerup/leave/cancel -> envía S
*/
function bindHold(btn){
  const cmd = btn.dataset.cmd;
  let down = false;

  const onDown = ev => { down = true; ev.preventDefault(); sendCmd(cmd); };
  const onUp   = ev => { if(!down) return; down=false; ev.preventDefault(); sendCmd('S'); };

  btn.addEventListener('pointerdown', onDown);
  btn.addEventListener('pointerup', onUp);
  btn.addEventListener('pointerleave', onUp);
  btn.addEventListener('pointercancel', onUp);
}

/* ==================== LEDS (toggle) ==================== */
function paintLedBtn(id, on){
  const b = $('led'+id);
  if(!b) return;
  b.classList.toggle('led-on', !!on);
  b.setAttribute('aria-pressed', on ? 'true' : 'false');
  b.title = 'LED'+id + (on ? ' (ON)' : ' (OFF)');
}

// Un solo endpoint que alterna y devuelve "0" o "1"
async function toggleLed(n){
  try{
    const r = await fetch('/ledToggle?n='+n);
    const t = await r.text();
    paintLedBtn(n, t.trim()==='1');
  }catch(e){ console.log(e); }
}

async function refreshLedStates(){
  try{
    const r = await fetch('/ledStates');
    const j = await r.json();
    [1,2,3,4].forEach(i => paintLedBtn(i, j['L'+i]===1));
  }catch(e){ console.log(e); }
}

/* ==================== SPEED ==================== */
let slideTimer=null;
function onSlide(v){
  $('speedLabel').textContent = pctFromDuty(v)+'%';
  if (slideTimer) clearTimeout(slideTimer);
  slideTimer = setTimeout(async ()=>{
    try{ await fetch('/speed?v='+v); }catch(e){ console.log(e); }
  }, 120);
}
async function refreshSpeed(){
  try{
    const r = await fetch('/speed');
    const t = await r.text();
    const d = parseInt(t)||0;
    $('slider').value = d;
    $('speedLabel').textContent = pctFromDuty(d)+'%';
  }catch(e){ console.log(e); }
}

/* ==================== LAST ==================== */
async function refreshLast(){
  try{
    const r = await fetch('/last');
    $('last').textContent = (await r.text()) || '—';
  }catch(e){ console.log(e); }
}

/* ==================== INICIO ==================== */
// Bind hold-to-move en F / B / L / R
['btnF','btnB','btnL','btnR'].forEach(id => bindHold($(id)));

setInterval(refreshLast, 1000);
refreshLast();
refreshSpeed();
refreshLedStates();
</script>
</body>
</html>
)HTML";





// ========================== SETUP ==========================
void setup(){
  Serial.begin(115200);
  delay(300);

  setupPins();
  setupWiFi();
  setupHTTP();
  setupTasks();
}

void loop(){}

// -----------------------------------------------------------
//  Tareas
// -----------------------------------------------------------
void setupTasks(){
  xTaskCreatePinnedToCore(Task1code, "Tarea_Principal", 4096, NULL, 1, &Task1, 0);
  delay(150);
  xTaskCreatePinnedToCore(Task2code, "Tarea_HTTP",     4096, NULL, 1, &Task2, 1);
  delay(150);
  xTaskCreatePinnedToCore(Task3code, "Tarea_Log",      3072, NULL, 1, &Task3, 1);
}

// -----------------------------------------------------------
//  Pines y PWM
// -----------------------------------------------------------
void setupPins(){
  // Dirección de motores
  pinMode(M1A, OUTPUT);
  pinMode(M1B, OUTPUT);
  pinMode(M2A, OUTPUT);
  pinMode(M2B, OUTPUT);

  // PWM en enable
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
void setupWiFi(){
  Serial.println("\nConectando a WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  for (int i=0; i<40 && WiFi.status()!=WL_CONNECTED; ++i){
    delay(250); Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED){
    Serial.print("Conectado. IP: "); Serial.println(WiFi.localIP());
  }else{
    Serial.println("Fallo STA. Activando AP 'ESP32-ROBOT' (sin clave)...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-ROBOT");
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  }
}

// -----------------------------------------------------------
//  HTTP routing
// -----------------------------------------------------------
void setupHTTP(){
  server.on("/",          HTTP_GET, handleRoot);
  server.on("/cmd",       HTTP_GET, handleCmd);
  server.on("/speed",     HTTP_GET, handleSpeed);
  server.on("/ledToggle", HTTP_GET, handleLedToggle);
  server.on("/ledStates", HTTP_GET, handleLedStates);
  server.on("/last",      HTTP_GET, handleLast);

  server.onNotFound([](){ server.send(404, "text/plain", "404"); });

  server.begin();
  Serial.println("Servidor HTTP listo.");
}

void handleRoot(){ server.send(200, "text/html; charset=utf-8", PAGE_HTML); }

// /cmd?c=F|B|L|R|S
void handleCmd(){
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

// /speed           -> devuelve duty actual (0..255)
// /speed?v=0..255  -> fija nuevo duty para ambos motores
void handleSpeed(){
  if (!server.hasArg("v")) { server.send(200, "text/plain", String(g_speedDuty)); return; }
  int v = server.arg("v").toInt();
  if (v < 0) v = 0; if (v > 255) v = 255;
  g_speedDuty = (uint8_t)v;
  server.send(200, "text/plain", "OK");
}

// /ledToggle?n=1..4 -> alterna y devuelve "0" (OFF) o "1" (ON)
void handleLedToggle(){
  if (!server.hasArg("n")) { server.send(400, "text/plain", "Falta parametro n"); return; }
  int n = server.arg("n").toInt();
  int pin = -1;
  if      (n == 1) pin = LED1;
  else if (n == 2) pin = LED2;
  else if (n == 3) pin = LED3;
  else if (n == 4) pin = LED4;
  if (pin < 0) { server.send(400, "text/plain", "Parametro n invalido"); return; }

  // Leer estado actual del pin (como salida) y alternar
  int curr = digitalRead(pin);
  int next = (curr == HIGH) ? LOW : HIGH;
  digitalWrite(pin, next);

  server.send(200, "text/plain", (next==HIGH) ? "1" : "0");
}

// /ledStates -> {"L1":0/1,"L2":0/1,"L3":0/1,"L4":0/1}
void handleLedStates(){
  String j = "{";
  j += "\"L1\":" + String(digitalRead(LED1)==HIGH?1:0) + ",";
  j += "\"L2\":" + String(digitalRead(LED2)==HIGH?1:0) + ",";
  j += "\"L3\":" + String(digitalRead(LED3)==HIGH?1:0) + ",";
  j += "\"L4\":" + String(digitalRead(LED4)==HIGH?1:0);
  j += "}";
  server.send(200, "application/json; charset=utf-8", j);
}

void handleLast(){ server.send(200, "text/plain; charset=utf-8", getLastCommand()); }

// -----------------------------------------------------------
//  Movimiento (usa g_speedDuty en EN)
// -----------------------------------------------------------
static inline void enableMotors(uint8_t m1_duty, uint8_t m2_duty){
  ledcWrite(CH_M1EN, m1_duty);
  ledcWrite(CH_M2EN, m2_duty);
}

void robotStop(){
  enableMotors(0, 0);
  digitalWrite(M1A, LOW); digitalWrite(M1B, LOW);
  digitalWrite(M2A, LOW); digitalWrite(M2B, LOW);
}

void robotForward(){
  digitalWrite(M1A, HIGH); digitalWrite(M1B, LOW);   // M1 adelante
  digitalWrite(M2A, HIGH); digitalWrite(M2B, LOW);   // M2 adelante
  enableMotors(g_speedDuty, g_speedDuty);
}

void robotBackward(){
  digitalWrite(M1A, LOW);  digitalWrite(M1B, HIGH);  // M1 atrás
  digitalWrite(M2A, LOW);  digitalWrite(M2B, HIGH);  // M2 atrás
  enableMotors(g_speedDuty, g_speedDuty);
}

void robotRight(){
  // Giro en sitio a la izquierda: M1 atrás, M2 adelante
  digitalWrite(M1A, LOW);  digitalWrite(M1B, HIGH);  // M1 atrás
  digitalWrite(M2A, HIGH); digitalWrite(M2B, LOW);   // M2 adelante
  enableMotors(230, 230);
}

void robotLeft(){
  // Giro en sitio a la derecha: M1 adelante, M2 atrás
  digitalWrite(M1A, HIGH); digitalWrite(M1B, LOW);   // M1 adelante
  digitalWrite(M2A, LOW);  digitalWrite(M2B, HIGH);  // M2 atrás
  enableMotors(230, 230);
}

// -----------------------------------------------------------
//  Último comando (simple y claro)
// -----------------------------------------------------------
void setLastCommand(const String& cmd){ lastCommand = cmd; }
String getLastCommand(){ return String(lastCommand); }

// -----------------------------------------------------------
//  Utilidad FreeRTOS (cede CPU)
// -----------------------------------------------------------
void delayRTOS(uint32_t ms){ vTaskDelay(ms / portTICK_PERIOD_MS); }

// ======================= TAREA 1 ==========================
//  Watchdog suave: si pasan X ms sin comando nuevo -> STOP
// ==========================================================
void Task1code(void *pvParameters){
  Serial.print("Tarea 1 corriendo en el core ");
  Serial.println(xPortGetCoreID());

  for(;;){

    delayRTOS(40);
  }
}

// ======================= TAREA 2 ==========================
//  Atiende el servidor HTTP
// ==========================================================
void Task2code(void *pvParameters){
  Serial.print("Tarea 2 corriendo en el core ");
  Serial.println(xPortGetCoreID());
  for(;;){
    server.handleClient();
    delayRTOS(1); // cede CPU al resto
  }
}

// ======================= TAREA 3 ==========================
//  Logs periódicos (estado)
// ==========================================================
void Task3code(void *pvParameters){
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
      Serial.print("IP: ");
      Serial.println((WiFi.getMode()==WIFI_AP)? WiFi.softAPIP() : WiFi.localIP());
    }
    delayRTOS(500);
  }
}
