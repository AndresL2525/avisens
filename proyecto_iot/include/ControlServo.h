#ifndef CONTROL_SERVO_H
#define CONTROL_SERVO_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"

#ifndef ANGULO_CERRADA
#define ANGULO_CERRADA 0
#endif

#ifndef ANGULO_ABIERTA
#define ANGULO_ABIERTA 90
#endif

#ifndef SERVO_PIN
#define SERVO_PIN 18
#endif

#ifndef SERVO_DURACION_GIRO
#define SERVO_DURACION_GIRO 600
#endif

#ifndef SERVO_TIEMPO_ABIERTA
#define SERVO_TIEMPO_ABIERTA 3000
#endif

#ifndef LOG_DEBUG
#define LOG_DEBUG(msg) Serial.println(F("[DEBUG] " msg))
#endif
#ifndef LOG_WARN
#define LOG_WARN(msg) Serial.println(F("[WARN]  " msg))
#endif
#ifndef LOG_ERROR
#define LOG_ERROR(msg) Serial.println(F("[ERROR] " msg))
#endif

class ControlServo
{
public:
  ControlServo();

  void begin();
  void actualizar(bool hayPresencia);
  EstadoPuerta getEstado() const { return estado_; }
  void cerrarEmergencia();
  void reset();

private:
  Servo servo_;
  EstadoPuerta estado_;
  unsigned long tiempoEstado_;
  unsigned long ahora_;

  void transicionar(EstadoPuerta nuevoEstado);
  void escribirServoCuidado(uint8_t angulo);
};

#endif