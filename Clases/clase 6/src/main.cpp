#include <Arduino.h>
#include <BluetoothSerial.h>

// ==========================================================
//  MAPEOS DE PINES (según tu hardware)
// ==========================================================
// Motor 1 (izquierdo)
const int M1A  = 12;   // IN1
const int M1B  = 14;   // IN2
const int M1EN = 27;   // ENA

// Motor 2 (derecho)
const int M2A  = 26;   // IN3
const int M2B  = 25;   // IN4
const int M2EN = 33;   // ENB

// LEDs (por ahora como indicadores generales)
const int LED1 = 2;
const int LED2 = 0;    // ojo: GPIO0 es pin de arranque
const int LED3 = 16;
const int LED4 = 17;

// ==========================================================
//  FreeRTOS: Tareas
// ==========================================================
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

// ===================== Bluetooth =====================
BluetoothSerial SerialBT;   // Objeto para Serial Bluetooth

// -----------------------------------------------------------
//  Prototipos
// -----------------------------------------------------------
void Task1code(void *pvParameters);
void Task2code(void *pvParameters);
void Task3code(void *pvParameters);

void setupTasks();
void delayRTOS(uint32_t ms);
void setupPins();

// --- Prototipos de funciones de movimiento del robot ---
void robotStop();
void robotForward();
void robotBackward();
void robotRight();
void robotLeft();
void robotForwardRight();
void robotForwardLeft();
void robotBackwardRight();
void robotBackwardLeft();


// ========================== SETUP ==========================
void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println(F("Iniciando ESP32..."));

  setupPins();

  // ---------- Bluetooth ----------
  if (!SerialBT.begin("ESP32-BT-ROBOT")) {
    Serial.println(F("Error iniciando Bluetooth"));
  } else {
    Serial.println(F("Bluetooth iniciado. Nombre: ESP32-BT-ROBOT"));
    Serial.println(F("Empareja desde tu celular/PC y abre un terminal serie BT."));
  }

  // Aseguramos robot detenido al inicio
  robotStop();

  setupTasks();
}

void loop() {}

// ===================== TAREAS Y UTILIDADES =====================

void setupTasks()
{
  xTaskCreatePinnedToCore(Task1code, "Tarea_Principal", 5000, NULL, 1, &Task1, 0);
  delay(200);
  xTaskCreatePinnedToCore(Task2code, "Tarea_BT",        6000, NULL, 1, &Task2, 1);
  delay(200);
  xTaskCreatePinnedToCore(Task3code, "Tarea_Log",       3000, NULL, 1, &Task3, 1);
  delay(200);
}

void setupPins()
{
  // LEDs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);

  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);

  // Motores
  pinMode(M1A, OUTPUT);
  pinMode(M1B, OUTPUT);
  pinMode(M1EN, OUTPUT);

  pinMode(M2A, OUTPUT);
  pinMode(M2B, OUTPUT);
  pinMode(M2EN, OUTPUT);

  // Apagamos todo al inicio
  digitalWrite(M1A, LOW);
  digitalWrite(M1B, LOW);
  digitalWrite(M1EN, LOW);

  digitalWrite(M2A, LOW);
  digitalWrite(M2B, LOW);
  digitalWrite(M2EN, LOW);
}

void delayRTOS(uint32_t ms)
{
  vTaskDelay(ms / portTICK_PERIOD_MS);
}

// -----------------------------------------------------------
//  Tarea 1: “vida” del sistema (parpadeo de LED1)
// -----------------------------------------------------------
void Task1code(void *pvParameters)
{
  Serial.print("Tarea 1 corriendo en el core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    digitalWrite(LED1, HIGH);
    delayRTOS(300);
    digitalWrite(LED1, LOW);
    delayRTOS(700);
  }
}

