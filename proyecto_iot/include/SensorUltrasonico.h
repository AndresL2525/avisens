#ifndef SENSOR_ULTRASONICO_H
#define SENSOR_ULTRASONICO_H

#include <Arduino.h>
#include "config.h"
#include "MovingAverage.h"

/**
 * @class SensorUltrasonico
 * @brief Gestor del sensor HC-SR04 para nivel de agua.
 * 
 * Implementa lectura no bloqueante del HC-SR04 con:
 * - Filtro de media móvil (MOVING_AVG_SIZE muestras)
 * - Validación de rango (0-MAX_DISTANCIA_AGUA cm)
 * - Contador de fallos con fail-safe
 * - Histéresis para bomba (ON/OFF con histeresis)
 */
class SensorUltrasonico {
 public:
  /**
   * @brief Constructor.
   */
  SensorUltrasonico();

  /**
   * @brief Inicializa pines TRIG y ECHO.
   */
  void begin();

  /**
   * @brief Realiza lectura no bloqueante con timeout.
   * @return Estructura LecturaUltrasonico con distancia y estado
   */
  LecturaUltrasonico leer();

  /**
   * @brief Obtiene la última lectura filtrada.
   * @return Estructura LecturaUltrasonico
   */
  LecturaUltrasonico getUltimaLectura() const;

  /**
   * @brief Indica si el sensor está en estado de error persistente.
   * @return true si ha superado MAX_FALLOS_SENSOR fallos consecutivos
   */
  bool enError() const;

  /**
   * @brief Reinicia el contador de fallos.
   */
  void reiniciarFallos();

  /**
   * @brief Fuerza reinicio completo del sensor.
   */
  void reset();

 private:
  MovingAverage<float, MOVING_AVG_SIZE> filtroDistancia_;
  LecturaUltrasonico ultimaLectura_;
  int contadorFallos_;
  unsigned long ultimoIntento_;
  unsigned long tiempoUltimoTrig_;

  LecturaUltrasonico medirDistancia();
};

#endif // SENSOR_ULTRASONICO_H
