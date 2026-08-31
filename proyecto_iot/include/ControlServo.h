#ifndef CONTROL_SERVO_H
#define CONTROL_SERVO_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

/**
 * @class ControlServo
 * @brief Controlador del servomotor de la puerta con FSM.
 * 
 * Implementa máquina de estados no bloqueante para:
 * - Detectar presencia (KY032)
 * - Abrir puerta (0°)
 * - Mantener abierta (NEUTRO)
 * - Cerrar puerta (180°)
 * - Estado de reposo
 * 
 * Usa millis() para timing de giros y tiempo abierto.
 */
class ControlServo {
 public:
  /**
   * @brief Constructor.
   */
  ControlServo();

  /**
   * @brief Inicializa el servo y lo coloca en posición neutra.
   */
  void begin();

  /**
   * @brief Actualiza la FSM (llamar cada ciclo de control).
   * @param hayPresencia true si se detectó presencia (KY032 = LOW)
   */
  void actualizar(bool hayPresencia);

  /**
   * @brief Obtiene estado actual de la puerta.
   * @return EstadoPuerta
   */
  EstadoPuerta getEstado() const { return estado_; }

  /**
   * @brief Fuerza cierre de puerta (emergencia).
   */
  void cerrarEmergencia();

  /**
   * @brief Reinicia la FSM.
   */
  void reset();

 private:
  Servo servo_;
  EstadoPuerta estado_;
  unsigned long tiempoEstado_;
  unsigned long ahora_;

  void transicionar(EstadoPuerta nuevoEstado);
  void escribirServoCuidado(uint8_t angulo);
};

#endif // CONTROL_SERVO_H
