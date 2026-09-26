#ifndef SENSOR_MQ135_H
#define SENSOR_MQ135_H

#include <Arduino.h>
#include "config.h"
#include "MovingAverage.h"

class SensorMQ135
{
public:
  SensorMQ135();

  void begin();
  LecturaMQ135 leer();
  LecturaMQ135 getUltimaLectura() const;
  String getNivelGas() const;
  void reset();

private:
  MovingAverage<int, MOVING_AVG_SIZE> filtroRaw_;
  LecturaMQ135 ultimaLectura_;
  unsigned long ultimoIntento_;

  String clasificarNivel(int rawValue) const;
};

#endif // SENSOR_MQ135_H