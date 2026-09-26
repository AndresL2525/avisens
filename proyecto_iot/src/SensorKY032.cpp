#include "SensorKY032.h"

SensorKY032::SensorKY032()
    : nivelCrudo_(HIGH),
      inicioNivelCrudo_(0),
      presenciaEstable_(false),
      inicioPresencia_(0),
      trabado_(false) {
  ultimaLectura_ = {false, 0};
}

void SensorKY032::begin() {
  pinMode(KY032_PIN, INPUT_PULLUP);
  reset();
  LOG_DEBUG("SensorKY032 inicializado");
}

LecturaKY032 SensorKY032::leer() {
  unsigned long ahora = millis();

  int nivel = digitalRead(KY032_PIN);
  if (nivel != nivelCrudo_) {
    nivelCrudo_ = nivel;
    inicioNivelCrudo_ = ahora;
  } else if (ahora - inicioNivelCrudo_ >= KY032_DEBOUNCE_MS) {
    bool presencia = (nivel == LOW);  // Salida en colector abierto: LOW es detección
    if (presencia != presenciaEstable_) {
      presenciaEstable_ = presencia;
      inicioPresencia_ = inicioNivelCrudo_;
    }
  }

  trabado_ = presenciaEstable_ && (ahora - inicioPresencia_ >= KY032_TRABADO_MS);

  ultimaLectura_ = {presenciaEstable_, ahora};
  return ultimaLectura_;
}

LecturaKY032 SensorKY032::getUltimaLectura() const {
  return ultimaLectura_;
}

bool SensorKY032::hayPresencia() const {
  return ultimaLectura_.presencia;
}

bool SensorKY032::enTrabado() const {
  return trabado_;
}

void SensorKY032::reset() {
  unsigned long ahora = millis();
  nivelCrudo_ = digitalRead(KY032_PIN);
  inicioNivelCrudo_ = ahora;
  presenciaEstable_ = false;
  inicioPresencia_ = ahora;
  trabado_ = false;
  ultimaLectura_ = {false, ahora};
}
