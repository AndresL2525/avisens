#include "SensorMQ135.h"

SensorMQ135::SensorMQ135()
    : ultimoIntento_(0) {
  ultimaLectura_ = {0, 0.0f, false, 0};
}

void SensorMQ135::begin() {
  pinMode(MQ135_PIN, INPUT);
  LOG_DEBUG("SensorMQ135 inicializado");
}

LecturaMQ135 SensorMQ135::leer() {
  unsigned long ahora = millis();

  int rawValue = analogRead(MQ135_PIN);
  float voltaje = rawValue * (3.3f / 4095.0f);

  // Aplicar filtro de media móvil
  int rawFiltrado = filtroRaw_.add(rawValue);

  LecturaMQ135 lectura;
  lectura.rawValue = rawFiltrado;
  lectura.voltaje = rawFiltrado * (3.3f / 4095.0f);
  lectura.valida = true;
  lectura.timestamp = ahora;

  ultimaLectura_ = lectura;
  return lectura;
}

LecturaMQ135 SensorMQ135::getUltimaLectura() const {
  return ultimaLectura_;
}

String SensorMQ135::getNivelGas() const {
  return clasificarNivel(ultimaLectura_.rawValue);
}

void SensorMQ135::reset() {
  filtroRaw_.reset();
  ultimaLectura_ = {0, 0.0f, false, 0};
}

String SensorMQ135::clasificarNivel(int rawValue) const {
  if (rawValue < NH3_MODERADO) {
    return "NORMAL";
  } else if (rawValue < NH3_ALTO) {
    return "MODERADO";
  } else {
    return "ALTO";
  }
}
