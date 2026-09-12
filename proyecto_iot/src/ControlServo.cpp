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
  // Inicia cerrada (a la izquierda)
  escribirServoCuidado(ANGULO_CERRADA);
  delay(500); // Estabilizar servo en posición inicial
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
      escribirServoCuidado(ANGULO_ABIERTA); // Rota a la derecha (90°)
      LOG_DEBUG("Puerta: ABRIENDO a la derecha (90°)...");
    }
    break;

  case EstadoPuerta::ABRIENDO:
    // Espera el tiempo de recorrido mecánico del servo
    if (ahora_ - tiempoEstado_ >= SERVO_DURACION_GIRO)
    {
      transicionar(EstadoPuerta::ABIERTA);
      LOG_DEBUG("Puerta: ABIERTA completamente");
    }
    break;

  case EstadoPuerta::ABIERTA:
    // Si el sensor detecta presencia continua, reinicia la cuenta para evitar cerrarse encima
    if (hayPresencia)
    {
      tiempoEstado_ = ahora_;
    }
    else if (ahora_ - tiempoEstado_ >= SERVO_TIEMPO_ABIERTA)
    {
      transicionar(EstadoPuerta::CERRANDO);
      escribirServoCuidado(ANGULO_CERRADA); // Vuelve a la izquierda (0°)
      LOG_DEBUG("Puerta: CERRANDO a la izquierda (0°)...");
    }
    break;

  case EstadoPuerta::CERRANDO:
    // Espera que termine de regresar antes de permitir otra apertura
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
  vTaskDelay(pdMS_TO_TICKS(1));
}