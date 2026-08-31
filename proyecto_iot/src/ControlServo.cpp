#include "ControlServo.h"

ControlServo::ControlServo()
    : estado_(EstadoPuerta::CERRADA),
      tiempoEstado_(0),
      ahora_(0) {
}

void ControlServo::begin() {
  servo_.attach(SERVO_PIN);
  servo_.write(SERVO_NEUTRO);
  delay(500);  // Estabilizar servo
  LOG_DEBUG("ControlServo inicializado");
}

void ControlServo::actualizar(bool hayPresencia) {
  ahora_ = millis();

  switch (estado_) {
    case EstadoPuerta::CERRADA:
      if (hayPresencia) {
        transicionar(EstadoPuerta::ABRIENDO);
        escribirServoCuidado(0);  // Rotar a 0° (abrir)
        LOG_DEBUG("Puerta: ABRIENDO...");
      }
      break;

    case EstadoPuerta::ABRIENDO:
      if (ahora_ - tiempoEstado_ >= SERVO_DURACION_GIRO) {
        transicionar(EstadoPuerta::ABIERTA);
        escribirServoCuidado(SERVO_NEUTRO);  // Neutral
        LOG_DEBUG("Puerta: ABIERTA");
      }
      break;

    case EstadoPuerta::ABIERTA:
      if (ahora_ - tiempoEstado_ >= SERVO_TIEMPO_ABIERTA) {
        transicionar(EstadoPuerta::CERRANDO);
        escribirServoCuidado(180);  // Rotar a 180° (cerrar)
        LOG_DEBUG("Puerta: CERRANDO...");
      }
      break;

    case EstadoPuerta::CERRANDO:
      if (ahora_ - tiempoEstado_ >= SERVO_DURACION_GIRO) {
        transicionar(EstadoPuerta::CERRADA);
        escribirServoCuidado(SERVO_NEUTRO);  // Neutral
        LOG_DEBUG("Puerta: CERRADA");
      }
      break;
  }
}

void ControlServo::cerrarEmergencia() {
  transicionar(EstadoPuerta::CERRANDO);
  escribirServoCuidado(180);
  LOG_WARN("Puerta cerrada por emergencia");
}

void ControlServo::reset() {
  estado_ = EstadoPuerta::CERRADA;
  tiempoEstado_ = millis();
  escribirServoCuidado(SERVO_NEUTRO);
}

void ControlServo::transicionar(EstadoPuerta nuevoEstado) {
  estado_ = nuevoEstado;
  tiempoEstado_ = millis();
}

void ControlServo::escribirServoCuidado(uint8_t angulo) {
  servo_.write(angulo);
  // Pequeño yield para permitir que FreeRTOS responda
  vTaskDelay(pdMS_TO_TICKS(1));
}
