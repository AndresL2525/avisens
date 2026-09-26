#include "DetectorGradiente.h"

DetectorGradiente::DetectorGradiente()
    : ultimaTemperatura_(0.0f),
      ultimoTimestamp_(0)
{
}

bool DetectorGradiente::evaluar(float temperatura, unsigned long ahora)
{
  if (ultimoTimestamp_ == 0)
  {
    ultimaTemperatura_ = temperatura;
    ultimoTimestamp_ = ahora;
    return false;
  }

  unsigned long deltaT = ahora - ultimoTimestamp_;
  float deltaTemp = fabsf(temperatura - ultimaTemperatura_);

  bool hayGradiente = (deltaT <= VENTANA_GRADIENTE_MS && deltaTemp > UMBRAL_GRADIENTE_TERMICO);

  ultimaTemperatura_ = temperatura;
  ultimoTimestamp_ = ahora;

  if (hayGradiente)
  {
    LOG_WARN("Gradiente térmico abrupto: ΔT=" + String(deltaTemp) + "°C en " + String(deltaT) + "ms");
  }
  return hayGradiente;
}
