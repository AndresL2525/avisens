#ifndef SENSOR_MQ135_H
#define SENSOR_MQ135_H

#include <Arduino.h>
#include "config.h"
#include "MovingAverage.h"

/**
 * @class SensorMQ135
 * @brief Gestor del sensor MQ135 (NH3/CO2) con filtro de media móvil.
 * 
 * Realiza lectura analógica del MQ135 y aplica un filtro de media móvil
 * con buffer circular de MOVING_AVG_SIZE muestras para suavizar ruido.
 */
class SensorMQ135 {
 public:
  /**
   * @brief Constructor. Inicializa filtro de media móvil.
   */
  SensorMQ135();

  /**
   * @brief Inicializa el sensor (configurar pin ADC).
   */
  void begin();

  /**
   * @brief Realiza lectura no bloqueante y aplica filtro.
   * @return Estructura LecturaMQ135 con raw value, voltaje y validez
   */
  LecturaMQ135 leer();

  /**
   * @brief Obtiene la última lectura filtrada.
   * @return Estructura LecturaMQ135
   */
  LecturaMQ135 getUltimaLectura() const;

  /**
   * @brief Obtiene clasificación de nivel de gas.
   * @return "NORMAL", "MODERADO" o "ALTO"
   */
  String getNivelGas() const;

  /**
   * @brief Reinicia el filtro de media móvil.
   */
  void reset();

 private:
  MovingAverage<int, MOVING_AVG_SIZE> filtroRaw_;
  LecturaMQ135 ultimaLectura_;
  unsigned long ultimoIntento_;

  String clasificarNivel(int rawValue) const;
};

#endif // SENSOR_MQ135_H
