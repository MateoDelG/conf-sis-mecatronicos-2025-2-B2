#include <Arduino.h>
#include <BluetoothSerial.h>

// ===================== Bluetooth =====================
BluetoothSerial SerialBT;   // Objeto para Serial Bluetooth

// ===================== LEDs =========================
// LEDs (controlados via BT)
const int LED1 = 4;
const int LED2 = 5;    // ojo: GPIO0 es pin de arranque
const int LED3 = 18;
const int LED4 = 19;

// ==========================================================
//  FreeRTOS: Tareas (no usamos punteros; solo los handles)
// ==========================================================
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

// -----------------------------------------------------------
//  Prototipos
// -----------------------------------------------------------
void Task1code(void *pvParameters);
void Task2code(void *pvParameters);
void Task3code(void *pvParameters);

void setupTasks();
void delayRTOS(uint32_t ms);
void setupPins();

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
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);

  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
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
    delayRTOS(1000);

  }
}

// -----------------------------------------------------------
//  Tarea 2: Comunicación Bluetooth
//  - Lee caracteres del BT y controla los LEDs
// -----------------------------------------------------------
void Task2code(void *pvParameters)
{
  Serial.print("Tarea 2 (BT) corriendo en el core ");
  Serial.println(xPortGetCoreID());

  SerialBT.println("ESP32 listo. Envia comandos...");

  for (;;)
  {
    if (SerialBT.available())
    {
      char c = SerialBT.read();

      // 🔥 NUEVO: IMPRIMIR TODO COMANDO QUE LLEGUE
      Serial.print("[BT RX] ");  
      Serial.println(c);

      switch (c)
      {
        case 'R': digitalWrite(LED1, HIGH); SerialBT.println("LED1 ON"); break;

        case 'L': digitalWrite(LED2, HIGH); SerialBT.println("LED2 ON"); break;

        case 'F': digitalWrite(LED3, HIGH); SerialBT.println("LED3 ON"); break;

        case 'B': digitalWrite(LED4, HIGH); SerialBT.println("LED4 ON"); break;

        case 'S':
          digitalWrite(LED1, LOW);
          digitalWrite(LED2, LOW);
          digitalWrite(LED3, LOW);
          digitalWrite(LED4, LOW);
          SerialBT.println("ALL OFF");
          break;

        default:
          SerialBT.print("Comando desconocido: ");
          SerialBT.println(c);
          break;
      }
    }

    // delayRTOS(20);
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
    // Serial.println(F("[LOG] Sistema corriendo. Envia comandos por Bluetooth."));
    delayRTOS(1000);
  }
}
