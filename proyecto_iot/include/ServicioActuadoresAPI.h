/*
 * ============================================================================
 * ServicioActuadoresAPI.h
 * ----------------------------------------------------------------------------
 * Reemplazo de ServicioActuadoresFirebase.h
 * 
 * Sincronización bidireccional de actuadores con el backend FastAPI:
 *   - SUBIR: reportar estado real de actuadores
 *   - SUBIR: registrar eventos en el historial
 *   - BAJAR: leer comandos pendientes enviados desde la app
 * ============================================================================
 */

#ifndef SERVICIO_ACTUADORES_API_H
#define SERVICIO_ACTUADORES_API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "TokenManager.h"
#include "GestorActuadores.h"

// Resultado de leer comandos pendientes desde el backend
struct ComandoRemoto {
    String id;           // ID del comando en MongoDB
    String nombre;       // "calefactor", "extractor", "humidificador", "alimentador"
    String modo;         // "AUTO" o "MANUAL"
    bool ordenManual;    // true=encender, false=apagar (solo si modo=MANUAL)
    bool valido;         // false si hubo error de lectura
};

class ServicioActuadoresAPI {
public:
    static void iniciar(TokenManager* tokenManager);

    // SUBIR: Reporta el estado actual de los 4 actuadores
    static bool reportarEstadoActuadores(const GestorActuadores& gestor);

    // SUBIR: Registra un evento en el historial
    static bool registrarEvento(
        const char* tipo,
        const char* origen,
        const char* mensaje,
        const char* nivel
    );

    // BAJAR: Lee comandos pendientes para este dispositivo
    // El ESP32 debe llamar esto periódicamente (cada 10 segundos)
    static bool leerComandosPendientes(ComandoRemoto comandos[], int maxComandos, int& cantidad);

    // CONFIRMAR: Marca un comando como ejecutado por el ESP32
    static bool confirmarComandoEjecutado(const String& commandId);

private:
    static TokenManager* _tokenManager;

    static String getBaseUrl();
    static void configurarHeaders(HTTPClient& http);
    static void manejarErrorHTTP(int codigo, const char* contexto);
    static bool asegurarToken();
};

#endif // SERVICIO_ACTUADORES_API_H
