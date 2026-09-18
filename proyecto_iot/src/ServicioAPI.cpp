/**
 * =============================================================================
 * ServicioAPI.cpp — Implementación del cliente HTTP REST
 * =============================================================================
 */

#include "ServicioAPI.h"
#include <ArduinoJson.h>

// ═══════════════════════════════════════════════════════════
// ─── CONSTRUCTOR ─────────────────────────────────────────
// ═══════════════════════════════════════════════════════════

ServicioAPI::ServicioAPI(
    const String& backend_url,
    const String& device_id,
    const String& device_secret)
    : backend_url_(backend_url),
      device_id_(device_id),
      device_secret_(device_secret),
      access_token_(""),
      token_expiry_time_(0) {
  LOG_DEBUG("ServicioAPI inicializado");
  LOG_DEBUG("Backend: " + backend_url_);
  LOG_DEBUG("Device ID: " + device_id_);
}

// ═══════════════════════════════════════════════════════════
// ─── AUTENTICACIÓN ──────────────────────────────────────
// ═══════════════════════════════════════════════════════════

bool ServicioAPI::autenticarDispositivo() {
  if (!WiFi.isConnected()) {
    LOG_WARN("WiFi no conectado - No se puede autenticar");
    return false;
  }

  String url = backend_url_ + "/auth/device/login";

  // Crear payload de autenticación
  DynamicJsonDocument doc(256);
  doc["device_id"] = device_id_;
  doc["secret"] = device_secret_;

  String payload;
  serializeJson(doc, payload);

  String respuesta = realizarRequestConReintentos("POST", "/auth/device/login", payload);

  if (respuesta.isEmpty()) {
    LOG_ERROR("Respuesta vacía de autenticación");
    return false;
  }

  // Parsear respuesta
  DynamicJsonDocument response(512);
  DeserializationError error = deserializeJson(response, respuesta);

  if (error) {
    LOG_ERROR("Error parseando respuesta de autenticación");
    return false;
  }

  if (!response.containsKey("access_token")) {
    LOG_ERROR("No se recibió access_token");
    return false;
  }

  access_token_ = response["access_token"].as<String>();

  // Calcular tiempo de expiración (suponemos 1 hora, -5 min de margen)
  token_expiry_time_ = millis() + (3600 - 300) * 1000UL;

  LOG_DEBUG("✓ Dispositivo autenticado exitosamente");
  Serial.println("✓ Token JWT: " + access_token_.substring(0, 20) + "...");

  return true;
}

// ═══════════════════════════════════════════════════════════
// ─── ENVÍO DE LECTURAS ────────────────────────────────────
// ═══════════════════════════════════════════════════════════

bool ServicioAPI::enviarLecturas(const LecturaSensores& datos) {
  if (!estaAutenticado()) {
    LOG_WARN("No autenticado - Intentando autenticar...");
    if (!autenticarDispositivo()) {
      return false;
    }
  }

  // Crear payload JSON
  DynamicJsonDocument doc(512);
  doc["device_id"] = datos.device_id;
  doc["temperatura"] = datos.temperatura;
  doc["humedad"] = datos.humedad;
  doc["peso"] = datos.peso;
  doc["obstaculo"] = datos.obstaculo;
  doc["calidad_aire"] = datos.calidad_aire;
  doc["voltaje_aire"] = datos.voltaje_aire;

  String payload;
  serializeJson(doc, payload);

  String respuesta = realizarRequestConReintentos("POST", "/sensors/readings", payload);

  if (respuesta.isEmpty()) {
    LOG_WARN("Error enviando lecturas - Respuesta vacía");
    return false;
  }

  DynamicJsonDocument response(512);
  DeserializationError error = deserializeJson(response, respuesta);

  if (error) {
    LOG_WARN("Error parseando respuesta de lecturas");
    return false;
  }

  if (response["success"] == true) {
    LOG_DEBUG("✓ Lectura enviada exitosamente");
    return true;
  }

  LOG_WARN("Error en respuesta del backend");
  return false;
}

