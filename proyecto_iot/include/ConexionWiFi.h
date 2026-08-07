/*
 * ============================================================================
 *  ConexionWiFi.h
 * ----------------------------------------------------------------------------
 *  Módulo encargado ÚNICAMENTE de gestionar la conexión WiFi del ESP32.
 *  Mantenerlo separado permite reutilizarlo en cualquier otro proyecto
 *  sin tener que copiar lógica de sensores ni de Firebase.
 * ============================================================================
 */

#ifndef CONEXION_WIFI_H
#define CONEXION_WIFI_H

#include <Arduino.h>

class ConexionWiFi {
public:
    // Intenta conectar al WiFi definido en config.h.
    // Es una función "bloqueante": el programa espera aquí hasta conectar.
    static void conectar();

    // Devuelve true si el ESP32 sigue conectado al WiFi.
    // Se debe llamar antes de cada envío HTTP para evitar errores.
    static bool estaConectado();

    // Devuelve la intensidad de la señal WiFi en dBm (útil para diagnóstico
    // y para mostrarla en la pantalla OLED).
    static int obtenerRSSI();
};

#endif // CONEXION_WIFI_H
