/*
 * ============================================================================
 *  SensorPeso.h
 * ----------------------------------------------------------------------------
 *  Módulo encargado ÚNICAMENTE de la celda de carga + amplificador HX711.
 *  Encapsula la calibración y el cálculo de peso para que el resto del
 *  programa solo pida "dame el peso actual" sin preocuparse del detalle.
 * ============================================================================
 */

#ifndef SENSOR_PESO_H
#define SENSOR_PESO_H

#include <Arduino.h>

class SensorPeso {
public:
    // Inicializa el HX711, aplica el factor de calibración (definido en
    // config.h) y pone la balanza en cero (tara).
    // IMPORTANTE: al ejecutar tare(), no debe haber ningún peso sobre la
    // celda de carga, o el cero quedará mal calculado.
    void iniciar();

    // Devuelve el peso actual en las unidades en que se calibró el
    // FACTOR_CALIBRACION_CELDA (normalmente gramos o kilogramos,
    // según cómo se haya hecho la calibración).
    // Internamente promedia varias lecturas para reducir el ruido
    // típico de las celdas de carga.
    float leerPeso();

    // Vuelve a poner la báscula en cero. Útil si se quiere "tarar"
    // (descontar el peso de un recipiente) durante la ejecución.
    void tarar();

    // Indica si el HX711 está respondiendo correctamente.
    // Si devuelve false, normalmente hay un problema de cableado
    // (DOUT/SCK mal conectados o sin alimentación).
    bool estaListo();
};

#endif // SENSOR_PESO_H
