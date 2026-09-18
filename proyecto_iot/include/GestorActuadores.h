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

  // ─── Control remoto / modo MANUAL (comandos desde la app web) ────

  /**
   * @brief Pone un relé en modo MANUAL y aplica el estado indicado.
   *
   * Mientras un relé esté en modo MANUAL, actualizar() NO sobrescribe
   * su estado con la lógica automática — queda "congelado" en el
   * valor que envió la app, hasta que se llame a establecerAutomatico().
   *
   * @param rele 1-4 (ver releDesdeNombre)
   * @param estado true = ON, false = OFF
   */
  void establecerManual(uint8_t rele, bool estado);

  /**
   * @brief Devuelve un relé al control de la lógica automática.
   * @param rele 1-4
   */
  void establecerAutomatico(uint8_t rele);

  /**
   * @brief Indica si un relé está actualmente en modo MANUAL.
   */
  bool esManual(uint8_t rele) const;

  /**
   * @brief Obtiene el estado actual de un relé por número (1-4).
   * @return true = ON, false = OFF (también si el número no es válido)
   */
  bool getEstado(uint8_t rele) const;

  /**
   * @brief Traduce el nombre lógico del actuador (tal como lo usa el
   * backend en actuator_commands.nombre) al número de relé interno.
   *
   * Nombres reconocidos: "calefactor" (K1), "ventilador" (K2),
   * "extractor" (K3), "bomba" (K4). No distingue mayúsculas/minúsculas.
   *
   * @return 1-4, o 0 si el nombre no se reconoce.
   */
  static uint8_t releDesdeNombre(const String& nombre);

  /**
   * @brief Nombre lógico de un relé (inverso de releDesdeNombre).
   * Útil para reportar estado con el mismo nombre que espera el backend.
   */
  static String nombreDesdeRele(uint8_t rele);

 private:
  Actuador k1_;  // Calefacción
  Actuador k2_;  // Ventilador
  Actuador k3_;  // Extractor
  Actuador k4_;  // Bomba

  bool ultimoEstadoBomba_;
  unsigned long ultimoControl_;

  // Modo MANUAL por relé: true = bajo control remoto (app), false = AUTO
  bool manualK1_ = false;
  bool manualK2_ = false;
  bool manualK3_ = false;
  bool manualK4_ = false;

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
