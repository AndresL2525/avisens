/*
 * ============================================================================
 * ServicioAPI.h
 * ----------------------------------------------------------------------------
 * Reemplazo de ServicioFirebase.h
 * 
 * Envia lecturas de sensores al backend FastAPI via HTTP REST.
 * Usa el token JWT almacenado en flash para autenticación.
 * 
 * ⚠️ IMPORTANTE: Por defecto usa HTTP (puerto 8000) para desarrollo.
 *    En producción REAL, DEBES usar HTTPS. El ESP32 tiene poca RAM
 *    (520KB), así que los certificados grandes pueden causar fallos.
 *    Opciones:
 *      1. Usa un proxy local (Raspberry Pi) que hable HTTP con el ESP32
 *         y HTTPS con la nube.
 *      2. Usa certificados ECDSA (más pequeños que RSA).
 *      3. Fija el certificado del servidor en el firmware (setCACert).
 * ============================================================================
 */

#ifndef SERVICIO_API_H
#define SERVICIO_API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "TokenManager.h"

class ServicioAPI {
public:
    // Inicializa el servicio con el TokenManager compartido
    static void iniciar(TokenManager* tokenManager);

    // Envia lectura completa de sensores al backend
    // Retorna true si el servidor respondió 201 Created
    static bool enviarLecturaCompleta(
        float temperatura,
        float humedad,
        float peso,
        bool obstaculo,
        int calidadAire,
        float voltajeAire = 0.0
    );

    // Autentica el dispositivo y obtiene un token JWT
    // Se llama al inicio o cuando el token es inválido (401)
    static bool autenticarDispositivo();

    // Verifica si el token actual es válido (hace un ping al backend)
    static bool tokenValido();

private:
    static TokenManager* _tokenManager;
    static unsigned long _ultimoReintento;
    static const unsigned long RETRY_INTERVAL_MS = 30000;  // 30 seg entre reintentos

    // Construye la URL base del backend
    static String getBaseUrl();

    // Configura headers comunes (Content-Type + Authorization)
    static void configurarHeaders(HTTPClient& http);

    // Maneja códigos de error HTTP y decide si reautenticar
    static void manejarErrorHTTP(int codigo, const char* contexto);

    // Reintenta la petición con backoff exponencial (stub para futuro)
    static bool reintentarConBackoff();
};

#endif // SERVICIO_API_H
