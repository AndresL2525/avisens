#include "SensorDHT.h"

SensorDHT::SensorDHT()
    : dht_(DHTPIN, DHTTYPE),
      contadorFallos_(0),
      ultimoIntento_(0) {
  ultimaLectura_ = {0.0f, 0.0f, false, 0};
}

void SensorDHT::begin() {
  dht_.begin();
  LOG_DEBUG("SensorDHT inicializado");
}

LecturaDHT SensorDHT::leer() {
  unsigned long ahora = millis();

  // Lectura no bloqueante: DHT22 requiere ~2.25ms
  float humedad = dht_.readHumidity();
  float temperatura = dht_.readTemperature();

  LecturaDHT lectura;
  lectura.timestamp = ahora;

  // Validación
  if (isnan(humedad) || isnan(temperatura)) {
    contadorFallos_++;
    lectura.valida = false;

    Serial.print("⚠ DHT22 fallo #");
    Serial.println(contadorFallos_);

    if (contadorFallos_ >= MAX_FALLOS_SENSOR) {
      LOG_ERROR("DHT22 fallo persistente — Entrando en ERROR");
    }
  } else {
    lectura.temperatura = temperatura;
    lectura.humedad = humedad;
    lectura.valida = true;
    contadorFallos_ = 0;
    ultimaLectura_ = lectura;
  }

  return lectura;
}

LecturaDHT SensorDHT::getUltimaLectura() const {
  return ultimaLectura_;
}

bool SensorDHT::enError() const {
  return contadorFallos_ >= MAX_FALLOS_SENSOR;
}

void SensorDHT::reiniciarFallos() {
  contadorFallos_ = 0;
}

void SensorDHT::reset() {
  contadorFallos_ = 0;
  ultimaLectura_ = {0.0f, 0.0f, false, 0};
  dht_.begin();
}
