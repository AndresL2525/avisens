#ifndef SENSOR_DHT_H
#define SENSOR_DHT_H

#include <Arduino.h>

struct LecturaDHT
{
    float temperatura;
    float humedad;
    bool valida;
};

class SensorDHT
{
public:
    void iniciar();
    LecturaDHT leer();
};

#endif