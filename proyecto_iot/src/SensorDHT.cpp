/**
 * SensorDHT.cpp (MODIFICADO)
 * 
 * Cambios:
 * - Validar temperatura en rango [-10, 60]°C
 * - Validar humedad en rango [0, 100]%
 * - Marcar como OUT_OF_RANGE si los valores están fuera de rango físico plausible
 * - Incrementar contador de fallos si está fuera de rango
 */

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

  // ─── 1. Validación: NaN (sensor desconectado) ────────────────────
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

  // ─── 2. Validación: Rango físico plausible ────────────────────────
  // Temperatura: -10°C a 60°C (rango operativo del DHT22)
  // Humedad: 0% a 100%

  if (temperatura < -10.0f || temperatura > 60.0f) {
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

  if (humedad < 0.0f || humedad > 100.0f) {
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

  // ─── 3. Lectura válida ────────────────────────────────────────────
  lectura.temperatura = temperatura;
  lectura.humedad = humedad;
  lectura.valida = true;
  contadorFallos_ = 0;  // Reset del contador si fue exitosa
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
