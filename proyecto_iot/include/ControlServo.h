#ifndef CONTROL_SERVO_H
#define CONTROL_SERVO_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"

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
