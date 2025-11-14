/*
  ============================================================================
  ESP32: WiFi + Servidor Web + Control de 4 LEDs + Estado en vivo (didáctico)
  ============================================================================
  ¿QUÉ APRENDERÁS?
  - Conectar el ESP32 a una red WiFi (modo estación "STA")
  - Levantar un servidor HTTP en el puerto 80
  - Servir una página web (HTML + CSS + JS) embebida en el firmware
  - Usar "endpoints" (rutas) para controlar hardware: /led y /status
  - Leer estado desde el navegador con fetch() y actualizar la página cada 1 s

  IMPORTANTE
  - Sin clases; solo funciones y variables globales simples (estilo principiante).
  - El código web está súper comentado para enseñar HTML/CSS/JS básico.
  - GPIO0 (LED2) es pin de arranque: puede usarse como salida, pero evita forzarlo
    a niveles "raros" durante el arranque (no pulses un botón a GND en BOOT).

  HARDWARE:
  - LEDs en GPIO: 2, 0, 16, 17 (puedes cambiarlos abajo)
  - ESP32 con WiFi (cualquier DevKit común)

  AUTOR: tú + ChatGPT (versión súper comentada)
*/

#include <Arduino.h>
#include <WiFi.h>      // Librería WiFi del ESP32
#include <WebServer.h> // Servidor HTTP sencillo (sin websockets, sin SSE)

// ---------------------------------------------------------------------------
// 1) CONFIGURACIÓN DE PINES (puedes ajustar a tu placa)
// ---------------------------------------------------------------------------
const int LED1 = 2;   // Suele estar conectado a un LED on-board (depende de la placa)
const int LED2 = 0;   // ATENCIÓN: GPIO0 es pin de arranque (ver comentario arriba)
const int LED3 = 16;
const int LED4 = 17;

// ---------------------------------------------------------------------------
// 2) CREDENCIALES WiFi (Rellena con tu red)
// ---------------------------------------------------------------------------
const char* WIFI_SSID = "Delga";
const char* WIFI_PASS = "Delga1213";

// ---------------------------------------------------------------------------
// 3) VARIABLES GLOBALES DE APOYO
// ---------------------------------------------------------------------------
WebServer server(80);             // Servidor HTTP en el puerto estándar 80
unsigned long lastTick = 0;       // Para imprimir "ticks" periódicos al Serial
const unsigned long TICK_MS = 2000; // Cada 2 segundos (ejemplo de tarea periódica)

