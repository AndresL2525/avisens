#ifndef SENSOR_KY032_H
#define SENSOR_KY032_H

#include <Arduino.h>

class SensorKY032
{
public:
    void iniciar(int pinOUT); // <-- Recibe el pin de la señal
    bool detectaObstaculo();  // true = obstáculo detectado

private:
    int _pinOUT;
};

#endif