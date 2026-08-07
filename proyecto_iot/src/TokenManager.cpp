/*
 * ============================================================================
 * TokenManager.cpp
 * ----------------------------------------------------------------------------
 * Implementación del almacenamiento persistente del token JWT.
 * ============================================================================
 */

#include "TokenManager.h"

TokenManager::TokenManager() {}

void TokenManager::iniciar() {
    // No abrimos aquí; se abre/cierra en cada operación para evitar corrupción
    Serial.println("[TokenManager] Listo para gestionar tokens JWT");
}

void TokenManager::guardarToken(const String& token) {
    _prefs.begin(NAMESPACE, false);  // false = modo escritura
    _prefs.putString(KEY_TOKEN, token);
    _prefs.end();

    // NO logueamos el token completo (seguridad)
    Serial.print("[TokenManager] Token guardado (");
    Serial.print(token.length());
    Serial.println(" chars)");
}

String TokenManager::leerToken() {
    _prefs.begin(NAMESPACE, true);  // true = modo solo lectura
    String token = _prefs.getString(KEY_TOKEN, "");
    _prefs.end();
    return token;
}

void TokenManager::borrarToken() {
    _prefs.begin(NAMESPACE, false);
    _prefs.remove(KEY_TOKEN);
    _prefs.end();
    Serial.println("[TokenManager] Token eliminado");
}

bool TokenManager::tieneToken() {
    return leerToken().length() > 0;
}

String TokenManager::getAuthorizationHeader() {
    String token = leerToken();
    if (token.length() == 0) {
        return "";
    }
    return "Bearer " + token;
}
