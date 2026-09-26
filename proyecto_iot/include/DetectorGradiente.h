#ifndef DETECTOR_GRADIENTE_H
#define DETECTOR_GRADIENTE_H

#include <Arduino.h>
#include "config.h"

class DetectorGradiente
{
public:
  DetectorGradiente();

  bool evaluar(float temperatura, unsigned long ahora);

private:
  float ultimaTemperatura_;
  unsigned long ultimoTimestamp_;
};

#endif
