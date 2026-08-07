/*
 * ============================================================================
 *  ConexionWiFi.cpp
 * ----------------------------------------------------------------------------
 *  Implementación de la lógica de conexión WiFi.
 *  MODIFICADO: Incluye tiempo de espera (timeout) para no bloquear el programa.
 * ============================================================================
 */

#include "ConexionWiFi.h"
#include <WiFi.h>
#include "config.h"

void ConexionWiFi::conectar()
{
    Serial.println("[WiFi] Iniciando conexion...");

    // --- FUERZA el modo Estación (Cliente) ---
    // Esto evita que el ESP32 intente crear su propia red WiFi.
    WiFi.mode(WIFI_STA);

    // Inicia la conexión con las credenciales de config.h
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // --- TIEMPO DE ESPERA: 15 segundos ---
    // Esto evita que el ESP32 se quede colgado para siempre.
    unsigned long tiempoInicio = millis();
    const unsigned long TIEMPO_MAXIMO = 15000; // 15 segundos

    while (WiFi.status() != WL_CONNECTED && millis() - tiempoInicio < TIEMPO_MAXIMO)
    {
        delay(400);
        Serial.print(".");
    }

    // --- Evaluar el resultado ---
    Serial.println(); // Salto de línea después de los puntos

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("[WiFi] Conectado. IP asignada: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("[WiFi] ERROR: Tiempo de espera agotado.");
        Serial.println("[WiFi] Revisa que la red sea 2.4GHz y las credenciales esten bien escritas.");
    }
}

bool ConexionWiFi::estaConectado()
{
    return WiFi.status() == WL_CONNECTED;
}

int ConexionWiFi::obtenerRSSI()
{
    return WiFi.RSSI();
}