// ---------------------------------------------------------------------------
// 4) PÁGINA HTML COMPLETA (con CSS y JS) INCRUSTADA EN EL FIRMWARE
//    - R"HTML( ... )HTML" permite escribir bloque multilínea sin escapar comillas
//    - Lo que ves aquí es EXACTAMENTE lo que el navegador recibe al entrar a "/"
// ---------------------------------------------------------------------------
const char* PAGE_HTML = R"HTML(
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <!-- meta viewport hace que la página sea "responsive" en celulares -->
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32 - WiFi Basics</title>

  <!-- ==================== CSS ====================
       Estilos MUY simples para que la interfaz sea legible y limpia.
       Podrías usar frameworks (Bootstrap, Tailwind), pero aquí
       preferimos "vanilla CSS" para explicar lo mínimo necesario. -->
  <style>
    :root { --radius: 12px; }
    body{
      font-family: system-ui, Segoe UI, Arial, sans-serif;
      margin: 18px; max-width: 680px;
    }
    h1{ font-size: 1.2rem; margin: 0 0 12px; }
    .card{
      border: 1px solid #ddd; border-radius: var(--radius);
      padding: 12px; margin: 12px 0;
    }
    .row{ display: flex; gap: 10px; flex-wrap: wrap; margin-top: 8px; }
    button{
      padding: 10px 12px; cursor: pointer;
      border-radius: var(--radius); border: 1px solid #ccc; background: #fff;
    }
    #status{
      background: #f8f8f8; padding: 10px; border-radius: var(--radius);
      white-space: pre-wrap;  /* Para ver saltos de línea si los hubiera */
      font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
    }
    code{ background: #f3f3f3; padding: 2px 6px; border-radius: 6px; }
  </style>
</head>
<body>
  <h1>ESP32 – WiFi + Servidor Web + LEDs + Estado</h1>

  <!-- ==================== BLOQUE: CONTROL DE LEDS ====================
       Cada par de botones llama a la función JS led(n, v),
       que hace una petición HTTP GET a /led?n=...&v=...
         - n: número de LED (1..4)
         - v: valor (1 = encender, 0 = apagar)
       En el ESP32, la ruta /led se atiende en 'handleLed()' (ver C++) -->
  <div class="card">
    <b>Control de LEDs</b>
    <div class="row">
      <button onclick="led(1,1)">LED1 ON</button>
      <button onclick="led(1,0)">LED1 OFF</button>
    </div>
    <div class="row">
      <button onclick="led(2,1)">LED2 ON</button>
      <button onclick="led(2,0)">LED2 OFF</button>
    </div>
    <div class="row">
      <button onclick="led(3,1)">LED3 ON</button>
      <button onclick="led(3,0)">LED3 OFF</button>
    </div>
    <div class="row">
      <button onclick="led(4,1)">LED4 ON</button>
      <button onclick="led(4,0)">LED4 OFF</button>
    </div>
    <small>También puedes probar manualmente en la barra de direcciones:
      <code>/led?n=1&v=1</code> (enciende LED1)</small>
  </div>

  <!-- ==================== BLOQUE: ESTADO EN VIVO ====================
       La idea: pedir al ESP32 (cada 1 s) un "resumen" de estado por HTTP.
       - JS hace fetch('/status') y coloca el texto recibido en el <div id="status">
       - En C++, 'handleStatus()' construye el mensaje (IP, RSSI, tiempo activo, etc.)
       Esto simula "telemetría sencilla" para que los estudiantes vean que el
       navegador puede LEER datos del dispositivo (no solo enviar comandos). -->
  <div class="card">
    <b>Estado del ESP32</b>
    <div id="status">Cargando...</div>
  </div>

  <!-- ==================== JAVASCRIPT ====================
       - 'fetch' es la API moderna del navegador para hacer solicitudes HTTP.
       - Usamos 'async/await' para escribir código asíncrono de forma clara. -->
  <script>
    // Enciende/Apaga un LED llamando al endpoint /led?n=...&v=...
    async function led(n, v) {
      try {
        // Construimos la URL con 'query params' n y v.
        // OJO: GET no lleva cuerpo; todo va en la URL.
        await fetch(`/led?n=${n}&v=${v}`);
        // Podríamos mostrar un aviso al usuario o actualizar una etiqueta.
      } catch (err) {
        console.log("Error al cambiar LED:", err);
      }
    }

    // Pide al ESP32 un texto de estado y lo muestra en el <div id="status">
    async function refreshStatus() {
      try {
        const resp = await fetch('/status');  // GET a /status
        const txt  = await resp.text();       // decodifica la respuesta como texto plano
        document.getElementById('status').textContent = txt;
      } catch (err) {
        document.getElementById('status').textContent = "Sin conexión con el ESP32";
      }
    }

    // Llamamos una vez al inicio...
    refreshStatus();
    // ...y luego cada 1000 ms para tener un "casi tiempo real" simple
    setInterval(refreshStatus, 1000);
  </script>
</body>
</html>
)HTML";

// ---------------------------------------------------------------------------
// 5) CONFIGURACIÓN DE PINES
//    - Ponemos los 4 LEDs como salida y empezamos en LOW (apagados)
// ---------------------------------------------------------------------------
void setupPins() {
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);

  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
}

