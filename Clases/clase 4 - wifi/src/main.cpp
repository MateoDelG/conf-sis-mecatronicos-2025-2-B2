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

// LEDs (controlados desde la web si quieres luego)
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
//  FreeRTOS: Tareas
// ==========================================================
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

// Servidor HTTP
WebServer server(80);

// Estado compartido sencillo
String   lastCommand    = "STOP";
uint32_t lastCmdMillis  = 0;

// PWM (ESP32: ledc)
const int PWM_FREQ = 15000;  // 15 kHz
const int PWM_RES  = 8;      // 8 bits (0..255)
const int CH_M1EN  = 0;
const int CH_M2EN  = 1;

volatile bool g_autoMode = false;


// Velocidad actual (duty 0..255)
volatile uint8_t g_speedDuty = 200;

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

// Handlers HTTP
void handleRoot();
void handleMove();
void handlePWM();
void handleNotFound();
void handleMode();


// -----------------------------------------------------------
//  HTML de la página (modo oscuro, móvil, flechas + PWM)
// -----------------------------------------------------------
const char PAGE_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <title>Control Robot ESP32</title>
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
  <style>
    :root{
      color-scheme: dark;
    }
    *{
      box-sizing:border-box;
      -webkit-tap-highlight-color: transparent;
    }
    body{
      margin:0;
      padding:0;
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background:#050812;
      color:#f5f5f5;
      min-height:100vh;
      display:flex;
      align-items:center;
      justify-content:center;
    }
    .card{
      width:100%;
      max-width:420px;
      margin:16px;
      padding:18px 18px 20px;
      border-radius:20px;
      background: radial-gradient(circle at top left, #222e5f 0, #050812 45%, #020308 100%);
      box-shadow:0 18px 40px rgba(0,0,0,0.7);
    }
    h1{
      margin:0 0 4px;
      font-size:1.3rem;
      text-align:center;
    }
    .subtitle{
      margin:0 0 12px;
      text-align:center;
      font-size:0.85rem;
      opacity:0.75;
    }

    /* Modo manual / automático */
    .mode-row{
      display:flex;
      justify-content:space-between;
      align-items:center;
      margin-bottom:10px;
      font-size:0.9rem;
    }
    .mode-label span{
      font-weight:600;
    }
    .mode-toggle{
      padding:8px 14px;
      border-radius:999px;
      border:1px solid rgba(255,255,255,0.1);
      background:#141826;
      color:#f5f5f5;
      font-size:0.8rem;
      cursor:pointer;
      user-select:none;
    }
    .mode-toggle.auto{
      background:#24814c;
    }

    .status-text{
      text-align:center;
      font-size:0.9rem;
      margin-bottom:8px;
      opacity:0.85;
    }

    .dpad{
      display:grid;
      grid-template-columns:repeat(3,1fr);
      grid-template-rows:repeat(3,70px);
      gap:8px;
      justify-items:center;
      align-items:center;
      margin:8px 0 18px;
    }
    .dpad button{
      width:100%;
      height:100%;
      max-width:96px;
      border-radius:18px;
      border:1px solid rgba(255,255,255,0.08);
      background:#141826;
      color:#f5f5f5;
      font-size:1.4rem;
      display:flex;
      align-items:center;
      justify-content:center;
      cursor:pointer;
      user-select:none;
      transition:transform 0.08s ease, box-shadow 0.08s ease, background 0.12s ease, opacity 0.12s ease;
    }
    .dpad button:active{
      transform:scale(0.96);
      box-shadow:0 0 18px rgba(0,0,0,0.8) inset;
      background:#1d2135;
    }
    .dpad button.disabled{
      opacity:0.35;
      cursor:default;
    }
    .dpad button.disabled:active{
      transform:none;
      box-shadow:none;
      background:#141826;
    }
    .dpad-empty{
      width:100%;
      height:100%;
    }

    /* Slider PWM */
    .pwm-block{
      margin-top:4px;
      padding:10px 10px 4px;
      border-radius:14px;
      background:rgba(5,10,30,0.9);
      border:1px solid rgba(120,160,255,0.35);
    }
    .pwm-header{
      display:flex;
      justify-content:space-between;
      align-items:center;
      font-size:0.9rem;
      margin-bottom:6px;
    }
    .pwm-value{
      font-variant-numeric: tabular-nums;
      font-size:0.9rem;
      opacity:0.9;
    }
    input[type="range"]{
      -webkit-appearance:none;
      width:100%;
      height:6px;
      border-radius:999px;
      background:#262a40;
      outline:none;
    }
    input[type="range"]::-webkit-slider-thumb{
      -webkit-appearance:none;
      width:18px;
      height:18px;
      border-radius:50%;
      background:#31b86a;
      border:2px solid #ffffff30;
      box-shadow:0 0 8px rgba(49,184,106,0.7);
      margin-top:-6px;
    }
    input[type="range"]::-moz-range-thumb{
      width:18px;
      height:18px;
      border-radius:50%;
      background:#31b86a;
      border:2px solid #ffffff30;
      box-shadow:0 0 8px rgba(49,184,106,0.7);
    }
    .footer{
      margin-top:14px;
      text-align:center;
      font-size:0.78rem;
      opacity:0.6;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>Control del robot</h1>
    <p class="subtitle">ESP32 · Modo manual / automático</p>

    <div class="mode-row">
      <div class="mode-label">
        Modo: <span id="modeLabel">Manual</span>
      </div>
      <button class="mode-toggle" id="modeBtn" onclick="toggleMode()">
        Cambiar a Automático
      </button>
    </div>

    <p class="status-text" id="statusDir">Dirección actual: STOP</p>

    <!-- Cruceta de 4 flechas -->
    <div class="dpad">
      <div class="dpad-empty"></div>
      <button
        id="btnUp"
        onpointerdown="startMove('FWD','Arriba')"
        onpointerup="stopMove()"
        onpointerleave="stopMove()"
      >▲</button>
      <div class="dpad-empty"></div>

      <button
        id="btnLeft"
        onpointerdown="startMove('LEFT','Izquierda')"
        onpointerup="stopMove()"
        onpointerleave="stopMove()"
      >◀</button>
      <div class="dpad-empty"></div>
      <button
        id="btnRight"
        onpointerdown="startMove('RIGHT','Derecha')"
        onpointerup="stopMove()"
        onpointerleave="stopMove()"
      >▶</button>

      <div class="dpad-empty"></div>
      <button
        id="btnDown"
        onpointerdown="startMove('BACK','Abajo')"
        onpointerup="stopMove()"
        onpointerleave="stopMove()"
      >▼</button>
      <div class="dpad-empty"></div>
    </div>

    <!-- Control PWM -->
    <div class="pwm-block">
      <div class="pwm-header">
        <span>PWM / Velocidad</span>
        <span class="pwm-value" id="pwmVal">200 / 255</span>
      </div>
      <input id="pwmSlider" type="range" min="0" max="255" value="200" oninput="updatePWM()">
    </div>

    <div class="footer">
      Manual: control con flechas. Automático: juego de luces con LEDs.
    </div>
  </div>

  <script>
    let autoMode = false;

    function sendMove(dir){
      fetch('/move?dir=' + encodeURIComponent(dir))
        .catch(_ => {
          const label = document.getElementById('statusDir');
          label.textContent = 'Error de conexión';
        });
    }

    function startMove(code, labelText){
      if (autoMode) return;  // en modo automático no se mueve
      const label = document.getElementById('statusDir');
      label.textContent = 'Dirección actual: ' + labelText;
      sendMove(code);
    }

    function stopMove(){
      if (autoMode) return;  // en modo automático no hay STOP del joystick
      const label = document.getElementById('statusDir');
      label.textContent = 'Dirección actual: STOP';
      sendMove('STOP');
    }

    function updatePWM(){
      const slider = document.getElementById('pwmSlider');
      const label  = document.getElementById('pwmVal');
      label.textContent = slider.value + ' / 255';

      fetch('/pwm?val=' + encodeURIComponent(slider.value))
        .catch(_ => {});
    }

    function setDpadEnabled(enabled){
      const ids = ['btnUp','btnLeft','btnRight','btnDown'];
      ids.forEach(id => {
        const b = document.getElementById(id);
        if (!b) return;
        if (enabled){
          b.classList.remove('disabled');
        } else {
          b.classList.add('disabled');
        }
      });
    }

    function toggleMode(){
      autoMode = !autoMode;
      const modeLabel = document.getElementById('modeLabel');
      const btn       = document.getElementById('modeBtn');
      const status    = document.getElementById('statusDir');

      if (autoMode){
        modeLabel.textContent = 'Automático';
        btn.textContent       = 'Cambiar a Manual';
        btn.classList.add('auto');
        setDpadEnabled(false);
        status.textContent = 'Modo automático: luces activas';
        // Avisar al ESP32
        fetch('/mode?m=auto').catch(_ => {});
      } else {
        modeLabel.textContent = 'Manual';
        btn.textContent       = 'Cambiar a Automático';
        btn.classList.remove('auto');
        setDpadEnabled(true);
        status.textContent = 'Dirección actual: STOP';
        // Avisar al ESP32
        fetch('/mode?m=manual').catch(_ => {});
      }
    }
  </script>
</body>
</html>
)HTML";


// ========================== SETUP / LOOP ===================
void setup()
{
  Serial.begin(115200);
  delay(500);

  setupPins();
  setupWiFi();
  setupHTTP();
  setupTasks();
}

void loop() {
  // Usamos solo tareas FreeRTOS
}

// ========================== TAREAS =========================
void setupTasks()
{
  xTaskCreatePinnedToCore(Task1code, "Tarea_Principal", 5000, NULL, 1, &Task1, 0);
  delay(200);
  xTaskCreatePinnedToCore(Task2code, "Tarea_HTTP",     6000, NULL, 1, &Task2, 1);
  delay(200);
  xTaskCreatePinnedToCore(Task3code, "Tarea_Log",      3000, NULL, 1, &Task3, 1);
  delay(200);
}

// Tarea 1: seguridad / lógica básica (por ejemplo, parar si no hay comandos)
void Task1code(void *pvParameters)
{
  (void) pvParameters;
  const uint32_t TIMEOUT_STOP_MS = 3000; // 3 s sin comandos -> STOP

  for(;;) {
    uint32_t now = millis();
    if ((now - lastCmdMillis) > TIMEOUT_STOP_MS && lastCommand != "STOP") {
      robotStop();
      lastCommand = "STOP";
    }
    delayRTOS(50);
  }
}

// Tarea 2: atender el servidor HTTP
void Task2code(void *pvParameters)
{
  (void) pvParameters;
  for(;;) {
    server.handleClient();
    delayRTOS(5);   // pequeño respiro
  }
}

// Tarea 3: logs periódicos
void Task3code(void *pvParameters)
{
  (void) pvParameters;
  uint32_t logCounter = 0;
  uint8_t step = 0;

  for(;;) {
    if (g_autoMode) {
      // Juego de luces sencillo: recorre los 4 LEDs
      // SOLO uno encendido a la vez
      digitalWrite(LED1, (step == 0) ? HIGH : LOW);
      digitalWrite(LED2, (step == 1) ? HIGH : LOW);
      digitalWrite(LED3, (step == 2) ? HIGH : LOW);
      digitalWrite(LED4, (step == 3) ? HIGH : LOW);

      step = (step + 1) % 4;

      // Log menos frecuente para no saturar
      if (logCounter % 10 == 0) {
        Serial.print("[AUTO] Paso LED: ");
        Serial.print(step);
        Serial.print(" | PWM: ");
        Serial.println(g_speedDuty);
      }

      logCounter++;
      delayRTOS(150);   // velocidad del juego de luces

    } else {
      // Modo manual: aseguramos todos los LEDs apagados (si lo deseas)
      digitalWrite(LED1, LOW);
      digitalWrite(LED2, LOW);
      digitalWrite(LED3, LOW);
      digitalWrite(LED4, LOW);

      if (logCounter % 5 == 0) {
        Serial.print("[MANUAL] Comando: ");
        Serial.print(lastCommand);
        Serial.print(" | PWM: ");
        Serial.println(g_speedDuty);
      }

      logCounter++;
      delayRTOS(300);
    }
  }
}

// ==========================================================
//  Pines y PWM
// ==========================================================
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

// ==========================================================
//  WiFi
// ==========================================================
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

// ==========================================================
//  HTTP
// ==========================================================
void setupHTTP()
{
  server.on("/",   HTTP_GET, handleRoot);
  server.on("/move", HTTP_GET, handleMove);
  server.on("/pwm",  HTTP_GET, handlePWM);
  server.on("/mode", HTTP_GET, handleMode);   // <--- NUEVA
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Servidor HTTP iniciado en puerto 80");
}

void handlePWM()
{
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "Falta parametro val (0..255)");
    return;
  }

  int val = server.arg("val").toInt();

  if (val < 0)   val = 0;
  if (val > 255) val = 255;

  g_speedDuty = (uint8_t)val;

  Serial.print("[HTTP] Nuevo PWM: ");
  Serial.println(g_speedDuty);

  // Sólo aplicamos a los motores en modo MANUAL
  if (!g_autoMode) {
    if      (lastCommand == "FWD")   robotForward();
    else if (lastCommand == "BACK")  robotBackward();
    else if (lastCommand == "LEFT")  robotLeft();
    else if (lastCommand == "RIGHT") robotRight();
    else                             robotStop();   // STOP u otro
  } else {
    // En modo automático puedes dejar los motores siempre en STOP
    robotStop();
  }

  server.send(200, "text/plain", "PWM=" + String(g_speedDuty));
}

