/*
 * ============================================================================
 *  ServicioFirebase.h
 * ----------------------------------------------------------------------------
 *  Módulo encargado ÚNICAMENTE de enviar datos a Firebase Realtime Database
 *  mediante peticiones HTTP REST. No usa una librería pesada de Firebase;
 *  usa HTTPClient directamente, lo cual es más liviano y más fácil de
 *  depurar para un proyecto académico.
 * ============================================================================
 */

#ifndef SERVICIO_FIREBASE_H
#define SERVICIO_FIREBASE_H

#include <Arduino.h>

class ServicioFirebase
{
public:
    // Envía las 3 lecturas (temperatura, humedad, peso) al nodo configurado
    // en config.h (FIREBASE_PATH). Usa el método HTTP PUT, que SOBREESCRIBE
    // el contenido del nodo cada vez (es decir, Firebase siempre tendrá el
    // último valor, no un historial).
    //
    // Devuelve 'true' si Firebase respondió con éxito (código HTTP 200),
    // 'false' en caso de error (sin WiFi, URL incorrecta, etc).
    static bool enviarLecturaCompleta(float temperatura, float humedad, float peso, bool obstaculo, int calidadAire);
};

#endif // SERVICIO_FIREBASE_H
