#ifndef SENSOR_KY032_H
#define SENSOR_KY032_H

#include <Arduino.h>
#include "config.h"

/**
 * @class SensorKY032
 * @brief Gestor del sensor de presencia KY-032 (infrarrojo).
 * 
 * Lee el estado digital (LOW = presencia detectada) de forma
 * no bloqueante. Útil para activar la puerta servomotorizada.
 */
class SensorKY032 {
 public:
  /**
   * @brief Constructor.
   */
  SensorKY032();

  /**
   * @brief Inicializa el sensor (configurar pin como entrada).
   */
  void begin();

  /**
   * @brief Realiza lectura no bloqueante del sensor.
   * @return Estructura LecturaKY032 con estado y timestamp
   */
  LecturaKY032 leer();

  /**
   * @brief Obtiene la última lectura.
   * @return Estructura LecturaKY032
   */
  LecturaKY032 getUltimaLectura() const;

  /**
   * @brief Indica si hay presencia detectada.
   * @return true si se detectó presencia (LOW)
   */
  bool hayPresencia() const;

 private:
  LecturaKY032 ultimaLectura_;
};

#endif // SENSOR_KY032_H
