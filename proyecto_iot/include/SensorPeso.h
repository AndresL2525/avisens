#ifndef SENSOR_PESO_H
#define SENSOR_PESO_H

#include <Arduino.h>
#include "config.h"

/**
 * @struct LecturaPeso
 * @brief Estructura de datos para lectura de peso (HX711)
 */
struct LecturaPeso {
  float peso;                // Gramos
  float voltaje;             // Voltaje ADC leído
  bool valida;               // Lectura válida
  unsigned long timestamp;
};

/**
 * @class SensorPeso
 * @brief Gestor del sensor de peso HX711 para monitoreo de consumo.
 * 
 * Características:
 * - Comunicación SPI con HX711
 * - Calibración manual (tara) requerida en startup
 * - Factor de escala (gram_per_count) configurable
 * - Detección de sobrecarga
 * - Contador de fallos con fallback
 * 
 * Uso típico:
 *   sensorPeso.begin();
 *   sensorPeso.tara();  // En calibración
 *   float peso = sensorPeso.leer().peso;
 */
class SensorPeso {
 public:
  /**
   * @brief Constructor. Inicializa pines del HX711.
   */
  SensorPeso();

  /**
   * @brief Inicializa comunicación con HX711 (GPIO).
   */
  void begin();

  /**
   * @brief Realiza lectura no bloqueante del HX711.
   * @return Estructura LecturaPeso con peso y validez
   */
  LecturaPeso leer();

  /**
   * @brief Obtiene la última lectura válida.
   * @return Estructura LecturaPeso
   */
  LecturaPeso getUltimaLectura() const;

  /**
   * @brief Tara el sensor (zero offset).
   * 
   * Lee 10 valores y promedia para establecer el offset.
   * Debe ejecutarse con celda descargada.
   */
  void tara();

  /**
   * @brief Establece factor de escala (g/count).
   * 
   * @param factor Gramos por unidad ADC
   * Ejemplo: 0.453592 para una celda de 20kg
   */
  void setFactor(float factor);

  /**
   * @brief Obtiene factor actual.
   */
  float getFactor() const { return factorEscala_; }

  /**
   * @brief Indica si sensor está en error.
   */
  bool enError() const;

  /**
   * @brief Reinicia el sensor y offset.
   */
  void reset();

 private:
  // Pines HX711
  uint8_t pinDT_;   // DATA
  uint8_t pinSCK_;  // CLOCK

  // Calibración
  float offsetCero_;      // Valor ADC cuando está tarado
  float factorEscala_;    // Gramos por unidad ADC
  bool tarado_;           // true si se ejecutó tara()

  // Estado
  LecturaPeso ultimaLectura_;
  int contadorFallos_;
  unsigned long ultimoIntento_;

  // Métodos privados
  long leerADC();
  bool verificarConexion();
  long promediarLecturas(uint16_t muestras);
};

#endif // SENSOR_PESO_H