// ---------------------------------------------------------------------------
// 6) CONEXIÓN WiFi (STA) con "fallback" a AP si falla
//    - STA: se conecta a tu router. Verás una IP del tipo 192.168.x.x
//    - AP : el ESP32 crea su propio WiFi (SSID "ESP32-AP"), IP 192.168.4.1
// ---------------------------------------------------------------------------
void connectWiFi() {
  Serial.println("\n[WiFi] Conectando como estación (STA)...");
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
    Serial.println("[WiFi] Conexión exitosa ✅");
    Serial.print("[WiFi] SSID: "); Serial.println(WIFI_SSID);
    Serial.print("[WiFi] IP:   "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] No se pudo conectar ❌");
    Serial.println("[WiFi] Activando modo AP (punto de acceso)");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-AP"); // AP sin contraseña para simplificar la demo
    Serial.print("[WiFi] AP SSID: ESP32-AP | IP: ");
    Serial.println(WiFi.softAPIP()); // Normalmente 192.168.4.1
  }
}

// ---------------------------------------------------------------------------
// 7) HANDLERS HTTP (rutas del servidor)
//    - handleRoot(): responde con el HTML de la app
//    - handleLed():  enciende/apaga LEDs según parámetros n y v
//    - handleStatus(): devuelve un texto con info útil (IP, RSSI, uptime)
// ---------------------------------------------------------------------------
void handleRoot() {
  // Enviamos el HTML incrustado con tipo "text/html"
  server.send(200, "text/html; charset=utf-8", PAGE_HTML);
}

void handleLed() {
  // Esperamos /led?n=1..4&v=0|1
  if (!server.hasArg("n") || !server.hasArg("v")) {
    server.send(400, "text/plain", "Faltan parametros n y/o v");
    return;
  }

  int n = server.arg("n").toInt(); // Número de LED (1..4)
  int v = server.arg("v").toInt(); // 0=OFF, 1=ON
  int pin = -1;

  if      (n == 1) pin = LED1;
  else if (n == 2) pin = LED2;
  else if (n == 3) pin = LED3;
  else if (n == 4) pin = LED4;

  if (pin < 0 || (v != 0 && v != 1)) {
    server.send(400, "text/plain", "Parametros invalidos");
    return;
  }

  digitalWrite(pin, v ? HIGH : LOW);
  server.send(200, "text/plain", "OK"); // Código 200 = OK (respuesta simple)
}

void handleStatus() {
  // Construimos un "reporte" de texto para enseñar
  // que el servidor puede devolver "telemetría" en texto plano.
  String msg;

  // Tiempo activo en segundos (uptime)
  msg += "Uptime: ";
  msg += millis() / 1000;
  msg += " s\n";

  // Estado WiFi: conectado (STA) o AP
  if (WiFi.getMode() == WIFI_AP) {
    msg += "Modo: AP (Punto de Acceso)\n";
    msg += "IP:   " + WiFi.softAPIP().toString() + "\n";
  } else {
    // Si está en STA pero aún desconectado:
    if (WiFi.status() == WL_CONNECTED) {
      msg += "IP:   " + WiFi.localIP().toString() + "\n";
    }
  }

  server.send(200, "text/plain; charset=utf-8", msg);
}

// ---------------------------------------------------------------------------
// 8) ARRANQUE DEL SERVIDOR HTTP (asignación de rutas)
// ---------------------------------------------------------------------------
void setupHTTP() {
  server.on("/",      HTTP_GET, handleRoot);    // Página principal
  server.on("/led",   HTTP_GET, handleLed);     // Control de LEDs
  server.on("/status",HTTP_GET, handleStatus);  // Estado para la UI
  server.begin();
  Serial.println("[HTTP] Servidor iniciado en puerto 80");
}

// ---------------------------------------------------------------------------
// 9) SETUP y LOOP PRINCIPALES
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  setupPins();     // LEDs como salida
  connectWiFi();   // Conecta a WiFi (o crea AP)
  setupHTTP();     // Rutas y arranque del servidor
}

void loop() {
  // Atiende peticiones HTTP de los navegadores (no bloquea)
  server.handleClient();

  // Ejemplo de "mensaje periódico" hacia el monitor serie (cada 2 s)
  if (millis() - lastTick > TICK_MS) {
    lastTick = millis();
    Serial.print("[Tick] t=");
    Serial.print(lastTick / 1000);
    Serial.print("s  |  IP: ");
    // La IP correcta depende del modo actual
    if (WiFi.getMode() == WIFI_AP) Serial.println(WiFi.softAPIP());
    else                           Serial.println(WiFi.localIP());
  }
}
