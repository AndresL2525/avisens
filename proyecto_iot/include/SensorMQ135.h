#ifndef SENSOR_MQ135_H
#define SENSOR_MQ135_H

#include <Arduino.h>

class SensorMQ135
{
public:
    // Inicializa el pin analógico
    void iniciar(int pinADC);

    // Lee el valor crudo del ADC (0-4095)
    int leerValorCrudo();

    // Convierte el valor crudo a voltaje (0-3.3V)
    float leerVoltaje();

    // (Opcional) Devuelve una estimación de ppm (requiere calibración)
    float leerPPM();

private:
    int _pinADC;
};

#endif