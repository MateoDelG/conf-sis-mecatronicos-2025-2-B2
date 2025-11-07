#include <Arduino.h>

TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

static const int TRIG_PIN = 5;
static const int ECHO_PIN = 18;

// -----------------------------------------------------------
//  Filtro de Kalman
//  - Q: "ruido del proceso" → qué tan incierto es el MODELO
//  - R: "ruido de medición" → qué tan incierto es el SENSOR
//  - X: estado estimado actual (nuestra "mejor" distancia en cm)
//  - P: error de la estimación (varianza de X)
//  - K: ganancia de Kalman (peso entre medición y predicción)
//  Notas:
//   * 'volatile' evita que el compilador optimice en exceso variables
//     que podrían ser usadas desde múltiples contextos (p. ej. tareas).
// -----------------------------------------------------------
volatile float Q = 0.01f;
volatile float R = 3.0f;

volatile float X = 0.0f;
volatile float P = 0.0f;
volatile float K = 0.0f;


void Task1code( void * pvParameters );
void Task2code( void * pvParameters );
void Task3code( void * pvParameters );


void setupTasks();
void setupPins();

float readUltrasonicDistanceCM();
float kalmanUpdate(float measurement);

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
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void delayRTOS(int ms){
  vTaskDelay(ms / portTICK_PERIOD_MS);
}

void Task1code( void * pvParameters ){
  Serial.println("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    float distanceCM = readUltrasonicDistanceCM();
    float filteredDistanceCM = kalmanUpdate(distanceCM);

  Serial.print(">Raw Sensor: ");
  Serial.println(distanceCM);
  Serial.print(">Kalman Filtered: ");
  Serial.println(filteredDistanceCM);


    delayRTOS(10);
  }
}

void Task2code( void * pvParameters ){
  Serial.println("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){

  }
}

void Task3code( void * pvParameters ){
  Serial.println("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){

  }
}

float readUltrasonicDistanceCM(){
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  float distanceCM = (duration / 2.0) * 0.0343;
  return distanceCM;
}


// -----------------------------------------------------------
//  Filtro de Kalman
//  - Q: "ruido del proceso" → qué tan incierto es el MODELO
//  - R: "ruido de medición" → qué tan incierto es el SENSOR
//  - X: estado estimado actual (nuestra "mejor" distancia en cm)
//  - P: error de la estimación (varianza de X)
//  - K: ganancia de Kalman (peso entre medición y predicción)
//  Notas:
//   * 'volatile' evita que el compilador optimice en exceso variables
//     que podrían ser usadas desde múltiples contextos (p. ej. tareas).
// -----------------------------------------------------------
// -----------------------------------------------------------
//  Paso de actualización del filtro de Kalman:
//   - measurement: lectura del sensor (cm)
//   - Si measurement = -1 (nuestro "marcador de error") o NaN,
//     NO se actualiza el estado y se devuelve X tal como está.
//   - En otro caso, se realiza el ciclo estándar:
//       P = P + Q
//       K = P / (P + R)
//       X = X + K * (measurement - X)
//       P = (1 - K) * P
// -----------------------------------------------------------
float kalmanUpdate(float measurement){
  // Predicción
  P = P + Q;

  // Si la medición es inválida, no actualizar
  if (isnan(measurement) || measurement == -1.0f) {
    return X;
  }

  // Cálculo de la ganancia de Kalman
  K = P / (P + R);

  // Actualización con la medición
  X = X + K * (measurement - X);

  // Actualización del error de la estimación
  P = (1 - K) * P;

  return X;
}