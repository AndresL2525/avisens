#ifndef GESTOR_ACTUADORES_H
#define GESTOR_ACTUADORES_H

#include <Arduino.h>
#include "config.h"
#include "Actuador.h"

/**
 * @class GestorActuadores
 * @brief Gestor centralizado de los 4 relés del sistema.
 * 
 * Controla:
 * - K1: Calefacción/Bombillos infrarrojo
 * - K2: Ventilador
 * - K3: Extractor
 * - K4: Bomba de agua
 * 
 * Implementa lógica de control con umbrales de temperatura,
 * humedad y gases. Maneja fail-safe en caso de error de sensores.
 */
class GestorActuadores {
 public:
  /**
   * @brief Constructor. Inicializa los 4 actuadores.
   */
  GestorActuadores();

  /**
   * @brief Inicializa todos los pines como salidas (desactivados).
   */
  void begin();

  /**
   * @brief Aplica lógica de control basada en lecturas de sensores.
   * 
   * @param temperatura Temperatura en °C (DHT22)
   * @param humedad Humedad relativa % (DHT22)
   * @param rawNH3 Valor raw del MQ135
   * @param distanciaAgua Distancia en cm (HC-SR04)
   * @param estadoSensorUltrasonico Estado del sensor ultrasónico
   * @param enErrorDHT true si DHT22 está en error
   * @param enErrorUltrasonico true si HC-SR04 está en error
   */
  void actualizar(
    float temperatura,
    float humedad,
    int rawNH3,
    float distanciaAgua,
    EstadoSensorUltrasonico estadoSensorUltrasonico,
    bool enErrorDHT,
    bool enErrorUltrasonico
  );

  /**
   * @brief Entra en modo Fail-Safe: desactiva todo salvo bomba (si error nivel agua).
   */
  void failSafe();

  /**
   * @brief Obtiene estado de K1 (Calefacción).
   */
  bool getK1() const { return k1_.getEstado(); }

  /**
   * @brief Obtiene estado de K2 (Ventilador).
   */
  bool getK2() const { return k2_.getEstado(); }

  /**
   * @brief Obtiene estado de K3 (Extractor).
   */
  bool getK3() const { return k3_.getEstado(); }

  /**
   * @brief Obtiene estado de K4 (Bomba).
   */
  bool getK4() const { return k4_.getEstado(); }

  /**
   * @brief Fuerza estado de un relé (principalmente para debug/emergencia).
   * @param rele 1-4
   * @param estado true para ON, false para OFF
   */
  void forzarRele(uint8_t rele, bool estado);

 private:
  Actuador k1_;  // Calefacción
  Actuador k2_;  // Ventilador
  Actuador k3_;  // Extractor
  Actuador k4_;  // Bomba

  bool ultimoEstadoBomba_;
  unsigned long ultimoControl_;

  // Métodos de lógica interna
  void aplicarControl(
    bool activarCalefaccion,
    bool activarVentilacion,
    bool activarBomba
  );

  bool calcularActivacionBomba(
    float distancia,
    EstadoSensorUltrasonico estado
  );
};

#endif // GESTOR_ACTUADORES_H
