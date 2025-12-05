// *****************************************************
// PROYECTO: Controlar un LED con lenguaje natural usando Gemini y ESP32
// INTERACCIÓN: Todo se hace por el Monitor Serial (consola)
// *****************************************************


// ===================== BLOQUE 1 =====================
//   LIBRERÍAS Y CONFIGURACIÓN BÁSICA
// ====================================================

#include <Arduino.h>      // Librería base de Arduino
#include <WiFi.h>         // Manejo de WiFi en el ESP32
#include <HTTPClient.h>   // Para hacer peticiones HTTP (POST) a la API de Gemini
#include <ArduinoJson.h>  // Para construir y leer JSON de forma segura

// ---------- CONFIGURACIÓN DE RED Y MODELO ----------

const char* WIFI_SSID     = "Delga";        // Nombre de tu red WiFi
const char* WIFI_PASSWORD = "Delga1213";    // Contraseña de tu WiFi

String URL_GEMINI = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=";
const char* GEMINI_API_KEY = "AIzaSyBTelEaX3qfvIfu5kaBI50NTeTogRIFnr0";

// Cantidad máxima de "tokens" (palabras/unidades) que queremos en la respuesta.
// Aquí usamos pocas porque solo necesitamos LED_ON / LED_OFF / NONE.
const int   GEMINI_MAX_TOKENS = 100;

// Pin que vamos a usar para el LED.
const int LED_PIN = 2;


// ===================== BLOQUE 2 =====================
//   PRE-PROMPT (SYSTEM INSTRUCTION)
//   QUÉ LE PEDIMOS EXACTAMENTE AL MODELO
// ====================================================

// Este texto le indica al modelo CÓMO debe comportarse.
// Lo tratamos como si fuera "la personalidad/reglas" del modelo.
const char* SYSTEM_PROMPT =
R"(
  "Eres un parser de comandos para un ESP32 que controla una luz (LED). "
  "El usuario escribira frases en espanol como 'encender luz', 'apagar luz', "
  "'prende el bombillo', etc. "
  "Tu trabajo es analizar la INTENCION principal del usuario y responder "
  "UNICAMENTE con UNA PALABRA en MAYUSCULAS, sin espacios ni saltos de linea:"
  "- Responde LED_ON si la intencion principal es encender la luz o el LED."
  "- Responde LED_OFF si la intencion principal es apagar la luz o el LED."
  "- Responde NONE si no hay una intencion clara de encender o apagar la luz."
  "No des explicaciones, no agregues texto adicional, no uses otras palabras.")";


// ===================== BLOQUE 3 =====================
//   FUNCIÓN: CONECTARSE A LA RED WiFi
// ====================================================

// Esta función intenta conectarse a la red WiFi usando el SSID y PASSWORD
// definidos arriba. No devuelve nada, solo bloquea hasta que haya conexión.
void connectWiFi() {
  Serial.println();
  Serial.print("Conectando a WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);                 // Modo estación (conectarse a un router)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Esperamos hasta que el estado sea "conectado"
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");                 // Imprime puntos mientras intenta conectar
  }

  Serial.println();
  Serial.println("WiFi conectado");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());      // Muestra la IP que obtuvo el ESP32
  Serial.println();
}


// ===================== BLOQUE 4 =====================
//   FUNCIÓN: ENVIAR PREGUNTA A GEMINI Y RECIBIR RESPUESTA
//   (JSON CONSTRUIDO CON ArduinoJson)
// ====================================================