// -----------------------------------------------------------
//  Tarea 2: Comunicación Bluetooth
//  - Lee caracteres del BT y controla el ROBOT
// -----------------------------------------------------------
void Task2code(void *pvParameters)
{
  Serial.print("Tarea 2 (BT) corriendo en el core ");
  Serial.println(xPortGetCoreID());

  SerialBT.println("ESP32 listo. Envia comandos (F,B,L,R,S)...");

  for (;;)
  {
    if (SerialBT.available())
    {
      char c = SerialBT.read();

      // Imprimir todo comando que llegue
      Serial.print("[BT RX] ");
      Serial.println(c);

      switch (c)
      {
        case 'F':   // Avanzar
        case 'f':
          robotForward();
          SerialBT.println("FORWARD");
          break;

        case 'B':   // Retroceder
        case 'b':
          robotBackward();
          SerialBT.println("BACKWARD");
          break;

        case 'L':   // Girar izquierda en sitio
        case 'l':
          robotLeft();
          SerialBT.println("LEFT");
          break;

        case 'R':   // Girar derecha en sitio
        case 'r':
          robotRight();
          SerialBT.println("RIGHT");
          break;

        // ===== NUEVOS COMANDOS COMBINADOS =====
        case 'H':   // Adelante + derecha
        case 'h':
          robotForwardRight();
          SerialBT.println("FORWARD+RIGHT");
          break;

        case 'G':   // Adelante + izquierda
        case 'g':
          robotForwardLeft();
          SerialBT.println("FORWARD+LEFT");
          break;

        case 'J':   // Atrás + derecha
        case 'j':
          robotBackwardRight();
          SerialBT.println("BACKWARD+RIGHT");
          break;

        case 'I':   // Atrás + izquierda
        case 'i':
          robotBackwardLeft();
          SerialBT.println("BACKWARD+LEFT");
          break;
        // ======================================

        case 'S':   // Stop
        case 's':
          robotStop();
          SerialBT.println("STOP");
          break;

        default:
          SerialBT.print("Comando desconocido: ");
          SerialBT.println(c);
          break;
      }

    }

    // Pequeña pausa para no saturar la CPU
    delayRTOS(5);
  }
}

// -----------------------------------------------------------
//  Tarea 3: Log periódico por Serial (USB)
// -----------------------------------------------------------
void Task3code(void *pvParameters)
{
  Serial.print("Tarea 3 (Log) corriendo en el core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    // Puedes descomentar si quieres log periódico
    // Serial.println(F("[LOG] Sistema corriendo. Envia comandos por Bluetooth."));
    delayRTOS(1000);
  }
}

// ===================== FUNCIONES DEL ROBOT =====================

// Sin PWM: EN siempre HIGH cuando el motor está activo, LOW cuando se detiene.

void robotStop()
{
  // Apaga EN y líneas de dirección
  digitalWrite(M1EN, LOW);
  digitalWrite(M2EN, LOW);

  digitalWrite(M1A, LOW);
  digitalWrite(M1B, LOW);
  digitalWrite(M2A, LOW);
  digitalWrite(M2B, LOW);

  // Opcional: apaga LEDs de movimiento
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
}

void robotForward()
{
  // Direcciones: ambos motores hacia adelante
  digitalWrite(M1A, HIGH);  // M1 adelante
  digitalWrite(M1B, LOW);
  digitalWrite(M2A, HIGH);  // M2 adelante
  digitalWrite(M2B, LOW);

  // Habilitar motores
  digitalWrite(M1EN, HIGH);
  digitalWrite(M2EN, HIGH);

  // LED3 como indicador "adelante"
  digitalWrite(LED3, HIGH);
  digitalWrite(LED2, LOW);
  digitalWrite(LED4, LOW);
}

void robotBackward()
{
  // Direcciones: ambos motores hacia atrás
  digitalWrite(M1A, LOW);
  digitalWrite(M1B, HIGH);  // M1 atrás
  digitalWrite(M2A, LOW);
  digitalWrite(M2B, HIGH);  // M2 atrás

  // Habilitar motores
  digitalWrite(M1EN, HIGH);
  digitalWrite(M2EN, HIGH);

  // LED4 como indicador "atrás"
  digitalWrite(LED4, HIGH);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
}

