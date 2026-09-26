#ifndef SENSOR_DHT_H
#define SENSOR_DHT_H

#include <Arduino.h>
#include "DHT.h"
#include "config.h"

class SensorDHT
{
public:
  SensorDHT();

  void begin();
  LecturaDHT leer();
  LecturaDHT getUltimaLectura() const;
  bool enError() const;
  void reiniciarFallos();
  void reset();

private:
  DHT dht_;
  LecturaDHT ultimaLectura_;
  int contadorFallos_;
  unsigned long ultimoIntento_;
};

#endif // SENSOR_DHT_H