#include "Persiana.h"

Persiana::Persiana()
    : estado_(EstadoPersiana::QUIETA),
      tiempoEstado_(0),
      ahora_(0),
      habilitado_(true) {
}

void Persiana::begin() {
  pinMode(EN1_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);

  detenerMotor();  // Dejar en estado seguro
  LOG_DEBUG("Persiana inicializada");
}

void Persiana::actualizar() {
  ahora_ = millis();

  if (!habilitado_) {
    // Si está deshabilitada, forzar estado quieto
    if (estado_ != EstadoPersiana::QUIETA) {
      detenerMotor();
      transicionar(EstadoPersiana::QUIETA);
    }
    return;
  }

  switch (estado_) {
    case EstadoPersiana::QUIETA:
      if (ahora_ - tiempoEstado_ >= INTERVALO_PERSIANA) {
        transicionar(EstadoPersiana::ABRIENDO);
        abrirMotor();
        LOG_DEBUG("Persiana: ABRIENDO...");
      }
      break;

    case EstadoPersiana::ABRIENDO:
      if (ahora_ - tiempoEstado_ >= DURACION_PERSIANA) {
        transicionar(EstadoPersiana::PAUSA);
        detenerMotor();
        LOG_DEBUG("Persiana: PAUSA");
      }
      break;

    case EstadoPersiana::PAUSA:
      if (ahora_ - tiempoEstado_ >= PAUSA_PERSIANA) {
        transicionar(EstadoPersiana::CERRANDO);
        cerrarMotor();
        LOG_DEBUG("Persiana: CERRANDO...");
      }
      break;

    case EstadoPersiana::CERRANDO:
      if (ahora_ - tiempoEstado_ >= DURACION_PERSIANA) {
        transicionar(EstadoPersiana::QUIETA);
        detenerMotor();
        LOG_DEBUG("Persiana: QUIETA — esperando 5 min");
      }
      break;
  }
}

void Persiana::setHabilitado(bool habilitado) {
  habilitado_ = habilitado;
  if (!habilitado) {
    detenerMotor();
  }
}

void Persiana::abrirManual() {
  if (!habilitado_) return;
  transicionar(EstadoPersiana::ABRIENDO);
  abrirMotor();
}

void Persiana::cerrarManual() {
  if (!habilitado_) return;
  transicionar(EstadoPersiana::CERRANDO);
  cerrarMotor();
}

void Persiana::detener() {
  detenerMotor();
  transicionar(EstadoPersiana::QUIETA);
}

void Persiana::reset() {
  estado_ = EstadoPersiana::QUIETA;
  tiempoEstado_ = millis();
  habilitado_ = true;
  detenerMotor();
}

void Persiana::transicionar(EstadoPersiana nuevoEstado) {
  estado_ = nuevoEstado;
  tiempoEstado_ = millis();
}

void Persiana::detenerMotor() {
  digitalWrite(EN1_PIN, LOW);
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  vTaskDelay(pdMS_TO_TICKS(1));
}

void Persiana::abrirMotor() {
  digitalWrite(EN1_PIN, HIGH);
  digitalWrite(IN1_PIN, HIGH);  // Sentido: abrir
  digitalWrite(IN2_PIN, LOW);
  vTaskDelay(pdMS_TO_TICKS(1));
}

void Persiana::cerrarMotor() {
  digitalWrite(EN1_PIN, HIGH);
  digitalWrite(IN1_PIN, LOW);   // Sentido: cerrar
  digitalWrite(IN2_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(1));
}
