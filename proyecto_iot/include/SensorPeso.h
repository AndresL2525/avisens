#ifndef SENSOR_PESO_H
#define SENSOR_PESO_H

#include <Arduino.h>
#include "config.h"

struct LecturaPeso
{
  float peso;
  float voltaje;
  bool valida;
  unsigned long timestamp;
};

class SensorPeso
{
public:
  SensorPeso();

  void begin();
  LecturaPeso leer();
  LecturaPeso getUltimaLectura() const;
  void tara();
  void setFactor(float factor);
  float getFactor() const { return factorEscala_; }
  bool enError() const;
  void reset();

private:
  uint8_t pinDT_;
  uint8_t pinSCK_;

  float offsetCero_;
  float factorEscala_;
  bool tarado_;
  bool avisoTaraEmitido_;

  LecturaPeso ultimaLectura_;
  int contadorFallos_;
  unsigned long ultimoIntento_;

  bool leerADC(long &valor);
  bool verificarConexion();
  bool promediarLecturas(uint16_t muestras, long &promedio);
};

#endif
