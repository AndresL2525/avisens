#ifndef SENSOR_ULTRASONICO_H
#define SENSOR_ULTRASONICO_H

#include <Arduino.h>
#include "config.h"
#include "MovingAverage.h"

class SensorUltrasonico
{
public:
  SensorUltrasonico();

  void begin();
  LecturaUltrasonico leer();
  LecturaUltrasonico getUltimaLectura() const;
  bool enError() const;
  void reiniciarFallos();
  void reset();

private:
  MovingAverage<float, MOVING_AVG_SIZE> filtroDistancia_;
  LecturaUltrasonico ultimaLectura_;
  int contadorFallos_;
  unsigned long ultimoIntento_;
  unsigned long tiempoUltimoTrig_;

  LecturaUltrasonico medirDistancia();
};

#endif // SENSOR_ULTRASONICO_H