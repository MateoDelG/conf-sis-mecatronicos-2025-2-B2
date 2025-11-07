#include <Arduino.h>
#include <Servo.h>

// -----------------------------------------------------------
//  Handles (identificadores) para las tareas de FreeRTOS.
//  Sirven para gestionar cada tarea (suspender, borrar, etc.)
// -----------------------------------------------------------
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

// -----------------------------------------------------------
//  Pines de hardware
//  TRIG/ECHO: sensor ultrasónico HC-SR04
//  SERVO_PIN: señal de control del servomotor
// -----------------------------------------------------------
static const int TRIG_PIN  = 5;
static const int ECHO_PIN  = 18;
static const int SERVO_PIN = 15;

Servo servoMotor; // Objeto que maneja el servo

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
volatile float Q = 0.001f;
volatile float R = 5.0f;
volatile float X = 0.0f;
volatile float P = 0.0f;
volatile float K = 0.0f;

// -----------------------------------------------------------
//  Prototipos de funciones para mantener el orden del código
// -----------------------------------------------------------
void Task1code(void *pvParameters);
void Task2code(void *pvParameters);
void Task3code(void *pvParameters);
void setupPins();
void setupTasks();
void delayRTOS(uint32_t ms);

float readDistanceCM();
void kalmanInit();
float kalmanUpdate(float measurement);

// ========================== SETUP ==========================
//  Se ejecuta una vez al encender o resetear la placa.
// ==========================================================
void setup()
{
  Serial.begin(115200);  // Velocidad del puerto serie (para Teleplot/depuración)
  delay(500);            // Pequeña espera para estabilizar puerto serie
  setupPins();           // Configuración de pines y servo
  setupTasks();          // Creación de tareas en los dos núcleos del ESP32
}

//  loop() queda vacío porque el trabajo lo hacen las tareas FreeRTOS.
void loop(){}