// Esta función:
// 1. Verifica que haya WiFi (y reconecta si no hay).
// 2. Construye un JSON con ArduinoJson 
//   "systemInstruction": {
//     "role": "system",
//     "parts": [
//       {
//         "text": "\"Eres un parser de comandos para un ESP32 que controla una luz (LED). \"\n\"El usuario escribira frases en espanol como 'encender luz', 'apagar luz', 'prende el bombillo', etc. \"\n\"Tu trabajo es analizar la INTENCION principal del usuario y responder UNICAMENTE con UNA PALABRA en MAYUSCULAS, sin espacios ni saltos de linea:\"\n\"- Responde LED_ON si la intencion principal es encender la luz o el LED.\"\n\"- Responde LED_OFF si la intencion principal es apagar la luz o el LED.\"\n\"- Responde NONE si no hay una intencion clara de encender o apagar la luz.\"\n\"No des explicaciones, no agregues texto adicional, no uses otras palabras.\""
//       }
//     ]
//   },
//   "contents": [
//     {
//       "role": "user",
//       "parts": [
//         {
//           "text": "enciende la luz por favor"
//         }
//       ]
//     }
//   ],
//   "generationConfig": {
//     "maxOutputTokens": 50
//   }
// }
// 3. Envía la petición a la API de Gemini
// 4. Lee la respuesta JSON y extrae el texto generado por el modelo.
// Devuelve ese texto como String.
String askGemini(const String& question) {
  // Si se perdió la conexión WiFi, la intentamos recuperar
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Reintentando conexión...");
    connectWiFi();
  }

  HTTPClient https; // Objeto para manejar la petición HTTP

  // URL de la API de Gemini con el modelo y la API key
  String url = URL_GEMINI + GEMINI_API_KEY;

  // Inicializamos la conexión HTTPS
  if (!https.begin(url)) {
    Serial.println("[HTTPS] No se pudo inicializar la conexión");
    return "ERROR_INIT";
  }

  // Cabecera: estamos enviando JSON
  https.addHeader("Content-Type", "application/json");

  // --------- Construir el JSON de la petición con ArduinoJson ---------
  // Creamos un documento JSON en memoria.
  JsonDocument payloadDoc;

  // systemInstruction: nuestro pre-prompt (reglas del modelo)
  JsonObject systemInstruction = payloadDoc.createNestedObject("systemInstruction");
  systemInstruction["role"] = "system";
  JsonArray systemParts = systemInstruction.createNestedArray("parts");
  JsonObject systemPart0 = systemParts.createNestedObject();
  systemPart0["text"] = SYSTEM_PROMPT;    // Aquí va el SYSTEM_PROMPT completo

  // contents: mensaje del usuario
  JsonArray contents = payloadDoc.createNestedArray("contents");
  JsonObject userMsg = contents.createNestedObject();
  userMsg["role"] = "user";
  JsonArray userParts = userMsg.createNestedArray("parts");
  JsonObject userPart0 = userParts.createNestedObject();
  userPart0["text"] = question;           // Aquí va lo que el estudiante escribe

  // generationConfig: configuración de la respuesta
  JsonObject genCfg = payloadDoc.createNestedObject("generationConfig");
  genCfg["maxOutputTokens"] = GEMINI_MAX_TOKENS;

  // Convertimos el documento JSON en un String listo para enviar
  String payload;
  serializeJson(payloadDoc, payload);

  // ver el JSON que estamos enviando
  Serial.println("JSON enviado a Gemini:");
  Serial.println(payload);
  Serial.println("---- fin JSON ----");

  // Enviamos la petición POST con el JSON construido
  int httpCode = https.POST(payload);

  // Si el código HTTP NO es 200 (OK) ni 301 (redirección), algo falló
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
    String errorMsg = String("[HTTPS] POST falló, código: ") + httpCode + " - " + https.errorToString(httpCode);
    Serial.println(errorMsg);
    https.end();
    return "ERROR_HTTP";
  }

  // Leemos la respuesta completa como String
  String response = https.getString();
  https.end(); // Cerramos la conexión

  // Creamos un documento JSON para parsear la respuesta
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.print("Error al parsear JSON: ");
    Serial.println(error.f_str());
    return "ERROR_JSON";
  }

  // Navegamos por la estructura JSON hasta llegar al texto generado:
  // candidates[0].content.parts[0].text
  const char* rawAnswer = doc["candidates"][0]["content"]["parts"][0]["text"];
  if (!rawAnswer) {
    // Si por alguna razón no hay texto, devolvemos NONE
    return "NONE";
  }

  String answer = String(rawAnswer);
  return answer;
}


// ===================== BLOQUE 5 =====================
//   FUNCIÓN: NORMALIZAR LA RESPUESTA DEL MODELO
// ====================================================

