/*
 * ============================================================================
 *  SensorPeso.cpp
 * ----------------------------------------------------------------------------
 *  Implementación del manejo de la celda de carga (1Kg) a través del
 *  amplificador HX711.
 * ============================================================================
 */

#include "SensorPeso.h"
#include <HX711.h>
#include "config.h"

// Objeto de la librería HX711, privado a este archivo.
static HX711 balanza;

void SensorPeso::iniciar() {
    // begin() le indica a la librería en qué pines está conectado
    // el HX711: DOUT (datos) y SCK (reloj).
    balanza.begin(HX711_DOUT_PIN, HX711_SCK_PIN);

    // set_scale() aplica el factor de calibración. A partir de aquí,
    // get_units() devolverá directamente el peso en la unidad en que
    // se calibró (ver instrucciones en config.h).
    balanza.set_scale(FACTOR_CALIBRACION_CELDA);

    // tare() pone la lectura actual como "cero". Debe hacerse SIN
    // ningún peso sobre la celda de carga.
    Serial.println("[HX711] Calibrando cero (tara)... no colocar peso.");
    balanza.tare();

    Serial.println("[HX711] Sensor de peso inicializado.");
}

bool SensorPeso::estaListo() {
    return balanza.is_ready();
}

float SensorPeso::leerPeso() {
    // Si el módulo no está listo (sin señal, mal conectado), devolvemos
    // 0 para evitar que el programa se quede esperando indefinidamente.
    if (!balanza.is_ready()) {
        Serial.println("[HX711] Sensor no responde (revisar cableado).");
        return 0.0;
    }

    // get_units(10) toma 10 lecturas y devuelve el promedio.
    // Promediar reduce el ruido/vibración típico de estas celdas.
    float peso = balanza.get_units(10);

    // Las celdas de carga pueden generar pequeños valores negativos
    // por ruido eléctrico cuando no hay nada sobre ellas. Los
    // limitamos a 0 para que la lectura tenga sentido físico.
    if (peso < 0) {
        peso = 0.0;
    }

    return peso;
}

void SensorPeso::tarar() {
    balanza.tare();
    Serial.println("[HX711] Tara realizada. Báscula en cero.");
}
