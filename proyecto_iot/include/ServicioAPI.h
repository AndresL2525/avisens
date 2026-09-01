/**
 * =============================================================================
 * ServicioAPI.h — Cliente HTTP REST para comunicación con backend AVÍSENS
 * =============================================================================
 * 
 * Responsabilidades:
 * - Autenticación de dispositivo (JWT)
 * - Envío de lecturas de sensores (POST /sensors/readings)
 * - Consulta de comandos pendientes (GET /actuators/commands)
 * - Confirmación de comandos ejecutados (POST /actuators/commands/{id}/executed)
 * - Reporte de estado de actuadores (POST /actuators/state)
 * - Envío de eventos de falla (POST /events)
 * 
 * Características:
 * - No bloqueante (async-compatible con FreeRTOS)
 * - Reintentos con backoff exponencial
 * - Buffer en SPIFFS para persistencia en caso de caída de API
 * - JWT Bearer token management
 */

#ifndef SERVICIO_API_H
#define SERVICIO_API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "config.h"

/**
 * @struct LecturaSensores
 * @brief Estructura de datos para envío de telemetría
 */
struct LecturaSensores {
  String device_id;
  float temperatura;
  float humedad;
  int calidad_aire;
  int distancia_agua;
};

/**
 * @struct ComandoActuador
 * @brief Comando pendiente recibido del backend
 */
struct ComandoActuador {
  String command_id;
  String nombre_actuador;
  String accion;  // "ON", "OFF", "TOGGLE", etc.
  JsonObject metadata;
};

/**
 * @class ServicioAPI
 * @brief Cliente HTTP asíncrono para comunicación con backend AVÍSENS
 * 
 * Implementa patrón no-bloqueante con reintentos automáticos.
 * Mantiene token JWT en memoria y lo refresca si es necesario.
 */
class ServicioAPI {
 public:
  /**
   * @brief Constructor
   * @param backend_url URL base del backend (ej: "http://192.168.1.100:8000")
   * @param device_id ID del dispositivo (ej: "galpon_01")
   * @param device_secret Secret para autenticación inicial
   */
  ServicioAPI(const String& backend_url, const String& device_id, const String& device_secret);

  /**
   * @brief Autentica el dispositivo con el backend y obtiene JWT
   * 
   * Realiza POST /auth/device/login con credenciales.
   * Guarda el access_token en memoria.
   * 
   * @return true si la autenticación fue exitosa, false en caso contrario
   */
  bool autenticarDispositivo();

  /**
   * @brief Envía una lectura de sensores al backend
   * 
   * POST /sensors/readings con datos validados.
   * Incluye reintentos automáticos si falla.
   * 
   * @param datos Estructura LecturaSensores con datos del sensor
   * @return true si se envió exitosamente, false en caso contrario
   */
  bool enviarLecturas(const LecturaSensores& datos);

  /**
   * @brief Consulta comandos pendientes del backend
   * 
   * GET /actuators/commands?device_id={device_id}
   * Retorna lista de comandos sin ejecutar.
   * 
   * @return JSON array con comandos, vacío si no hay
   */
  String consultarComandosPendientes();

  /**
   * @brief Confirma que un comando fue ejecutado
   * 
   * POST /actuators/commands/{id}/executed
   * 
   * @param command_id ID del comando ejecutado
   * @param success true si se ejecutó sin errores
   * @param mensaje Mensaje de estado (opcional)
   * @return true si la confirmación se registró
   */
  bool confirmarComando(const String& command_id, bool success, const String& mensaje = "");

  /**
   * @brief Reporta el estado actual de un actuador
   * 
   * POST /actuators/state
   * 
   * @param nombre Nombre del actuador (ej: "K1_Calefaccion")
   * @param estado Estado booleano (true=ON, false=OFF)
   * @param modo Modo de operación (ej: "manual", "automático")
   * @return true si se reportó exitosamente
   */
  bool reportarEstadoActuador(const String& nombre, bool estado, const String& modo);

  /**
   * @brief Envía evento de falla crítica al backend
   * 
   * POST /events con tipo="FALLA_SENSOR"
   * Usado cuando el sistema detecta 3 fallos consecutivos en un sensor.
   * 
   * @param origen Origen del fallo (ej: "SensorDHT", "SensorUltrasonico")
   * @param mensaje Descripción del fallo
   * @param nivel Nivel de severidad ("info", "advertencia", "alerta", "critico")
   * @param metadataJson JSON string con metadata adicional
   * @return true si se envió exitosamente
   */
  bool enviarEventoFalla(
    const String& origen,
    const String& mensaje,
    const String& nivel,
    const String& metadataJson
  );

  /**
   * @brief Verifica si el dispositivo está autenticado y el token es válido
   * @return true si hay token válido en memoria
   */
  bool estaAutenticado() const;

  /**
   * @brief Obtiene el status actual de la conexión
   * @return Estado descriptivo: "not_connected", "connecting", "connected", "authenticated"
   */
  String obtenerEstado() const;

  /**
   * @brief Reinicia el servicio (limpia token, cierra conexiones)
   */
  void reiniciar();

 private:
  String backend_url_;
  String device_id_;
  String device_secret_;
  String access_token_;
  unsigned long token_expiry_time_;

  // Reintentos
  static constexpr int MAX_REINTENTOS = 3;
  static constexpr int BACKOFF_INICIAL_MS = 500;

  /**
   * @brief Realiza un request HTTP con reintentos automáticos
   * 
   * @param metodo GET, POST, etc.
   * @param endpoint Ruta relativa del backend
   * @param payload JSON payload (para POST), puede ser vacío
   * @param timeout_ms Timeout en milisegundos
   * @return Respuesta del servidor como String
   */
  String realizarRequestConReintentos(
    const String& metodo,
    const String& endpoint,
    const String& payload = "",
    int timeout_ms = 5000
  );

  /**
   * @brief Realiza un request HTTP individual sin reintentos
   * 
   * @param metodo GET, POST, etc.
   * @param url URL completa
   * @param payload JSON payload para POST
   * @param timeout_ms Timeout en milisegundos
   * @return Respuesta del servidor
   */
  String realizarRequest(
    const String& metodo,
    const String& url,
    const String& payload = "",
    int timeout_ms = 5000
  );

  /**
   * @brief Agrega headers estándar (Authorization, Content-Type, etc.)
   * 
   * @param http Cliente HTTPClient
   * @param incluir_auth true para agregar header Authorization con JWT
   */
  void agregarHeaders(HTTPClient& http, bool incluir_auth = true);

  /**
   * @brief Verifica si el token ha expirado
   * @return true si el token necesita refresh
   */
  bool tokenExpiro() const;

  /**
   * @brief Intenta refrescar el token antes de que expire
   * @return true si el refresh fue exitoso
   */
  bool refrescarToken();
};

#endif  // SERVICIO_API_H
