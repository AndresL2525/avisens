#include "SensorDHT.h"
#include <DHTesp.h>
#include "config.h"

static DHTesp dht;

void SensorDHT::iniciar()
{
    dht.setup(DHT_PIN, DHTesp::DHT11);
    delay(1500);
    Serial.println("[DHT11] Sensor inicializado con DHTesp.");
}

LecturaDHT SensorDHT::leer()
{
    LecturaDHT lectura;
    lectura.valida = false;

    for (int intento = 0; intento < 3; intento++)
    {
        float temp = dht.getTemperature();
        float hum = dht.getHumidity();

        if (!isnan(temp) && !isnan(hum))
        {
            lectura.temperatura = temp;
            lectura.humedad = hum;
            lectura.valida = true;
            break;
        }
        delay(200);
    }

    if (!lectura.valida)
    {
        Serial.println("[DHT11] Error de lectura (DHTesp)");
    }

    return lectura;
}