void robotLeft()
{
  // Giro en sitio hacia la derecha:
  // motor izquierdo adelante, motor derecho atrás
  digitalWrite(M1A, HIGH);  // M1 adelante
  digitalWrite(M1B, LOW);
  digitalWrite(M2A, LOW);
  digitalWrite(M2B, HIGH);  // M2 atrás

  digitalWrite(M1EN, HIGH);
  digitalWrite(M2EN, HIGH);

  // LED2 como indicador "derecha"
  digitalWrite(LED2, HIGH);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
}

void robotRight()
{
  // Giro en sitio hacia la izquierda:
  // motor izquierdo atrás, motor derecho adelante
  digitalWrite(M1A, LOW);
  digitalWrite(M1B, HIGH);  // M1 atrás
  digitalWrite(M2A, HIGH);  // M2 adelante
  digitalWrite(M2B, LOW);

  digitalWrite(M1EN, HIGH);
  digitalWrite(M2EN, HIGH);

  // LED2 también, si quieres diferenciar puedes usar combinación de LEDs
  digitalWrite(LED2, HIGH);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
}


// =====================================================================
//  MOVIMIENTOS COMBINADOS (AVANZAR/RETROCEDER MIENTRAS GIRA)
//  - Se apaga la rueda interna (EN = LOW) y se deja activa la externa.
// =====================================================================

// Adelante + izquierda: avanza haciendo curva hacia la izquierda
//  -> motor derecho adelante, motor izquierdo detenido
void robotForwardLeft()
{
  // Motor izquierdo adelante
  digitalWrite(M1A, HIGH);
  digitalWrite(M1B, LOW);
  digitalWrite(M1EN, HIGH);   // activo

  // Motor derecho detenido
  digitalWrite(M2EN, LOW);
  digitalWrite(M2A, LOW);
  digitalWrite(M2B, LOW);

  // Indicadores
  digitalWrite(LED3, HIGH);   // adelante
  digitalWrite(LED2, HIGH);   // derecha
  digitalWrite(LED4, LOW);
}

// Adelante + derecha: avanza haciendo curva hacia la derecha
//  -> motor izquierdo adelante, motor derecho detenido
void robotForwardRight()
{
  // Motor derecho adelante
  digitalWrite(M2A, HIGH);
  digitalWrite(M2B, LOW);
  digitalWrite(M2EN, HIGH);   // activo

  // Motor izquierdo detenido
  digitalWrite(M1EN, LOW);
  digitalWrite(M1A, LOW);
  digitalWrite(M1B, LOW);

  // Indicadores
  digitalWrite(LED3, HIGH);   // adelante
  digitalWrite(LED2, HIGH);   // izquierda (reutilizamos LED2)
  digitalWrite(LED4, LOW);
}

// Atrás + izquierda: retrocede haciendo curva hacia la izquierda
//  -> motor derecho atrás, motor izquierdo detenido
void robotBackwardLeft()
{
  // Motor izquierdo atrás
  digitalWrite(M1A, LOW);
  digitalWrite(M1B, HIGH);
  digitalWrite(M1EN, HIGH);   // activo

  // Motor derecho detenido
  digitalWrite(M2EN, LOW);
  digitalWrite(M2A, LOW);
  digitalWrite(M2B, LOW);

  // Indicadores
  digitalWrite(LED4, HIGH);   // atrás
  digitalWrite(LED2, HIGH);   // derecha
  digitalWrite(LED3, LOW);
}

// Atrás + derecha: retrocede haciendo curva hacia la derecha
//  -> motor izquierdo atrás, motor derecho detenido
void robotBackwardRight()
{
  // Motor derecho atrás
  digitalWrite(M2A, LOW);
  digitalWrite(M2B, HIGH);
  digitalWrite(M2EN, HIGH);   // activo

  // Motor izquierdo detenido
  digitalWrite(M1EN, LOW);
  digitalWrite(M1A, LOW);
  digitalWrite(M1B, LOW);

  // Indicadores
  digitalWrite(LED4, HIGH);   // atrás
  digitalWrite(LED2, HIGH);   // izquierda
  digitalWrite(LED3, LOW);
}
