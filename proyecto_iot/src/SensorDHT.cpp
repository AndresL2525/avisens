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

  float humedad = dht_.readHumidity();
  float temperatura = dht_.readTemperature();

  LecturaDHT lectura;
  lectura.temperatura = 0.0f;
  lectura.humedad = 0.0f;
  lectura.valida = false;
  lectura.timestamp = ahora;

  if (isnan(humedad) || isnan(temperatura)) {
    contadorFallos_++;
    lectura.valida = false;

    Serial.print("⚠ DHT22 fallo (NaN) #");
    Serial.println(contadorFallos_);

    if (contadorFallos_ >= MAX_FALLOS_SENSOR) {
      LOG_ERROR("DHT22 fallo persistente (desconectado) — Entrando en ERROR");
    }
    return lectura;
  }

  if (temperatura < TEMP_MIN_VALIDA || temperatura > TEMP_MAX_VALIDA) {
    contadorFallos_++;
    lectura.valida = false;

    Serial.print("⚠ DHT22 temperatura fuera de rango: ");
    Serial.print(temperatura);
    Serial.println("°C");

    if (contadorFallos_ >= MAX_FALLOS_SENSOR) {
      LOG_ERROR("DHT22 fallo persistente (rango) — Entrando en ERROR");
    }
    return lectura;
  }

  if (humedad < HUM_MIN_VALIDA || humedad > HUM_MAX_VALIDA) {
    contadorFallos_++;
    lectura.valida = false;

    Serial.print("⚠ DHT22 humedad fuera de rango: ");
    Serial.print(humedad);
    Serial.println("%");

    if (contadorFallos_ >= MAX_FALLOS_SENSOR) {
      LOG_ERROR("DHT22 fallo persistente (rango) — Entrando en ERROR");
    }
    return lectura;
  }

  lectura.temperatura = temperatura;
  lectura.humedad = humedad;
  lectura.valida = true;
  contadorFallos_ = 0;
  ultimaLectura_ = lectura;

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
  LOG_DEBUG("SensorDHT reseteado");
}
