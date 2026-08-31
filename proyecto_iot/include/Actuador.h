#ifndef ACTUADOR_H
#define ACTUADOR_H

#include <Arduino.h>
#include "config.h"

/**
 * @class Actuador
 * @brief Clase base para actuadores digitales (relés).
 * 
 * Define interfaz común para control de dispositivos de dos estados
 * (ON/OFF). Lógica invertida: LOW = ON (relé activo), HIGH = OFF.
 */
class Actuador {
 public:
  /**
   * @brief Constructor.
   * @param pin GPIO del actuador
   */
  explicit Actuador(uint8_t pin);

  /**
   * @brief Inicializa el pin como salida (desactivado).
   */
  virtual void begin();

  /**
   * @brief Activa el actuador (LOW en relé).
   */
  virtual void activar();

  /**
   * @brief Desactiva el actuador (HIGH en relé).
   */
  virtual void desactivar();

  /**
   * @brief Establece estado del actuador.
   * @param estado true para activar, false para desactivar
   */
  virtual void setEstado(bool estado);

  /**
   * @brief Obtiene estado actual del actuador.
   * @return true si está activo, false si está inactivo
   */
  virtual bool getEstado() const;

  /**
   * @brief Conmuta el estado del actuador.
   */
  virtual void conmutar();

 protected:
  uint8_t pin_;
  bool estado_;  // true = activo, false = inactivo
};

#endif // ACTUADOR_H
