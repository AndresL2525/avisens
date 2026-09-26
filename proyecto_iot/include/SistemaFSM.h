#ifndef SISTEMA_FSM_H
#define SISTEMA_FSM_H

#include <Arduino.h>
#include "config.h"

class SistemaFSM
{
public:
  SistemaFSM();

  void avanzar();
  void evaluarSensoresCriticos(bool errorCritico);
  void rearmar();
  void marcarCalibracionCompletada();

  EstadoSistema estado() const { return estado_; }
  uint32_t fallosAcumulados() const { return fallosAcumulados_; }
  bool enFailSafe() const;

private:
  EstadoSistema estado_;
  EstadoSistema ultimoEstadoAtendido_;
  uint32_t ciclosArranque_;
  uint32_t ciclosEnCalibracion_;
  bool calibracionCompletada_;
  uint32_t fallosAcumulados_;

  void aplicarFailSafe();
};

#endif
