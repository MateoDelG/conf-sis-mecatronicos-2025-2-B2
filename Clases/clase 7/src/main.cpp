#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "ArduinoJson.h"

const char* ssid = "Delga";
const char* password = "Delga1213";

String URL_GEMINI = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=";
const char* API_KEY = "AIzaSyBTelEaX3qfvIfu5kaBI50NTeTogRIFnr0";

const int LED_PIN = 2;

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

const int GEMINI_MAX_TOKENS = 50;

void conectWiFi() {
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

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

String askGemini(const String& userInput) {
    if(WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected!");
        conectWiFi();
    }

    HTTPClient https;

    String fullURL = URL_GEMINI + API_KEY;

    if(!https.begin(fullURL)) {
        Serial.println("Unable to connect");
        return "ERROR al iniciar";
    }

    https.addHeader("Content-Type", "application/json");

    JsonDocument payloadDoc;

    JsonObject systemInstruction = payloadDoc.createNestedObject("systemInstruction");
    systemInstruction["role"] = "system";
    JsonArray systemParts = systemInstruction.createNestedArray("parts");
    JsonObject systemPart0 = systemParts.createNestedObject();
    systemPart0["text"] = SYSTEM_PROMPT;    // Aquí va el SYSTEM_PROMPT completo

    JsonArray contents = payloadDoc.createNestedArray("contents");
    JsonObject userMsg = contents.createNestedObject();
    userMsg["role"] = "user";
    JsonArray userParts = userMsg.createNestedArray("parts");
    JsonObject userPart0 = userParts.createNestedObject();
    userPart0["text"] = userInput;           // Aquí va lo que el estudiante escribe

    JsonObject genCfg = payloadDoc.createNestedObject("generationConfig");
    genCfg["maxOutputTokens"] = GEMINI_MAX_TOKENS;

    String payload;
    serializeJson(payloadDoc, payload);

    int httpResponseCode = https.POST(payload);

    if(httpResponseCode != 200 && httpResponseCode != 301) {
        Serial.print("HTTP Error code: ");
        Serial.println(httpResponseCode);
        https.end();
        return "ERROR en la solicitud";
    }

    String response = https.getString();
    https.end();

    JsonDocument responseDoc;

    DeserializationError error = deserializeJson(responseDoc, response);
    if (error) {
        Serial.print("JSON deserialization error: ");
        Serial.println(error.c_str());
        return "ERROR en JSON";
    }

    const char* generatedText = responseDoc["candidates"][0]["content"]["parts"][0]["text"];
    if(!generatedText) {
        Serial.println("No se encontro texto generado");
        return "NONE";
    }
    String result = String(generatedText);
    return result;
}

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

void executeCommand(const String& command) {
  if (command == "LED_ON") {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED encendido.");
  } else if (command == "LED_OFF") {
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED apagado.");
  } else {
    Serial.println("Ninguna accion tomada.");
  }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    conectWiFi();

}

void loop() {

  if (Serial.available()) {
    String userInput = Serial.readStringUntil('\n');
    userInput.trim();

    if (userInput.length() > 0) {
        Serial.println("User Input: " + userInput);
        String command = askGemini(userInput);
        String normalizedCommand = normalizeCommand(command);
        executeCommand(normalizedCommand);
    }
  }
}
