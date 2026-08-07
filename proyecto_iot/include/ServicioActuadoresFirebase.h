/*
 * ============================================================================
 *  ServicioActuadoresFirebase.h
 * ----------------------------------------------------------------------------
 *  Módulo encargado de la sincronización con Firebase relacionada a
 *  ACTUADORES y EVENTOS -- separado de ServicioFirebase (que solo se
 *  encarga de subir las lecturas de sensores) para mantener cada
 *  archivo enfocado en una sola responsabilidad.
 *
 *  Tiene DOS direcciones de comunicación, a diferencia del resto del
 *  proyecto que solo subía datos:
 *
 *    SUBIR (PUT):  el ESP32 reporta el estado real de cada actuador
 *                  a /actuadores, y agrega un evento nuevo a /eventos
 *                  cada vez que algo relevante ocurre.
 *
 *    BAJAR (GET):  el ESP32 lee /actuadores para saber si alguna app
 *                  cambió el modo a MANUAL o envió una orden manual
 *                  (encender/apagar). Esto es lo que permite que las
 *                  apps "anulen" la decisión automática.
 * ============================================================================
 */

#ifndef SERVICIO_ACTUADORES_FIREBASE_H
#define SERVICIO_ACTUADORES_FIREBASE_H

#include <Arduino.h>
#include "GestorActuadores.h"

// Resultado de leer /actuadores desde Firebase. Se usa para que
// main.cpp pueda aplicar las órdenes manuales sin que este servicio
// necesite conocer la clase GestorActuadores directamente.
struct EstadoRemotoActuador
{
    ModoActuador modo;
    bool ordenManual;
    bool valido; // false si hubo error de lectura/parseo
};

class ServicioActuadoresFirebase
{
public:
    // Sube el estado actual de los 4 actuadores a /actuadores (PUT).
    static bool reportarEstadoActuadores(const GestorActuadores &gestor);

    // Agrega un nuevo evento al historial /eventos usando POST (push),
    // que SÍ conserva historial completo, a diferencia de PUT.
    static bool registrarEvento(const char *tipo, const char *origen,
                                const char *mensaje, const char *nivel);

    // Lee el modo/orden manual de UN actuador específico desde Firebase.
    // nodoActuador: "calefactor", "extractor", "humidificador", "alimentador"
    static EstadoRemotoActuador leerEstadoRemoto(const char *nodoActuador);
};

#endif // SERVICIO_ACTUADORES_FIREBASE_H
