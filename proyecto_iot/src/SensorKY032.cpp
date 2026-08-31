#include "SensorKY032.h"

SensorKY032::SensorKY032() {
  ultimaLectura_ = {false, 0};
}

void SensorKY032::begin() {
  pinMode(KY032_PIN, INPUT_PULLUP);
  LOG_DEBUG("SensorKY032 inicializado");
}

LecturaKY032 SensorKY032::leer() {
  unsigned long ahora = millis();

  // Lógica: LOW = presencia detectada
  int estado = digitalRead(KY032_PIN);
  bool presencia = (estado == LOW);

  ultimaLectura_ = {presencia, ahora};
  return ultimaLectura_;
}

LecturaKY032 SensorKY032::getUltimaLectura() const {
  return ultimaLectura_;
}

bool SensorKY032::hayPresencia() const {
  return ultimaLectura_.presencia;
}
