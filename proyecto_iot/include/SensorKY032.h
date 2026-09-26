#ifndef SENSOR_KY032_H
#define SENSOR_KY032_H

#include <Arduino.h>
#include "config.h"

class SensorKY032
{
public:
  SensorKY032();

  void begin();
  LecturaKY032 leer();
  LecturaKY032 getUltimaLectura() const;
  bool hayPresencia() const;
  bool enTrabado() const;
  void reset();

private:
  LecturaKY032 ultimaLectura_;
  int nivelCrudo_;
  unsigned long inicioNivelCrudo_;
  bool presenciaEstable_;
  unsigned long inicioPresencia_;
  bool trabado_;
};

#endif // SENSOR_KY032_H