// ═══════════════════════════════════════════════════════════
// ─── CONSULTA DE COMANDOS ────────────────────────────────
// ═══════════════════════════════════════════════════════════

String ServicioAPI::consultarComandosPendientes() {
  if (!estaAutenticado()) {
    return "";
  }

  String endpoint = "/actuators/commands?device_id=" + device_id_;
  String respuesta = realizarRequestConReintentos("GET", endpoint);

  if (respuesta.isEmpty()) {
    LOG_DEBUG("No hay comandos pendientes");
    return "[]";
  }

  return respuesta;
}

// ═══════════════════════════════════════════════════════════
// ─── CONFIRMACIÓN DE COMANDOS ────────────────────────────
// ═══════════════════════════════════════════════════════════

bool ServicioAPI::confirmarComando(
    const String& command_id,
    bool success,
    const String& mensaje) {
  if (!estaAutenticado()) {
    return false;
  }

  DynamicJsonDocument doc(256);
  doc["executed"] = success;
  doc["message"] = mensaje.isEmpty() ? "Ejecutado" : mensaje;

  String payload;
  serializeJson(doc, payload);

  String endpoint = "/actuators/commands/" + command_id + "/executed";
  String respuesta = realizarRequestConReintentos("POST", endpoint, payload);

  if (respuesta.isEmpty()) {
    return false;
  }

  DynamicJsonDocument response(256);
  if (deserializeJson(response, respuesta) != DeserializationError::Ok) {
    return false;
  }

  return response["success"] == true;
}

// ═══════════════════════════════════════════════════════════
// ─── REPORTE DE ESTADO ────────────────────────────────────
// ═══════════════════════════════════════════════════════════

bool ServicioAPI::reportarEstadoActuador(
    const String& nombre,
    bool estado,
    const String& modo) {
  if (!estaAutenticado()) {
    return false;
  }

  DynamicJsonDocument doc(256);
  doc["device_id"] = device_id_;
  doc["nombre"] = nombre;
  doc["estado"] = estado;
  doc["modo"] = modo;
  doc["timestamp"] = (unsigned long)millis();

  String payload;
  serializeJson(doc, payload);

  String respuesta = realizarRequestConReintentos("POST", "/actuators/state", payload);

  if (respuesta.isEmpty()) {
    return false;
  }

  DynamicJsonDocument response(256);
  if (deserializeJson(response, respuesta) != DeserializationError::Ok) {
    return false;
  }

  return response["success"] == true;
}

// ═══════════════════════════════════════════════════════════
// ─── ENVÍO DE EVENTOS DE FALLA ────────────────────────────
// ═══════════════════════════════════════════════════════════

bool ServicioAPI::enviarEventoFalla(
    const String& origen,
    const String& mensaje,
    const String& nivel,
    const String& metadataJson) {
  if (!estaAutenticado()) {
    LOG_WARN("No autenticado para enviar evento - Intentando autenticar...");
    if (!autenticarDispositivo()) {
      LOG_ERROR("No se pudo autenticar para enviar evento crítico");
      return false;
    }
  }

  DynamicJsonDocument doc(1024);
  doc["device_id"] = device_id_;
  doc["tipo"] = "FALLA_SENSOR";
  doc["origen"] = origen;
  doc["mensaje"] = mensaje;
  doc["nivel"] = nivel;

  // Parsear metadata si viene como JSON string
  if (!metadataJson.isEmpty()) {
    DynamicJsonDocument metadataDoc(512);
    if (deserializeJson(metadataDoc, metadataJson) == DeserializationError::Ok) {
      doc["metadata"] = metadataDoc.as<JsonObject>();
    } else {
      // Si no es JSON válido, agregar como string
      doc["metadata"]["raw_data"] = metadataJson;
    }
  } else {
    doc["metadata"] = JsonObject();
  }

  String payload;
  serializeJson(doc, payload);

  LOG_WARN("Enviando evento crítico al backend: " + origen);

  String respuesta = realizarRequestConReintentos("POST", "/events", payload, 10000);

  if (respuesta.isEmpty()) {
    LOG_ERROR("Falló envío de evento crítico");
    return false;
  }

  DynamicJsonDocument response(256);
  if (deserializeJson(response, respuesta) != DeserializationError::Ok) {
    return false;
  }

  if (response["success"] == true) {
    LOG_DEBUG("✓ Evento crítico enviado");
    return true;
  }

  return false;
}

