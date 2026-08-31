#include "SensorPeso.h"

// Pines HX711 (configurables en config.h)
#define HX711_DT   15  // GPIO 15 (DATA)
#define HX711_SCK  16  // ✅ GPIO 16 (CLOCK) — SIN CONFLICTO

SensorPeso::SensorPeso()
    : pinDT_(HX711_DT),
      pinSCK_(HX711_SCK),
      offsetCero_(0),
      factorEscala_(1.0f),
      tarado_(false),
      contadorFallos_(0),
      ultimoIntento_(0) {
  ultimaLectura_ = {0.0f, 0.0f, false, 0};
}

void SensorPeso::begin() {
  pinMode(pinDT_, INPUT);
  pinMode(pinSCK_, OUTPUT);
  digitalWrite(pinSCK_, LOW);

  delay(100);

  // Verificar conexión con HX711
  if (!verificarConexion()) {
    LOG_ERROR("HX711 no detectado — Revisar conexión DT/SCK");
    contadorFallos_++;
  } else {
    LOG_DEBUG("SensorPeso HX711 inicializado");
  }
}

LecturaPeso SensorPeso::leer() {
  unsigned long ahora = millis();
  LecturaPeso lectura;
  lectura.timestamp = ahora;

  if (!tarado_) {
    LOG_WARN("SensorPeso: Ejecuta tara() antes de leer");
    lectura.valida = false;
    return lectura;
  }

  long rawADC = leerADC();

  if (rawADC == 0) {
    contadorFallos_++;
    lectura.valida = false;
    Serial.print("⚠ HX711 fallo #");
    Serial.println(contadorFallos_);
    return lectura;
  }

  // Convertir ADC a gramos usando factor de escala
  float peso = (rawADC - offsetCero_) * factorEscala_;

  // Detección de sobrecarga (típicamente >2^23 en HX711)
  if (rawADC > 8388607) {
    LOG_WARN("HX711 sobrecargado");
    lectura.valida = false;
    contadorFallos_++;
    return lectura;
  }

  lectura.peso = (peso < 0) ? 0 : peso;  // No permitir negativos
  lectura.voltaje = rawADC * (3.3f / 16777216.0f);  // 24-bit
  lectura.valida = true;

  contadorFallos_ = 0;
  ultimaLectura_ = lectura;
  return lectura;
}

LecturaPeso SensorPeso::getUltimaLectura() const {
  return ultimaLectura_;
}

void SensorPeso::tara() {
  LOG_DEBUG("HX711: Iniciando tara (10 muestras)...");
  delay(500);

  offsetCero_ = promediarLecturas(10);

  LOG_DEBUG("Tara completada. Offset = ");
  Serial.println(offsetCero_);

  tarado_ = true;
}

void SensorPeso::setFactor(float factor) {
  factorEscala_ = factor;
  Serial.print("Factor de escala: ");
  Serial.println(factor);
}

bool SensorPeso::enError() const {
  return contadorFallos_ >= MAX_FALLOS_SENSOR;
}

void SensorPeso::reset() {
  offsetCero_ = 0;
  factorEscala_ = 1.0f;
  tarado_ = false;
  contadorFallos_ = 0;
  ultimaLectura_ = {0.0f, 0.0f, false, 0};
  LOG_DEBUG("SensorPeso reiniciado");
}

// ─── Métodos privados ───────────────────────────────────

/**
 * @brief Lee 24 bits del HX711 (protocolo SPI simplificado).
 * 
 * Formato:
 *   DT pasa a LOW cuando datos disponibles
 *   Lee 24 bits en flancos de SCK
 *   25º pulso = selecciona próxima ganancia
 */
long SensorPeso::leerADC() {
  // Esperar a que datos estén listos (DT = LOW)
  unsigned long timeout = millis() + 1000;  // 1 segundo timeout
  while (digitalRead(pinDT_) == HIGH) {
    if (millis() > timeout) {
      LOG_WARN("HX711 timeout esperando datos");
      return 0;
    }
    delayMicroseconds(1);
  }

  long resultado = 0;

  // Leer 24 bits (MSB primero)
  for (int i = 0; i < 24; i++) {
    digitalWrite(pinSCK_, HIGH);
    delayMicroseconds(1);

    resultado <<= 1;
    if (digitalRead(pinDT_) == LOW) {
      resultado |= 1;
    }

    digitalWrite(pinSCK_, LOW);
    delayMicroseconds(1);
  }

  // 25º pulso (selecciona ganancia 128 para próxima lectura)
  digitalWrite(pinSCK_, HIGH);
  delayMicroseconds(1);
  digitalWrite(pinSCK_, LOW);
  delayMicroseconds(1);

  return resultado;
}

/**
 * @brief Verifica si HX711 está conectado.
 */
bool SensorPeso::verificarConexion() {
  // Intentar leer 3 veces
  for (int i = 0; i < 3; i++) {
    long valor = leerADC();
    if (valor > 0) {
      return true;
    }
    delay(100);
  }
  return false;
}

/**
 * @brief Promedia N lecturas del ADC.
 */
long SensorPeso::promediarLecturas(uint16_t muestras) {
  long suma = 0;
  for (uint16_t i = 0; i < muestras; i++) {
    suma += leerADC();
    delay(100);  // Espacio entre lecturas
  }
  return suma / muestras;
}
