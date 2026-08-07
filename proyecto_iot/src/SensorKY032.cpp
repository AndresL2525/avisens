#include "SensorKY032.h"

void SensorKY032::iniciar(int pinOUT)
{
    _pinOUT = pinOUT;
    pinMode(_pinOUT, INPUT);
    Serial.println("[KY032] Inicializado en pin " + String(pinOUT));
}

bool SensorKY032::detectaObstaculo()
{
    return digitalRead(_pinOUT) == LOW; // LOW = obstáculo
}