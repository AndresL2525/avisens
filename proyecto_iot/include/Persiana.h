#ifndef PERSIANA_H
#define PERSIANA_H

#include <Arduino.h>
#include "config.h"

class Persiana
{
public:
  Persiana();

  void begin();
  void actualizar();
  EstadoPersiana getEstado() const { return estado_; }
  void setHabilitado(bool habilitado);
  bool isHabilitado() const { return habilitado_; }
  void abrirManual();
  void cerrarManual();
  void detener();
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

#endif
