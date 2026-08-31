#include "Alimentador.h"

Alimentador::Alimentador()
    : estado_(EstadoAlimentador::APAGADO),
      tiempoEstado_(0),
      ahora_(0),
      habilitado_(true) {
}

void Alimentador::begin() {
  pinMode(EN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);

  detenerMotor();  // Dejar en estado seguro
  LOG_DEBUG("Alimentador inicializado");
}

void Alimentador::actualizar() {
  ahora_ = millis();

  if (!habilitado_) {
    // Si está deshabilitado, forzar apagado
    if (estado_ != EstadoAlimentador::APAGADO) {
      detenerMotor();
      transicionar(EstadoAlimentador::APAGADO);
    }
    return;
  }

  switch (estado_) {
    case EstadoAlimentador::APAGADO:
      if (ahora_ - tiempoEstado_ >= INTERVALO_ALIMENTO) {
        transicionar(EstadoAlimentador::ENCENDIDO);
        iniciarMotor(128);  // 50% PWM
        LOG_DEBUG("Alimentador: ON (50%)");
      }
      break;

    case EstadoAlimentador::ENCENDIDO:
      if (ahora_ - tiempoEstado_ >= DURACION_ALIMENTO) {
        transicionar(EstadoAlimentador::APAGADO);
        detenerMotor();
        LOG_DEBUG("Alimentador: OFF");
      }
      break;
  }
}

void Alimentador::setHabilitado(bool habilitado) {
  habilitado_ = habilitado;
  if (!habilitado) {
    detenerMotor();
  }
}

void Alimentador::detener() {
  detenerMotor();
  transicionar(EstadoAlimentador::APAGADO);
}

void Alimentador::reset() {
  estado_ = EstadoAlimentador::APAGADO;
  tiempoEstado_ = millis();
  habilitado_ = true;
  detenerMotor();
}

void Alimentador::transicionar(EstadoAlimentador nuevoEstado) {
  estado_ = nuevoEstado;
  tiempoEstado_ = millis();
}

void Alimentador::detenerMotor() {
  analogWrite(EN2_PIN, 0);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
}

void Alimentador::iniciarMotor(uint8_t pwm) {
  digitalWrite(IN3_PIN, HIGH);
  digitalWrite(IN4_PIN, LOW);
  analogWrite(EN2_PIN, pwm);
  vTaskDelay(pdMS_TO_TICKS(1));  // Yield
}