// -----------------------------------------------------------
//  Crea y lanza las tareas. Cada xTaskCreatePinnedToCore:
//
//  - Task1code:  Core 0 → tarea principal (lee sensor, filtra y mueve servo)
//  - Task2code:  Core 1 → tarea secundaria sin uso
//  - Task3code:  Core 1 → tarea terciaria sin uso
// -----------------------------------------------------------
void setupTasks()
{
  // Crear tarea 1 (Core 0)
  xTaskCreatePinnedToCore(
      Task1code,         // Función que ejecutará la tarea
      "Tarea_Principal", // Nombre descriptivo
      5000,              // Tamaño de stack (bytes) para esta tarea
      NULL,              // Parámetro (no usamos)
      1,                 // Prioridad (1 es baja; valores mayores = más prioridad)
      &Task1,            // Handle para referenciar la tarea
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

  // Crear tarea 3 (Core 1)
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

// -----------------------------------------------------------
//  Configuración de pines de IO y vínculo del servo a su pin
// -----------------------------------------------------------
void setupPins()
{
  pinMode(TRIG_PIN, OUTPUT); // TRIG envía un pulso de 10 µs
  pinMode(ECHO_PIN, INPUT);  // ECHO recibe el pulso devuelto
  servoMotor.attach(SERVO_PIN); // Asocia el objeto 'servoMotor' al pin
}

// -----------------------------------------------------------
//  Retardo amigable para FreeRTOS (no bloquea completamente).
//  Convierte milisegundos a "ticks" del RTOS y cede CPU.
// -----------------------------------------------------------
void delayRTOS(uint32_t ms)
{
  vTaskDelay(ms / portTICK_PERIOD_MS);
}

// ======================= TAREA 1 ==========================
//  Tarea principal (Core 0):
//   1) Lee distancia con HC-SR04
//   2) Aplica filtro de Kalman (evita NaN con marcador -1)
//   3) Publica datos a Teleplot (RAW y KALMAN)
//   4) Mapea distancia filtrada a ángulo de servo y lo mueve
// ==========================================================
void Task1code(void *pvParameters)
{
  Serial.print("Tarea 1 corriendo en el core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    // 1) Medir distancia en centímetros
    float raw = readDistanceCM();

    // Si no se recibe eco (timeout) o llega NaN, usamos -1 como "marcador de error".
    // Este valor es muy útil para Teleplot, pues no rompe la gráfica.
    if ((raw < 0) || isnan(raw)){
      raw = -1;  // marcador de error en Teleplot
    }

    // 2) Actualizar estimación con Kalman.
    //    Si raw = -1, la función devuelve el estado previo (no actualiza).
    float filtered = kalmanUpdate(raw);

    // 3) Enviar datos a Teleplot.
    //    Teleplot detecta series con el prefijo ">NOMBRE_SERIE:"
    Serial.print(">RAW:");
    Serial.println(raw);
    Serial.print(">KALMAN:");
    Serial.println(filtered);

    // 4) Control del servo:
    //    Tomamos la distancia filtrada (más estable) para evitar vibraciones.
    //    - Si 'filtered' < 0 (sin dato), usamos X (última estimación).
    //    - Restringimos distancia útil entre 10 y 50 cm para el mapeo.
    //    - Mapeamos 5–40 cm a 180–0° (cerca = 180°, lejos = 0°).
    float d = filtered;          // Fuente principal para el control
    // float d = raw;
    if (d < 0) d = X;            // Si no hay dato, usa último estimado
    d = constrain(d, 5.0f, 40.0f);

    int angle = map((int)d, 5, 40, 180, 0); 
    servoMotor.write(angle);                // Mueve el servo al ángulo calculado

    // Publicamos también el ángulo a Teleplot para ver la respuesta del actuador
    Serial.print(">ANGLE:");
    Serial.println(angle);
  }
}


void Task2code(void *pvParameters)
{
  Serial.print("Tarea 2 corriendo en el core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    delayRTOS(10);  // Cede CPU durante ~1 segundo
  }
}

void Task3code(void *pvParameters)
{
  Serial.print("Tarea 3 corriendo en el core ");
  Serial.println(xPortGetCoreID());
  for (;;)
  {
    delayRTOS(10); // Pequeño retardo cooperativo
  }
}

// -----------------------------------------------------------
//  Lectura de distancia con HC-SR04 (en cm):
//   1) TRIG: pulso de 10 µs
//   2) ECHO: medimos duración del pulso alto de retorno
//   3) Convertimos tiempo (µs) a distancia (cm) con v≈0.034 cm/µs
//   4) Si no hay eco en 30 ms o la lectura es inválida,
//      devuelve el último valor válido (para evitar saltos en la señal)
// -----------------------------------------------------------
float readDistanceCM() {
  // Variable estática: conserva su valor entre llamadas
  static float lastValid = 0.0f;

  // Asegura nivel bajo antes del pulso
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Pulso de 10 µs para disparar la medición
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Timeout 30 ms ~ 5 m máx aprox. (evita bloqueos largos)
  unsigned long dur = pulseIn(ECHO_PIN, HIGH, 30000UL);

  // Si no se recibe eco o el valor es absurdo, usar el último válido
  if (dur == 0) {
    return lastValid;
  }

  // Distancia (cm) = (tiempo_µs * velocidad_sonido_cm/µs) / 2
  float distance = (dur * 0.034f) / 2.0f;

  // Validación adicional: descartar lecturas fuera de rango útil
  if (isnan(distance) || distance <= 2.0f || distance > 400.0f) {
    return lastValid;
  }

  // Si todo está bien, actualizar y devolver la nueva lectura
  lastValid = distance;
  return distance;
}


// -----------------------------------------------------------
//  Inicializa X con la primera medición válida.
//  Útil para evitar "saltos" iniciales del estimador.
// -----------------------------------------------------------
void kalmanInit(){
  float d0 = NAN;
  unsigned long t0 = millis();

  // Intentamos leer por hasta 1.5 s
  while (isnan(d0) && (millis() - t0 < 1500)) {
    d0 = readDistanceCM();
  }
  if (!isnan(d0)) {
    X = d0;  // estado inicial = primera distancia válida
  }
}

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
float kalmanUpdate(float measurement) {
  // Ignorar mediciones inválidas (marcador -1 o NaN).
  // Devolvemos X (última estimación) sin modificarla.
  if (measurement < 0 || isnan(measurement)) {
    return X;
  }

  // 1) Predicción de la incertidumbre: aumentamos P por la incertidumbre del proceso
  P = P + Q;

  // 2) Ganancia de Kalman: cuánto confiamos en el sensor vs el modelo
  K = P / (P + R);

  // 3) Corrección: acercamos X hacia la medición, ponderado por K
  X = X + K * (measurement - X);

  // 4) Actualización del error de estimación
  P = (1.0f - K) * P;

  // Regresamos la nueva mejor estimación
  return X;
}