void handleMode()
{
  if (!server.hasArg("m")) {
    server.send(400, "text/plain", "Falta parametro m (manual|auto)");
    return;
  }

  String m = server.arg("m");
  m.toLowerCase();

  if (m == "auto") {
    g_autoMode = true;
    robotStop();           // por seguridad, quieto en modo auto
    lastCommand = "STOP";
    server.send(200, "text/plain", "MODO=AUTO");
  } else {
    g_autoMode = false;
    robotStop();           // también arrancamos quietos al volver a manual
    lastCommand = "STOP";
    server.send(200, "text/plain", "MODO=MANUAL");
  }

  lastCmdMillis = millis();
}

void handleRoot()
{
  server.send_P(200, "text/html", PAGE_HTML);
}

void handleMove()
{
  if (g_autoMode) {
    // En modo automático ignoramos las órdenes de movimiento
    server.send(200, "text/plain", "AUTO_MODE");
    return;
  }

  if (!server.hasArg("dir")) {
    server.send(400, "text/plain", "Falta parametro dir");
    return;
  }

  String dir = server.arg("dir");

  if      (dir == "FWD")   { robotForward();  lastCommand = "FWD";  }
  else if (dir == "BACK")  { robotBackward(); lastCommand = "BACK"; }
  else if (dir == "LEFT")  { robotLeft();     lastCommand = "LEFT"; }
  else if (dir == "RIGHT") { robotRight();    lastCommand = "RIGHT";}
  else if (dir == "STOP")  { robotStop();     lastCommand = "STOP"; }
  else {
    robotStop();
    lastCommand = "STOP";
  }

  lastCmdMillis = millis();
  server.send(200, "text/plain", "OK " + lastCommand);
}

void handleNotFound()
{
  String msg = "Ruta no encontrada\n\n";
  msg += "Rutas disponibles:\n";
  msg += "  /        (pagina)\n";
  msg += "  /move?dir=FWD|BACK|LEFT|RIGHT\n";
  msg += "  /pwm?val=0..255\n";
  server.send(404, "text/plain", msg);
}

// ==========================================================
//  Movimiento (usa g_speedDuty en EN)
// ==========================================================
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

// ==========================================================
//  Utilidad delay para FreeRTOS
// ==========================================================
void delayRTOS(uint32_t ms)
{
  vTaskDelay(ms / portTICK_PERIOD_MS);
}
