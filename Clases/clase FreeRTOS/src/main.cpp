#include <Arduino.h>

TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;


const int led1 = 15;
const int led2 = 2;
const int led3 = 4;


void Task1code( void * pvParameters );
void Task2code( void * pvParameters );
void Task3code( void * pvParameters );


void setupTasks();
void setupPins();


void setup() {
Serial.begin(115200);
setupTasks();
setupPins();
delay(500);

}
void loop() {}

void setupTasks(){
  xTaskCreatePinnedToCore(
   Task1code,
    "Task_1",
    5000,
    NULL,
    1,
    &Task1,
    0); // Pin the task to core 0

  xTaskCreatePinnedToCore(
   Task2code,
    "Task_2",
    5000,
    NULL,
    1,
    &Task2,
    1); // Pin the task to core 1

  xTaskCreatePinnedToCore(
   Task3code,
    "Task_3",
    5000,
    NULL,
    1,
    &Task3,
    1); // Pin the task to core 1
}
void setupPins(){
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
void delayRTOS(int ms){
  vTaskDelay(ms / portTICK_PERIOD_MS);
}

void Task1code( void * pvParameters ){
  Serial.println("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    Serial.println("Hello from Task1");

    digitalWrite(led1, HIGH);
    delayRTOS(1000);
    digitalWrite(led1, LOW);
    delayRTOS(1000);
  }
}

void Task2code( void * pvParameters ){
  Serial.println("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    Serial.println("Hola desde la tarea 2");
    digitalWrite(led2, HIGH);
    delayRTOS(100);
    digitalWrite(led2, LOW);
    delayRTOS(100);
  }
}

void Task3code( void * pvParameters ){
  Serial.println("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    Serial.println("Hola desde la tarea 3");
    digitalWrite(led3, HIGH);
    delayRTOS(3000);
    digitalWrite(led3, LOW);
    delayRTOS(3000);

    while(true){
      Serial.println("Tarea 3 en bucle infinito");
      digitalWrite(led3, HIGH);
      delay(50);
    }
  }
}