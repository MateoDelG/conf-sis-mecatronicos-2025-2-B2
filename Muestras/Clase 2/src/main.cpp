#include <Arduino.h>

TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

// Pines de LEDs
const int led1 = 15; // LED principal (Core 0)
const int led2 = 2;  // LED secundario (Core 1)
const int led3 = 4;  // LED secundario (Core 1)

// Variable compartida
volatile int contador = 0;
// --------------------------------------------------------------
//  ¿Qué es una variable 'volatile'?
// Una variable declarada como 'volatile' le indica al compilador
// que su valor puede cambiar en cualquier momento, incluso fuera
// del flujo normal del programa (por ejemplo, dentro de una tarea
// diferente, una interrupción o un segundo núcleo del procesador).
//
// Esto evita que el compilador "optimice" la lectura de la variable
// y use un valor almacenado en caché, asegurando que siempre se lea
// su valor real directamente desde la memoria.
//
// En este ejemplo, 'contador' es 'volatile' porque es usada por
// dos tareas que corren en núcleos distintos (Core 0 y Core 1).
// --------------------------------------------------------------

void Task1code(void *pvParameters);
void Task2code(void *pvParameters);
void Task3code(void *pvParameters);
void setupPins();
void setupTasks();
void delayRTOS(uint32_t ms);
void blinkLED3();

// ========================== SETUP ==========================
void setup()
{
  Serial.begin(115200);
  delay(500);
  setupPins();
  setupTasks();
  Serial.println("\n--- DEMO DUAL CORE - ESP32 ---");
}
void loop(){}

void setupTasks()
{
  // Crear tarea 1 (Core 0)
  xTaskCreatePinnedToCore(
      Task1code,         // Función
      "Tarea_Principal", // Nombre
      5000,              // Tamaño de stack
      NULL,              // Parámetro
      1,                 // Prioridad
      &Task1,            // Handle
      0);                // Núcleo 0
  delay(500);

  // Crear tarea 2 (Core 1)
  xTaskCreatePinnedToCore(
      Task2code,
      "Tarea_dos",
      5000,
      NULL,
      1,
      &Task2,
      1); // Núcleo 1
  delay(500);

  // Crear tarea 2 (Core 1)
  xTaskCreatePinnedToCore(
      Task3code,
      "Tarea_tres",
      5000,
      NULL,
      1,
      &Task3,
      1); // Núcleo 1
  delay(500);
}

void setupPins()
{
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
}
// --------------------------------------------------------------
//  ¿Por qué usar 'vTaskDelay()' en lugar de 'delay()'?
// En FreeRTOS, 'vTaskDelay()' suspende la tarea actual durante un
// número determinado de "ticks" del sistema, pero sin bloquear la CPU.
//
// Esto significa que mientras una tarea espera, el planificador de
// FreeRTOS puede ejecutar otras tareas en paralelo, aprovechando ambos
// núcleos del ESP32.
//
// En cambio, 'delay()' detiene completamente la ejecución del hilo
// donde se llama (bloquea el CPU), impidiendo que otras tareas corran
// y rompiendo el esquema multitarea.
//
// Por eso, en sistemas con FreeRTOS o múltiples núcleos, se debe usar:
//     vTaskDelay(pdMS_TO_TICKS(tiempo_ms));
// para lograr una pausa precisa y eficiente.
// --------------------------------------------------------------
void delayRTOS(uint32_t ms)
{
  vTaskDelay(ms / portTICK_PERIOD_MS);
}

// ======================= TAREA 1 ==========================
void Task1code(void *pvParameters)
{
  Serial.print("Tarea 1 corriendo en el core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {

    digitalWrite(led1, HIGH);
    delayRTOS(100);
    digitalWrite(led1, LOW);
    delayRTOS(100);

    contador++;
    Serial.printf("[Core %d] Contador = %d\n", xPortGetCoreID(), contador);
  }
}

// ======================= TAREA 2 ==========================
void Task2code(void *pvParameters)
{
  Serial.print("Tarea 2 corriendo en el core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    // LED parpadea más rápido
    digitalWrite(led2, HIGH);
    delayRTOS(1000);
    digitalWrite(led2, LOW);
    delayRTOS(1000);

    // Lee el valor compartido
    Serial.printf("[Core %d] LED2 activo | Contador leído = %d\n", xPortGetCoreID(), contador);
  }
}

void Task3code(void *pvParameters)
{
  Serial.print("Tarea 3 corriendo en el core ");
  Serial.println(xPortGetCoreID());
  for (;;)
  {
    blinkLED3();
    // Cede la CPU por un breve periodo para evitar busy-loop
    delayRTOS(10);
  }
}

void blinkLED3()
{
    // temporización no bloqueante con millis() para led3
    static unsigned long previousMillis = 0;
    const unsigned long interval = 3000; // ms
    static bool ledState = false;

    unsigned long now = millis();
    if (now - previousMillis >= interval)
    {
      previousMillis = now;
      ledState = !ledState;
      digitalWrite(led3, ledState ? HIGH : LOW);
      // --------------------------------------------------------------
      //  Representación condicional con el operador ternario:
      //    digitalWrite(led3, ledState ? HIGH : LOW);
      //
      // Esta línea equivale a:
      //    if (ledState) {
      //        digitalWrite(led3, HIGH);  // Enciende el LED
      //    } else {
      //        digitalWrite(led3, LOW);   // Apaga el LED
      //    }
      //
      // El operador '? :' evalúa la condición antes del signo de pregunta:
      //    (condición) ? valor_si_verdadero : valor_si_falso;
      //
      // En este caso:
      //    Si ledState es TRUE → se envía HIGH a digitalWrite()
      //    Si ledState es FALSE → se envía LOW
      //
      // Es una forma compacta y legible de escribir una estructura if–else
      // cuando se necesita ejecutar una sola instrucción.
      // --------------------------------------------------------------

      // Lee el valor compartido
      Serial.printf("[Core %d] LED3 %s | Contador leído = %d\n", xPortGetCoreID(), ledState ? "ON" : "OFF", contador);

      while(true){
        Serial.println("bloqueado en tarea 3");
        delay(500);
      }
    }
}