// ═══════════════════════════════════════════════════════════
// ─── FUNCIONES AUXILIARES ────────────────────────────────
// ═══════════════════════════════════════════════════════════

bool ServicioAPI::estaAutenticado() const {
  return !access_token_.isEmpty() && !tokenExpiro();
}

String ServicioAPI::obtenerEstado() const {
  if (access_token_.isEmpty()) {
    return "not_authenticated";
  }
  if (tokenExpiro()) {
    return "token_expired";
  }
  return "authenticated";
}

void ServicioAPI::reiniciar() {
  access_token_ = "";
  token_expiry_time_ = 0;
  LOG_DEBUG("ServicioAPI reiniciado");
}

bool ServicioAPI::tokenExpiro() const {
  if (token_expiry_time_ == 0) {
    return true;
  }
  return (unsigned long)millis() > token_expiry_time_;
}

bool ServicioAPI::refrescarToken() {
  LOG_DEBUG("Intentando refrescar token...");
  return autenticarDispositivo();
}

// ═══════════════════════════════════════════════════════════
// ─── IMPLEMENTACIÓN DE REQUESTS HTTP ──────────────────────
// ═══════════════════════════════════════════════════════════

String ServicioAPI::realizarRequestConReintentos(
    const String& metodo,
    const String& endpoint,
    const String& payload,
    int timeout_ms) {
  int backoff = BACKOFF_INICIAL_MS;

  for (int intento = 0; intento < MAX_REINTENTOS; intento++) {
    String url = backend_url_ + endpoint;
    String resultado = realizarRequest(metodo, url, payload, timeout_ms);

    if (!resultado.isEmpty()) {
      return resultado;
    }

    if (intento < MAX_REINTENTOS - 1) {
      LOG_WARN("Reintento " + String(intento + 1) + " en " + String(backoff) + "ms");
      vTaskDelay(pdMS_TO_TICKS(backoff));

      // Backoff exponencial: 500ms, 1000ms, 2000ms
      backoff = min(backoff * 2, 10000);
    }
  }

  return "";
}

String ServicioAPI::realizarRequest(
    const String& metodo,
    const String& url,
    const String& payload,
    int timeout_ms) {
  if (!WiFi.isConnected()) {
    LOG_WARN("WiFi desconectado");
    return "";
  }

  HTTPClient http;
  http.setConnectTimeout(timeout_ms);
  http.setTimeout(timeout_ms);

  bool success = false;
  int http_code = 0;
  String respuesta = "";

  try {
    if (metodo == "GET") {
      agregarHeaders(http, true);
      success = http.begin(url);
      if (success) {
        http_code = http.GET();
      }
    } else if (metodo == "POST") {
      agregarHeaders(http, true);
      success = http.begin(url);
      if (success) {
        http.addHeader("Content-Type", "application/json");
        http_code = http.POST((uint8_t*)payload.c_str(), payload.length());
      }
    }

    if (http_code == HTTP_CODE_OK || http_code == HTTP_CODE_CREATED) {
      respuesta = http.getString();
      LOG_DEBUG("✓ HTTP " + String(http_code) + " - " + metodo + " " + url);
    } else {
      LOG_WARN("HTTP " + String(http_code) + " - " + metodo + " " + url);
    }

  } catch (const std::exception& e) {
    LOG_ERROR("Excepción en request HTTP: " + String(e.what()));
  }

  http.end();
  return respuesta;
}

void ServicioAPI::agregarHeaders(HTTPClient& http, bool incluir_auth) {
  http.addHeader("User-Agent", "AVISENS-ESP32/1.0");
  http.addHeader("Accept", "application/json");

  if (incluir_auth && !access_token_.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + access_token_);
  }
}
