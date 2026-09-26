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

  if (lectura.estado == EstadoSensorUltrasonico::OK) {
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
      // Se arrastra la última distancia conocida para no publicar un hueco,
      // pero se conserva el estado de fallo: el consumidor debe saber que no es fresca.
      lectura.distancia = ultimaLectura_.distancia;
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

  // El HC-SR04 exige un TRIG en alto de 10 µs precedido de un flanco limpio
  digitalWrite(TRIG_AGUA, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_AGUA, HIGH);
  delayMicroseconds(TRIG_PULSO_US);
  digitalWrite(TRIG_AGUA, LOW);

  long duracion = pulseIn(ECHO_AGUA, HIGH, ECHO_TIMEOUT_US);

  if (duracion == 0) {
    resultado.estado = EstadoSensorUltrasonico::TIMEOUT;
    return resultado;
  }

  float distancia = duracion * VELOCIDAD_SONIDO_CM_US / 2.0f;

  if (distancia > MAX_DISTANCIA_AGUA) {
    resultado.estado = EstadoSensorUltrasonico::OUT_OF_RANGE;
    return resultado;
  }

  if (distancia < MIN_DISTANCIA_AGUA) {
    resultado.estado = EstadoSensorUltrasonico::OUT_OF_RANGE;
    return resultado;
  }

  resultado.distancia = distancia;
  resultado.estado = EstadoSensorUltrasonico::OK;
  return resultado;
}
