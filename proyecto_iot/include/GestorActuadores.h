#ifndef GESTOR_ACTUADORES_H
#define GESTOR_ACTUADORES_H

#include <Arduino.h>
#include "config.h"
#include "Actuador.h"

class GestorActuadores
{
public:
  GestorActuadores();

  void begin();

  void actualizar(
      float temperatura,
      float humedad,
      int rawNH3,
      float distanciaAgua,
      EstadoSensorUltrasonico estadoSensorUltrasonico,
      bool enErrorDHT,
      bool enErrorUltrasonico);

  void failSafe();

  bool getK1() const { return k1_.getEstado(); }
  bool getK2() const { return k2_.getEstado(); }
  bool getK3() const { return k3_.getEstado(); }
  bool getK4() const { return k4_.getEstado(); }

  void forzarRele(uint8_t rele, bool estado);

  void establecerManual(uint8_t rele, bool estado);
  void establecerAutomatico(uint8_t rele);
  bool esManual(uint8_t rele) const;
  bool getEstado(uint8_t rele) const;

  static uint8_t releDesdeNombre(const String &nombre);
  static String nombreDesdeRele(uint8_t rele);

private:
  Actuador k1_;
  Actuador k2_;
  Actuador k3_;
  Actuador k4_;

  bool ultimoEstadoBomba_;
  unsigned long ultimoControl_;

  bool enFailSafe_ = false;

  bool manualK1_ = false;
  bool manualK2_ = false;
  bool manualK3_ = false;
  bool manualK4_ = false;

  void aplicarControl(
      bool activarCalefaccion,
      bool activarVentilacion,
      bool activarBomba);

  bool calcularActivacionBomba(
      float distancia,
      EstadoSensorUltrasonico estado);
};

#endif
