/*
 * ============================================================================
 *  ServicioFirebase.cpp
 * ----------------------------------------------------------------------------
 *  Implementación del envío de datos a Firebase Realtime Database
 *  usando peticiones HTTP REST (PUT) con cuerpo en formato JSON.
 * ============================================================================
 */

#include "ServicioFirebase.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "ConexionWiFi.h"

bool ServicioFirebase::enviarLecturaCompleta(float temperatura, float humedad, float peso, bool obstaculo, int calidadAire)
{
    if (!ConexionWiFi::estaConectado())
    {
        Serial.println("[Firebase] Sin conexion WiFi.");
        return false;
    }

    String url = String(FIREBASE_HOST) + String(FIREBASE_PATH) + ".json?auth=" + String(FIREBASE_AUTH);

    StaticJsonDocument<512> doc;
    doc["temperatura"] = temperatura;
    doc["humedad"] = humedad;
    doc["peso"] = peso;
    doc["obstaculo"] = obstaculo;
    doc["calidadAire"] = calidadAire;
    doc["timestamp"] = millis();

    String cuerpoJson;
    serializeJson(doc, cuerpoJson);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int codigo = http.PUT(cuerpoJson);
    bool exito = (codigo == 200);

    if (exito)
        Serial.println("[Firebase] Datos enviados.");
    else
        Serial.printf("[Firebase] Error HTTP: %d\n", codigo);

    http.end();
    return exito;
}