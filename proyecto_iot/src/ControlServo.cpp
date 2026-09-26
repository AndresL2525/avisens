#include "ControlServo.h"

ControlServo::ControlServo()
    : estado_(EstadoPuerta::CERRADA),
      tiempoEstado_(0),
      ahora_(0)
{
}

void ControlServo::begin()
{
  servo_.attach(SERVO_PIN);
  escribirServoCuidado(ANGULO_CERRADA);
  delay(500);  // Deja al servo alcanzar la posición inicial antes de arrancar las tareas
  LOG_DEBUG("ControlServo inicializado en posición CERRADA (0°)");
}

void ControlServo::actualizar(bool hayPresencia)
{
  ahora_ = millis();

  switch (estado_)
  {
  case EstadoPuerta::CERRADA:
    if (hayPresencia)
    {
      transicionar(EstadoPuerta::ABRIENDO);
      escribirServoCuidado(ANGULO_ABIERTA);
      LOG_DEBUG("Puerta: ABRIENDO a la derecha (90°)...");
    }
    break;

  case EstadoPuerta::ABRIENDO:
    if (ahora_ - tiempoEstado_ >= SERVO_DURACION_GIRO)
    {
      transicionar(EstadoPuerta::ABIERTA);
      LOG_DEBUG("Puerta: ABIERTA completamente");
    }
    break;

  case EstadoPuerta::ABIERTA:
    if (hayPresencia)
    {
      tiempoEstado_ = ahora_;
    }
    else if (ahora_ - tiempoEstado_ >= SERVO_TIEMPO_ABIERTA)
    {
      transicionar(EstadoPuerta::CERRANDO);
      escribirServoCuidado(ANGULO_CERRADA);
      LOG_DEBUG("Puerta: CERRANDO a la izquierda (0°)...");
    }
    break;

  case EstadoPuerta::CERRANDO:
    if (ahora_ - tiempoEstado_ >= SERVO_DURACION_GIRO)
    {
      transicionar(EstadoPuerta::CERRADA);
      LOG_DEBUG("Puerta: CERRADA");
    }
    break;
  }
}

void ControlServo::cerrarEmergencia()
{
  transicionar(EstadoPuerta::CERRANDO);
  escribirServoCuidado(ANGULO_CERRADA);
  LOG_WARN("Puerta cerrada por emergencia (0°)");
}

void ControlServo::reset()
{
  estado_ = EstadoPuerta::CERRADA;
  tiempoEstado_ = millis();
  escribirServoCuidado(ANGULO_CERRADA);
}

void ControlServo::transicionar(EstadoPuerta nuevoEstado)
{
  estado_ = nuevoEstado;
  tiempoEstado_ = millis();
}

void ControlServo::escribirServoCuidado(uint8_t angulo)
{
  servo_.write(angulo);
  vTaskDelay(pdMS_TO_TICKS(1));  // Cede el core mientras el canal PWM del servo conmuta
}
