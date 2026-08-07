/*
 * ============================================================================
 *  ServicioActuadoresFirebase.cpp
 * ----------------------------------------------------------------------------
 *  Implementación de la sincronización bidireccional con Firebase para
 *  actuadores y eventos.
 * ============================================================================
 */

#include "ServicioActuadoresFirebase.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "ConexionWiFi.h"

// ────────────────────────────────────────────────────────────────────────
// SUBIR: reportar el estado real de los 4 actuadores
// ────────────────────────────────────────────────────────────────────────
bool ServicioActuadoresFirebase::reportarEstadoActuadores(const GestorActuadores& gestor) {
    if (!ConexionWiFi::estaConectado()) return false;

    String url = String(FIREBASE_HOST) + "/actuadores.json?auth=" + String(FIREBASE_AUTH);

    // Armamos el JSON completo de los 4 actuadores en un solo PUT, para
    // no hacer 4 peticiones HTTP separadas en cada ciclo.
    StaticJsonDocument<512> doc;

    JsonObject calefactor = doc.createNestedObject("calefactor");
    calefactor["estado"] = gestor.calefactor().estaEncendido();
    calefactor["modo"] = (gestor.calefactor().obtenerModo() == ModoActuador::AUTO) ? "AUTO" : "MANUAL";

    JsonObject extractor = doc.createNestedObject("extractor");
    extractor["estado"] = gestor.extractor().estaEncendido();
    extractor["modo"] = (gestor.extractor().obtenerModo() == ModoActuador::AUTO) ? "AUTO" : "MANUAL";

    JsonObject humidificador = doc.createNestedObject("humidificador");
    humidificador["estado"] = gestor.humidificador().estaEncendido();
    humidificador["modo"] = (gestor.humidificador().obtenerModo() == ModoActuador::AUTO) ? "AUTO" : "MANUAL";

    JsonObject alimentador = doc.createNestedObject("alimentador");
    alimentador["estado"] = gestor.alimentador().estaActivo();
    alimentador["ultimaAlimentacion"] = gestor.alimentador().obtenerUltimaAlimentacion();

    String cuerpoJson;
    serializeJson(doc, cuerpoJson);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int codigo = http.PUT(cuerpoJson);
    http.end();

    return codigo == 200;
}

// ────────────────────────────────────────────────────────────────────────
// SUBIR: registrar un evento en el historial (POST, no PUT)
// ────────────────────────────────────────────────────────────────────────
bool ServicioActuadoresFirebase::registrarEvento(const char* tipo, const char* origen,
                                                    const char* mensaje, const char* nivel) {
    if (!ConexionWiFi::estaConectado()) return false;

    // OJO: aquí usamos /eventos, NO /eventos/algo -- el .json al final
    // de un POST le dice a Firebase "genera tú un ID único" (equivalente
    // a push() en el SDK oficial). Por eso este nodo SÍ acumula
    // historial, a diferencia de /sensores y /actuadores que usan PUT.
    String url = String(FIREBASE_HOST) + "/eventos.json?auth=" + String(FIREBASE_AUTH);

    StaticJsonDocument<256> doc;
    doc["tipo"] = tipo;
    doc["origen"] = origen;
    doc["mensaje"] = mensaje;
    doc["nivel"] = nivel;
    doc["timestamp"] = millis();

    String cuerpoJson;
    serializeJson(doc, cuerpoJson);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int codigo = http.POST(cuerpoJson); // <-- POST, no PUT
    http.end();

    if (codigo == 200) {
        Serial.printf("[Evento] %s: %s\n", origen, mensaje);
        return true;
    }
    return false;
}

// ────────────────────────────────────────────────────────────────────────
// BAJAR: leer si una app cambió el modo/orden de un actuador
// ────────────────────────────────────────────────────────────────────────
EstadoRemotoActuador ServicioActuadoresFirebase::leerEstadoRemoto(const char* nodoActuador) {
    EstadoRemotoActuador resultado;
    resultado.modo = ModoActuador::AUTO;
    resultado.ordenManual = false;
    resultado.valido = false;

    if (!ConexionWiFi::estaConectado()) return resultado;

    String url = String(FIREBASE_HOST) + "/actuadores/" + nodoActuador + ".json?auth=" + String(FIREBASE_AUTH);

    HTTPClient http;
    http.begin(url);
    int codigo = http.GET(); // <-- aquí SÍ usamos GET, para leer

    if (codigo == 200) {
        String cuerpo = http.getString();

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, cuerpo);

        if (!error && !doc.isNull()) {
            const char* modoTexto = doc["modo"] | "AUTO";
            resultado.modo = (strcmp(modoTexto, "MANUAL") == 0) ? ModoActuador::MANUAL : ModoActuador::AUTO;
            resultado.ordenManual = doc["ordenManual"] | false;
            resultado.valido = true;
        }
    }

    http.end();
    return resultado;
}
