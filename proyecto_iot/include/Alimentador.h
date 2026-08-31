#ifndef ALIMENTADOR_H
#define ALIMENTADOR_H

#include <Arduino.h>
#include "config.h"

/**
 * @class Alimentador
 * @brief Controlador del motor tornillo sinfín (L293D Canal B).
 * 
 * Implementa FSM para dispensado periódico de alimento:
 * - Intervalo ON: INTERVALO_ALIMENTO ms
 * - Duración del giro: DURACION_ALIMENTO ms (100% PWM)
 * - Ciclo: Encendido → Apagado → Espera → Repite
 * 
 * PWM a 50% para torque moderado y consumo controlado.
 */
class Alimentador {
 public:
  /**
   * @brief Constructor.
   */
  Alimentador();

  /**
   * @brief Inicializa pines del L293D Canal B (EN2, IN3, IN4).
   */
  void begin();

  /**
   * @brief Actualiza la FSM del alimentador (llamar cada ciclo).
   */
  void actualizar();

  /**
   * @brief Obtiene estado actual del motor.
   * @return EstadoAlimentador
   */
  EstadoAlimentador getEstado() const { return estado_; }

  /**
   * @brief Habilita/deshabilita el sistema de alimentación.
   * @param habilitado true para permitir ciclos, false para mantener apagado
   */
  void setHabilitado(bool habilitado);

  /**
   * @brief Indica si el alimentador está habilitado.
   */
  bool isHabilitado() const { return habilitado_; }

  /**
   * @brief Detiene inmediatamente el motor.
   */
  void detener();

  /**
   * @brief Reinicia la FSM.
   */
  void reset();

 private:
  EstadoAlimentador estado_;
  unsigned long tiempoEstado_;
  unsigned long ahora_;
  bool habilitado_;

  void transicionar(EstadoAlimentador nuevoEstado);
  void detenerMotor();
  void iniciarMotor(uint8_t pwm);
};

#endif // ALIMENTADOR_H
