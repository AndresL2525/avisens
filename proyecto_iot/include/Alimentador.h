#ifndef ALIMENTADOR_H
#define ALIMENTADOR_H

#include <Arduino.h>
#include "config.h"

class Alimentador {
 public:
  Alimentador();

  void begin();
  void actualizar();
  EstadoAlimentador getEstado() const { return estado_; }
  void setHabilitado(bool habilitado);
  bool isHabilitado() const { return habilitado_; }
  void detener();
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

#endif
