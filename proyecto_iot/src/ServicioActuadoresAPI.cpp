/*
 * ============================================================================
 * ServicioActuadoresAPI.cpp
 * ----------------------------------------------------------------------------
 * Implementación de sincronización de actuadores y eventos.
 * ============================================================================
 */

#include "ServicioActuadoresAPI.h"
#include "config.h"
#include "ConexionWiFi.h"

TokenManager* ServicioActuadoresAPI::_tokenManager = nullptr;

void ServicioActuadoresAPI::iniciar(TokenManager* tokenManager) {
    _tokenManager = tokenManager;
    Serial.println("[ServicioActuadoresAPI] Inicializado");
}

String ServicioActuadoresAPI::getBaseUrl() {
    return String(API_BASE_URL);
}

void ServicioActuadoresAPI::configurarHeaders(HTTPClient& http) {
    http.addHeader("Content-Type", "application/json");
    String auth = _tokenManager->getAuthorizationHeader();
    if (auth.length() > 0) {
        http.addHeader("Authorization", auth);
    }
}

void ServicioActuadoresAPI::manejarErrorHTTP(int codigo, const char* contexto) {
    if (codigo == 401) {
        Serial.printf("[ActuadoresAPI] %s → 401. Token inválido.\n", contexto);
        _tokenManager->borrarToken();
    } else if (codigo == 429) {
        Serial.printf("[ActuadoresAPI] %s → 429 Rate Limited.\n", contexto);
    } else if (codigo >= 500) {
        Serial.printf("[ActuadoresAPI] %s → %d Server Error.\n", contexto, codigo);
    } else if (codigo == -1) {
        Serial.printf("[ActuadoresAPI] %s → Error de conexión.\n", contexto);
    } else if (codigo != 200 && codigo != 201) {
        Serial.printf("[ActuadoresAPI] %s → HTTP %d\n", contexto, codigo);
    }
}

bool ServicioActuadoresAPI::asegurarToken() {
    if (!_tokenManager->tieneToken()) {
        Serial.println("[ActuadoresAPI] Sin token. No se puede continuar.");
        return false;
    }
    return true;
}

bool ServicioActuadoresAPI::reportarEstadoActuadores(const GestorActuadores& gestor) {
    if (!ConexionWiFi::estaConectado() || !asegurarToken()) {
        return false;
    }

    String url = getBaseUrl() + "/actuators/state";

    StaticJsonDocument<512> doc;
    doc["device_id"] = DEVICE_ID;
    doc["nombre"] = "all";  // Enviamos todos en un solo payload
    doc["estado"] = true;   // Placeholder, en realidad enviamos cada uno

    // Enviamos cada actuador como un objeto separado en un array
    JsonArray actuadores = doc.createNestedArray("actuadores");

    JsonObject calefactor = actuadores.createNestedObject();
    calefactor["nombre"] = "calefactor";
    calefactor["estado"] = gestor.calefactor().estaEncendido();
    calefactor["modo"] = (gestor.calefactor().obtenerModo() == ModoActuador::AUTO) ? "AUTO" : "MANUAL";

    JsonObject extractor = actuadores.createNestedObject();
    extractor["nombre"] = "extractor";
    extractor["estado"] = gestor.extractor().estaEncendido();
    extractor["modo"] = (gestor.extractor().obtenerModo() == ModoActuador::AUTO) ? "AUTO" : "MANUAL";

    JsonObject humidificador = actuadores.createNestedObject();
    humidificador["nombre"] = "humidificador";
    humidificador["estado"] = gestor.humidificador().estaEncendido();
    humidificador["modo"] = (gestor.humidificador().obtenerModo() == ModoActuador::AUTO) ? "AUTO" : "MANUAL";

    JsonObject alimentador = actuadores.createNestedObject();
    alimentador["nombre"] = "alimentador";
    alimentador["estado"] = gestor.alimentador().estaActivo();

    String cuerpoJson;
    serializeJson(doc, cuerpoJson);

    HTTPClient http;
    http.setTimeout(8000);
    http.begin(url);
    configurarHeaders(http);

    int codigo = http.POST(cuerpoJson);
    http.end();

    bool exito = (codigo == 200);
    if (!exito) {
        manejarErrorHTTP(codigo, "Reportar estado");
    }

    return exito;
}

bool ServicioActuadoresAPI::registrarEvento(
    const char* tipo,
    const char* origen,
    const char* mensaje,
    const char* nivel
) {
    if (!ConexionWiFi::estaConectado() || !asegurarToken()) {
        return false;
    }

    String url = getBaseUrl() + "/events";

    StaticJsonDocument<512> doc;
    doc["device_id"] = DEVICE_ID;
    doc["tipo"] = tipo;
    doc["origen"] = origen;
    doc["mensaje"] = mensaje;
    doc["nivel"] = nivel;

    String cuerpoJson;
    serializeJson(doc, cuerpoJson);

    HTTPClient http;
    http.setTimeout(8000);
    http.begin(url);
    configurarHeaders(http);

    int codigo = http.POST(cuerpoJson);
    http.end();

    bool exito = (codigo == 201);
    if (exito) {
        Serial.printf("[Evento] %s: %s\n", origen, mensaje);
    } else {
        manejarErrorHTTP(codigo, "Registrar evento");
    }

    return exito;
}

bool ServicioActuadoresAPI::leerComandosPendientes(
    ComandoRemoto comandos[],
    int maxComandos,
    int& cantidad
) {
    cantidad = 0;

    if (!ConexionWiFi::estaConectado() || !asegurarToken()) {
        return false;
    }

    String url = getBaseUrl() + "/actuators/commands";

    HTTPClient http;
    http.setTimeout(8000);
    http.begin(url);
    configurarHeaders(http);

    int codigo = http.GET();
    bool exito = false;

    if (codigo == 200) {
        String respuesta = http.getString();

        StaticJsonDocument<2048> doc;  // Buffer grande para múltiples comandos
        DeserializationError error = deserializeJson(doc, respuesta);

        if (!error && doc.is<JsonArray>()) {
            JsonArray arr = doc.as<JsonArray>();
            int i = 0;
            for (JsonObject cmd : arr) {
                if (i >= maxComandos) break;

                comandos[i].id = cmd["_id"] | "";
                comandos[i].nombre = cmd["nombre"] | "";
                comandos[i].modo = cmd["modo"] | "AUTO";
                comandos[i].ordenManual = cmd["orden_manual"] | false;
                comandos[i].valido = true;
                i++;
            }
            cantidad = i;
            exito = true;

            if (cantidad > 0) {
                Serial.printf("[ActuadoresAPI] %d comandos pendientes recibidos.\n", cantidad);
            }
        }
    } else {
        manejarErrorHTTP(codigo, "Leer comandos");
    }

    http.end();
    return exito;
}

bool ServicioActuadoresAPI::confirmarComandoEjecutado(const String& commandId) {
    if (!ConexionWiFi::estaConectado() || !asegurarToken() || commandId.length() == 0) {
        return false;
    }

    String url = getBaseUrl() + "/actuators/commands/" + commandId + "/executed";

    HTTPClient http;
    http.setTimeout(5000);
    http.begin(url);
    configurarHeaders(http);

    int codigo = http.POST("{}");  // Body vacío, solo necesitamos el POST
    http.end();

    bool exito = (codigo == 200);
    if (exito) {
        Serial.printf("[ActuadoresAPI] Comando %s confirmado como ejecutado.\n", commandId.c_str());
    } else {
        manejarErrorHTTP(codigo, "Confirmar comando");
    }

    return exito;
}
