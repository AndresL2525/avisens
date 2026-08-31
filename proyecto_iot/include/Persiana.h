#ifndef PERSIANA_H
#define PERSIANA_H

#include <Arduino.h>
#include "config.h"

/**
 * @class Persiana
 * @brief Controlador de persiana/cortina (L293D Canal A).
 * 
 * Implementa FSM no bloqueante para:
 * - Apertura periódica (cada INTERVALO_PERSIANA ms)
 * - Cierre automático después de DURACION_PERSIANA ms
 * - Pausa entre ciclos (PAUSA_PERSIANA ms)
 * 
 * Ideal para renovación de aire periódica en galpón.
 */
class Persiana {
 public:
  /**
   * @brief Constructor.
   */
  Persiana();

  /**
   * @brief Inicializa pines del L293D Canal A (EN1, IN1, IN2).
   */
  void begin();

  /**
   * @brief Actualiza la FSM de la persiana (llamar cada ciclo).
   */
  void actualizar();

  /**
   * @brief Obtiene estado actual de la persiana.
   * @return EstadoPersiana
   */
  EstadoPersiana getEstado() const { return estado_; }

  /**
   * @brief Habilita/deshabilita sistema de persiana.
   * @param habilitado true para permitir ciclos automáticos
   */
  void setHabilitado(bool habilitado);

  /**
   * @brief Indica si la persiana está habilitada.
   */
  bool isHabilitado() const { return habilitado_; }

  /**
   * @brief Abre la persiana manualmente.
   */
  void abrirManual();

  /**
   * @brief Cierra la persiana manualmente.
   */
  void cerrarManual();

  /**
   * @brief Detiene el motor inmediatamente.
   */
  void detener();

  /**
   * @brief Reinicia la FSM.
   */
  void reset();

 private:
  EstadoPersiana estado_;
  unsigned long tiempoEstado_;
  unsigned long ahora_;
  bool habilitado_;

  void transicionar(EstadoPersiana nuevoEstado);
  void detenerMotor();
  void abrirMotor();
  void cerrarMotor();
};

#endif // PERSIANA_H
