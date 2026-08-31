#include "SensorUltrasonico.h"

SensorUltrasonico::SensorUltrasonico()
    : contadorFallos_(0),
      ultimoIntento_(0),
      tiempoUltimoTrig_(0) {
  ultimaLectura_ = {-1.0f, EstadoSensorUltrasonico::ERROR, 0};
}

void SensorUltrasonico::begin() {
  pinMode(TRIG_AGUA, OUTPUT);
  pinMode(ECHO_AGUA, INPUT);
  digitalWrite(TRIG_AGUA, LOW);
  LOG_DEBUG("SensorUltrasonico inicializado");
}

LecturaUltrasonico SensorUltrasonico::leer() {
  unsigned long ahora = millis();

  LecturaUltrasonico lectura = medirDistancia();
  lectura.timestamp = ahora;

  // Procesamiento de resultado
  if (lectura.estado == EstadoSensorUltrasonico::OK) {
    // Aplicar filtro de media móvil
    float distanciaFiltrada = filtroDistancia_.add(lectura.distancia);
    lectura.distancia = distanciaFiltrada;
    contadorFallos_ = 0;
    ultimaLectura_ = lectura;
  } else {
    contadorFallos_++;
    Serial.print("⚠ HC-SR04 fallo #");
    Serial.print(contadorFallos_);
    Serial.print(" — Estado: ");
    Serial.println((int)lectura.estado);

    if (contadorFallos_ >= MAX_FALLOS_SENSOR) {
      LOG_ERROR("HC-SR04 fallo persistente — Entrando en ERROR");
      lectura.estado = EstadoSensorUltrasonico::ERROR;
    } else {
      // Mantener última lectura válida si está disponible
      lectura = ultimaLectura_;
    }
  }

  return lectura;
}

LecturaUltrasonico SensorUltrasonico::getUltimaLectura() const {
  return ultimaLectura_;
}

bool SensorUltrasonico::enError() const {
  return contadorFallos_ >= MAX_FALLOS_SENSOR;
}

void SensorUltrasonico::reiniciarFallos() {
  contadorFallos_ = 0;
}

void SensorUltrasonico::reset() {
  contadorFallos_ = 0;
  ultimoIntento_ = 0;
  tiempoUltimoTrig_ = 0;
  filtroDistancia_.reset();
  ultimaLectura_ = {-1.0f, EstadoSensorUltrasonico::ERROR, 0};
}

LecturaUltrasonico SensorUltrasonico::medirDistancia() {
  unsigned long ahora = millis();
  LecturaUltrasonico resultado;
  resultado.estado = EstadoSensorUltrasonico::ERROR;
  resultado.distancia = -1.0f;
  resultado.timestamp = ahora;

  // Generar pulso TRIG
  digitalWrite(TRIG_AGUA, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_AGUA, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_AGUA, LOW);

  // Esperar pulso ECHO con timeout de 30 ms (aprox. 400 cm)
  long duracion = pulseIn(ECHO_AGUA, HIGH, 30000);

  if (duracion == 0) {
    resultado.estado = EstadoSensorUltrasonico::TIMEOUT;
    return resultado;
  }

  // Cálculo: velocidad sonido = 343 m/s → 0.0343 cm/µs
  float distancia = duracion * 0.0343f / 2.0f;

  if (distancia > MAX_DISTANCIA_AGUA) {
    resultado.estado = EstadoSensorUltrasonico::OUT_OF_RANGE;
    return resultado;
  }

  if (distancia < 0.5f) {
    resultado.estado = EstadoSensorUltrasonico::OUT_OF_RANGE;
    return resultado;
  }

  resultado.distancia = distancia;
  resultado.estado = EstadoSensorUltrasonico::OK;
  return resultado;
}
