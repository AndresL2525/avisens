/*
 * ============================================================================
 * TokenManager.h
 * ----------------------------------------------------------------------------
 * Gestiona el almacenamiento del token JWT en la memoria flash (NVS) del ESP32.
 * Usa Preferences (API de ESP32) para persistir el token entre reinicios.
 * 
 * El token se almacena en el namespace "avisens_auth" con la clave "jwt_token".
 * Si el token expira o es inválido, el ESP32 debe re-autenticarse.
 * ============================================================================
 */

#ifndef TOKEN_MANAGER_H
#define TOKEN_MANAGER_H

#include <Preferences.h>
#include <Arduino.h>

class TokenManager {
public:
    TokenManager();

    // Inicializa el almacenamiento NVS
    void iniciar();

    // Guarda el token JWT en flash
    void guardarToken(const String& token);

    // Lee el token desde flash. Retorna "" si no hay token.
    String leerToken();

    // Elimina el token (logout / token inválido)
    void borrarToken();

    // Verifica si hay un token almacenado
    bool tieneToken();

    // Retorna el token como header Authorization: Bearer <token>
    String getAuthorizationHeader();

private:
    Preferences _prefs;
    static constexpr const char* NAMESPACE = "avisens_auth";
    static constexpr const char* KEY_TOKEN = "jwt_token";
};

#endif // TOKEN_MANAGER_H
