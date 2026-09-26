#include "Actuador.h"

Actuador::Actuador(uint8_t pin)
    : pin_(pin),
      estado_(false) {
}

void Actuador::begin() {
  pinMode(pin_, OUTPUT);
  desactivar();
}

void Actuador::activar() {
  digitalWrite(pin_, LOW);  // Módulo de relé activo en LOW
  estado_ = true;
}

void Actuador::desactivar() {
  digitalWrite(pin_, HIGH);  // HIGH es el estado seguro de la línea
  estado_ = false;
}

void Actuador::setEstado(bool estado) {
  if (estado) {
    activar();
  } else {
    desactivar();
  }
}

bool Actuador::getEstado() const {
  return estado_;
}

void Actuador::conmutar() {
  setEstado(!estado_);
}