// El modelo podría devolver "LED_ON", "led_on", "LED_ON\n", etc.
// Aquí la limpiamos para quedarnos con:
//   - LED_ON
//   - LED_OFF
//   - NONE
String normalizeCommand(String answer) {
  // Mostramos primero la respuesta "cruda" para depurar
  Serial.print("Respuesta cruda de Gemini: ");
  Serial.println(answer);

  // Quitamos espacios al inicio y final
  answer.trim();
  // Convertimos todo a mayúsculas
  answer.toUpperCase();

  // Si el modelo devolvió más de una palabra, nos quedamos con la primera
  int spaceIndex = answer.indexOf(' ');
  if (spaceIndex > 0) {
    answer = answer.substring(0, spaceIndex);
  }

  // Aceptamos solo estas tres opciones
  if (answer == "LED_ON" || answer == "LED_OFF" || answer == "NONE") {
    return answer;
  }

  // Si devuelve algo extraño, por seguridad devolvemos NONE
  return "NONE";
}


// ===================== BLOQUE 6 =====================
//   FUNCIÓN: EJECUTAR EL COMANDO SOBRE EL LED
// ====================================================

// Según el comando que nos devuelva el modelo, prendemos o apagamos el LED.
// Si el comando es NONE, no hacemos nada.
void executeLedCommand(const String& cmd) {
  if (cmd == "LED_ON") {
    digitalWrite(LED_PIN, HIGH);
    Serial.println(">> LED encendido");
  } else if (cmd == "LED_OFF") {
    digitalWrite(LED_PIN, LOW);
    Serial.println(">> LED apagado");
  } else { // NONE
    Serial.println(">> Comando no relacionado con la luz. No se cambia el LED.");
  }
}


// ===================== BLOQUE 7 =====================
//   SETUP: SE EJECUTA UNA SOLA VEZ AL INICIO
// ====================================================

void setup() {
  Serial.begin(115200);      // Iniciamos la comunicación serial a 115200 baudios
  while (!Serial) {
    ; // En algunas placas espera a que el puerto serie esté listo
  }

  // Configuramos el pin del LED como salida y lo apagamos al inicio
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("==== ESP32 + Gemini (parser LED) por consola ====");
  connectWiFi();  // Intentamos conectarnos al WiFi al inicio
  Serial.println("Escribe un comando (ej: 'encender luz', 'apagar luz').");
  Serial.println("Escribe 'salir' para terminar.");
  Serial.println();
}


// ===================== BLOQUE 8 =====================
//   LOOP: SE EJECUTA CONSTANTEMENTE
// ====================================================

void loop() {
  // Revisamos si el usuario escribió algo en el Monitor Serie
  if (Serial.available()) {
    // Leemos hasta el salto de línea (cuando el usuario presiona Enter)
    String question = Serial.readStringUntil('\n');
    question.trim();  // Quitamos espacios en blanco al inicio y final

    // Si la línea está vacía, no hacemos nada
    if (question.length() == 0) {
      return;
    }

    // Comando especial para terminar el programa
    if (question.equalsIgnoreCase("salir")) {
      Serial.println("Fin del programa. Reinicia el ESP32 para comenzar de nuevo.");
      // Bucle infinito para "congelar" el programa
      while (true) {
        delay(1000);
      }
    }

    // Mostramos lo que se va a enviar al modelo
    Serial.println();
    Serial.print("Enviando a Gemini: ");
    Serial.println(question);
    Serial.println("Procesando...\n");

    // 1) Enviamos el texto del usuario al modelo
    String rawAnswer = askGemini(question);

    // 2) Normalizamos la respuesta para obtener LED_ON / LED_OFF / NONE
    String cmd       = normalizeCommand(rawAnswer);

    // 3) Ejecutamos el comando sobre el LED
    Serial.print("Comando interpretado: ");
    Serial.println(cmd);

    executeLedCommand(cmd);

    Serial.println("\nEscribe otra frase o 'salir' para terminar:");
    Serial.println();
  }

  // Pequeño delay para no saturar el procesador
  delay(10);
}
