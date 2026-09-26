#ifndef ACTUADOR_H
#define ACTUADOR_H

#include <Arduino.h>
#include "config.h"

class Actuador
{
public:
  explicit Actuador(uint8_t pin);

  virtual void begin();
  virtual void activar();
  virtual void desactivar();
  virtual void setEstado(bool estado);
  virtual bool getEstado() const;
  virtual void conmutar();

protected:
  uint8_t pin_;
  bool estado_;
};

#endif
