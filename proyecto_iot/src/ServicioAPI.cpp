/*
 * ============================================================================
 * ServicioAPI.cpp
 * ----------------------------------------------------------------------------
 * Implementación del envío de datos al backend FastAPI.
 * 
 * Flujo de envío:
 *   1. Verifica WiFi conectado
 *   2. Lee token JWT de flash
 *   3. Si no hay token → autentica primero
 *   4. Construye JSON con lecturas
 *   5. POST a /sensors/readings
 *   6. Si 401 → reautentica y reintenta (una vez)
 *   7. Si 429 → espera antes de reintentar
 *   8. Si 5xx → loguea error, no reintenta inmediatamente
 * ============================================================================
 */

#include "ServicioAPI.h"
#include "config.h"
#include "ConexionWiFi.h"

TokenManager* ServicioAPI::_tokenManager = nullptr;
unsigned long ServicioAPI::_ultimoReintento = 0;

void ServicioAPI::iniciar(TokenManager* tokenManager) {
    _tokenManager = tokenManager;
    Serial.println("[ServicioAPI] Inicializado");

    // Al iniciar, intentamos autenticar si no tenemos token
    if (!_tokenManager->tieneToken()) {
        Serial.println("[ServicioAPI] No hay token. Intentando autenticación...");
        autenticarDispositivo();
    }
}

String ServicioAPI::getBaseUrl() {
    // Usa HTTP para desarrollo. Cambia a HTTPS en producción.
    // Ejemplo: "http://192.168.1.100:8000" o "https://tu-api.com"
    return String(API_BASE_URL);
}

void ServicioAPI::configurarHeaders(HTTPClient& http) {
    http.addHeader("Content-Type", "application/json");

    String auth = _tokenManager->getAuthorizationHeader();
    if (auth.length() > 0) {
        http.addHeader("Authorization", auth);
    }
}

void ServicioAPI::manejarErrorHTTP(int codigo, const char* contexto) {
    switch (codigo) {
        case 401:
            Serial.printf("[ServicioAPI] %s → 401 Unauthorized. Token inválido o expirado.\n", contexto);
            _tokenManager->borrarToken();
            // Intentamos reautenticar en el próximo ciclo
            break;

        case 403:
            Serial.printf("[ServicioAPI] %s → 403 Forbidden. Dispositivo no autorizado.\n", contexto);
            break;

        case 422:
            Serial.printf("[ServicioAPI] %s → 422 Validation Error. Datos inválidos.\n", contexto);
            break;

        case 429:
            Serial.printf("[ServicioAPI] %s → 429 Rate Limited. Esperando...\n", contexto);
            _ultimoReintento = millis();
            break;

        case 500:
        case 502:
        case 503:
            Serial.printf("[ServicioAPI] %s → %d Server Error. Servidor no disponible.\n", contexto, codigo);
            break;

        case -1:
            Serial.printf("[ServicioAPI] %s → Error de conexión (timeout o sin red).\n", contexto);
            break;

        default:
            Serial.printf("[ServicioAPI] %s → Error HTTP %d\n", contexto, codigo);
    }
}

bool ServicioAPI::autenticarDispositivo() {
    if (!ConexionWiFi::estaConectado()) {
        Serial.println("[ServicioAPI] Sin WiFi. No se puede autenticar.");
        return false;
    }

    String url = getBaseUrl() + "/auth/device/login";

    StaticJsonDocument<256> doc;
    doc["device_id"] = DEVICE_ID;
    doc["device_secret"] = DEVICE_SECRET;

    String cuerpoJson;
    serializeJson(doc, cuerpoJson);

    HTTPClient http;
    http.setTimeout(10000);  // 10 segundos de timeout
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int codigo = http.POST(cuerpoJson);
    bool exito = false;

    if (codigo == 200) {
        String respuesta = http.getString();

        StaticJsonDocument<512> respDoc;
        DeserializationError error = deserializeJson(respDoc, respuesta);

        if (!error && respDoc.containsKey("access_token")) {
            const char* token = respDoc["access_token"];
            _tokenManager->guardarToken(token);
            Serial.println("[ServicioAPI] ✅ Autenticación exitosa. Token guardado.");
            exito = true;
        } else {
            Serial.println("[ServicioAPI] ❌ Respuesta de autenticación inválida.");
        }
    } else {
        manejarErrorHTTP(codigo, "Autenticación");
    }

    http.end();
    return exito;
}

bool ServicioAPI::enviarLecturaCompleta(
    float temperatura,
    float humedad,
    float peso,
    bool obstaculo,
    int calidadAire,
    float voltajeAire
) {
    // 1. Verificar WiFi
    if (!ConexionWiFi::estaConectado()) {
        Serial.println("[ServicioAPI] Sin conexión WiFi.");
        return false;
    }

    // 2. Verificar que tenemos token
    if (!_tokenManager->tieneToken()) {
        Serial.println("[ServicioAPI] Sin token. Intentando autenticar...");
        if (!autenticarDispositivo()) {
            return false;
        }
    }

    // 3. Construir URL y JSON
    String url = getBaseUrl() + "/sensors/readings";

    StaticJsonDocument<512> doc;
    doc["device_id"] = DEVICE_ID;
    doc["temperatura"] = temperatura;
    doc["humedad"] = humedad;
    doc["peso"] = peso;
    doc["obstaculo"] = obstaculo;
    doc["calidad_aire"] = calidadAire;
    doc["voltaje_aire"] = voltajeAire;

    // El ESP32 no tiene reloj RTC con batería, así que NO envía timestamp.
    // El backend usará la hora del servidor (received_at).
    // Si más adelante agregas NTP, descomenta la siguiente línea:
    // doc["timestamp"] = obtenerEpochUTC();  // Necesitarías implementar NTP

    String cuerpoJson;
    serializeJson(doc, cuerpoJson);

    // 4. Enviar petición
    HTTPClient http;
    http.setTimeout(8000);  // 8 segundos de timeout (evita bloqueos)
    http.begin(url);
    configurarHeaders(http);

    int codigo = http.POST(cuerpoJson);
    bool exito = (codigo == 201);

    if (exito) {
        Serial.println("[ServicioAPI] ✅ Lectura enviada correctamente.");
    } else {
        manejarErrorHTTP(codigo, "Envío de lectura");

        // Si fue 401, intentamos reautenticar UNA vez y reintentar
        if (codigo == 401) {
            Serial.println("[ServicioAPI] Reintentando con nuevo token...");
            http.end();

            if (autenticarDispositivo()) {
                // Reintento único
                http.begin(url);
                configurarHeaders(http);
                codigo = http.POST(cuerpoJson);
                exito = (codigo == 201);

                if (exito) {
                    Serial.println("[ServicioAPI] ✅ Lectura enviada en reintento.");
                } else {
                    manejarErrorHTTP(codigo, "Reintento de envío");
                }
            }
        }
    }

    http.end();
    return exito;
}

bool ServicioAPI::tokenValido() {
    if (!_tokenManager->tieneToken()) {
        return false;
    }

    // Hacemos un GET a /health para verificar que el token funciona
    // (el health check es público, así que esto no valida el token realmente,
    // pero verifica que el servidor responde)
    String url = getBaseUrl() + "/health";

    HTTPClient http;
    http.setTimeout(5000);
    http.begin(url);
    configurarHeaders(http);

    int codigo = http.GET();
    http.end();

    return (codigo == 200);
}
