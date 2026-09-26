#include "ClienteMQTT.h"
#include <ArduinoJson.h>
#include "GestorActuadores.h"

ClienteMQTT *ClienteMQTT::instancia_ = nullptr;

static constexpr char SEGMENTO_ACTUADORES[] = "actuadores/";
static constexpr char SUFIJO_COMANDO[] = "/set";
static constexpr char PAYLOAD_LWT_OFFLINE[] =
    "{\"status\":\"OFFLINE\",\"causa\":\"DESCONEXION_INESPERADA\"}";

const char *nombreEstadoPersiana(EstadoPersiana estado)
{
  switch (estado)
  {
  case EstadoPersiana::QUIETA:
    return "QUIETA";
  case EstadoPersiana::ABRIENDO:
    return "ABRIENDO";
  case EstadoPersiana::PAUSA:
    return "PAUSA";
  case EstadoPersiana::CERRANDO:
    return "CERRANDO";
  }
  return "QUIETA";
}

const char *nombreEstadoPuerta(EstadoPuerta estado)
{
  switch (estado)
  {
  case EstadoPuerta::CERRADA:
    return "CERRADA";
  case EstadoPuerta::ABRIENDO:
    return "ABRIENDO";
  case EstadoPuerta::ABIERTA:
    return "ABIERTA";
  case EstadoPuerta::CERRANDO:
    return "CERRANDO";
  }
  return "CERRADA";
}

const char *nombreEstadoSistema(EstadoSistema estado)
{
  switch (estado)
  {
  case EstadoSistema::INIT:
    return "INIT";
  case EstadoSistema::CALIBRATION:
    return "CALIBRATION";
  case EstadoSistema::MONITORING:
    return "MONITORING";
  case EstadoSistema::ACTUATION:
    return "ACTUATION";
  case EstadoSistema::ERROR:
    return "ERROR";
  case EstadoSistema::SHUTDOWN:
    return "SHUTDOWN";
  }
  return "MONITORING";
}

const char *nombreEstadoUltrasonico(EstadoSensorUltrasonico estado)
{
  switch (estado)
  {
  case EstadoSensorUltrasonico::OK:
    return "OK";
  case EstadoSensorUltrasonico::TIMEOUT:
    return "TIMEOUT";
  case EstadoSensorUltrasonico::OUT_OF_RANGE:
    return "OUT_OF_RANGE";
  case EstadoSensorUltrasonico::ERROR:
    return "ERROR";
  }
  return "ERROR";
}

ClienteMQTT::ClienteMQTT(const char *host, uint16_t puerto, const char *deviceId)
    : mqtt_(wifiClient_),
      host_(host),
      puerto_(puerto),
      deviceId_(deviceId),
      manejador_(nullptr),
      ultimoIntento_(0),
      reconexiones_(0)
{
  baseTopic_ = "avisens/" + deviceId_ + "/";
  clientId_ = "esp32_" + deviceId_;
  topicoLwt_ = baseTopic_ + "status/lwt";
}

void ClienteMQTT::begin(ManejadorComando manejador)
{
  instancia_ = this;
  manejador_ = manejador;

  mqtt_.setServer(host_.c_str(), puerto_);
  mqtt_.setCallback(callbackEstatico);
  mqtt_.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt_.setBufferSize(MQTT_BUFFER_SIZE);

  LOG_DEBUG("ClienteMQTT " + clientId_ + " -> " + host_ + ":" + String(puerto_));
}

void ClienteMQTT::actualizar()
{
  if (!WiFi.isConnected())
  {
    return;
  }

  if (!mqtt_.connected())
  {
    unsigned long ahora = millis();
    if (ahora - ultimoIntento_ < MQTT_REINTENTO_MS)
    {
      return;
    }
    ultimoIntento_ = ahora;
    conectar();
    return;
  }

  mqtt_.loop();
}

bool ClienteMQTT::estaConectado()
{
  return mqtt_.connected();
}

bool ClienteMQTT::conectar()
{
  const char *usuario = strlen(MQTT_USUARIO) > 0 ? MQTT_USUARIO : nullptr;
  const char *clave = strlen(MQTT_CLAVE) > 0 ? MQTT_CLAVE : nullptr;

  bool conectado = mqtt_.connect(
      clientId_.c_str(),
      usuario,
      clave,
      topicoLwt_.c_str(),
      MQTT_QOS_COMANDOS,
      true,
      PAYLOAD_LWT_OFFLINE);

  if (!conectado)
  {
    LOG_WARN("Conexión MQTT rechazada, estado " + String(mqtt_.state()));
    return false;
  }

  reconexiones_++;

  String online = "{\"status\":\"ONLINE\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  publicar(topicoLwt_, online, true);

  String topicoComandos = baseTopic_ + SEGMENTO_ACTUADORES + "+" + SUFIJO_COMANDO;
  mqtt_.subscribe(topicoComandos.c_str(), MQTT_QOS_COMANDOS);

  LOG_DEBUG("✓ MQTT conectado, suscrito a " + topicoComandos);
  return true;
}

void ClienteMQTT::callbackEstatico(char *topic, uint8_t *payload, unsigned int length)
{
  if (instancia_ == nullptr)
  {
    return;
  }

  String cuerpo;
  cuerpo.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++)
  {
    cuerpo += static_cast<char>(payload[i]);
  }

  instancia_->procesarComando(String(topic), cuerpo);
}

void ClienteMQTT::procesarComando(const String &topic, const String &payload)
{
  int posSegmento = topic.indexOf(SEGMENTO_ACTUADORES);
  if (posSegmento < 0)
  {
    return;
  }

  int inicioNombre = posSegmento + strlen(SEGMENTO_ACTUADORES);
  int finNombre = topic.indexOf('/', inicioNombre);
  if (finNombre < 0)
  {
    return;
  }

  String actuador = topic.substring(inicioNombre, finNombre);

  StaticJsonDocument<JSON_CAPACIDAD_COMANDO> doc;
  if (deserializeJson(doc, payload) != DeserializationError::Ok)
  {
    LOG_ERROR("Comando con JSON inválido en " + actuador);
    return;
  }

  String modo = doc["modo"] | "AUTO";
  modo.toUpperCase();

  ComandoActuador comando;
  comando.rele = GestorActuadores::releDesdeNombre(actuador);
  comando.modoManual = (modo == "MANUAL");
  comando.estado = doc["estado"] | false;

  if (comando.rele == 0)
  {
    LOG_WARN("Actuador sin control remoto: " + actuador);
    return;
  }

  if (manejador_ != nullptr)
  {
    manejador_(comando);
  }
}

String ClienteMQTT::topico(const char *sufijo) const
{
  return baseTopic_ + sufijo;
}

bool ClienteMQTT::publicar(const String &topic, const String &payload, bool retener)
{
  if (!mqtt_.connected())
  {
    return false;
  }

  bool publicado = mqtt_.publish(topic.c_str(), payload.c_str(), retener);
  if (!publicado)
  {
    LOG_WARN("Fallo publicando en " + topic);
  }
  return publicado;
}

bool ClienteMQTT::publicarDHT(const SnapshotTelemetria &s)
{
  StaticJsonDocument<JSON_CAPACIDAD_TELEMETRIA> doc;
  doc["device_id"] = deviceId_;
  doc["uptime_ms"] = s.uptimeMs;
  doc["temperatura"] = s.temperatura;
  doc["humedad"] = s.humedad;
  doc["sensor_ok"] = s.dhtOk;

  String payload;
  serializeJson(doc, payload);
  return publicar(topico("telemetria/dht22"), payload);
}

bool ClienteMQTT::publicarGases(const SnapshotTelemetria &s)
{
  const char *nivel = "NORMAL";
  if (s.gasRaw >= NH3_ALTO)
  {
    nivel = "ALTO";
  }
  else if (s.gasRaw >= NH3_MODERADO)
  {
    nivel = "MODERADO";
  }

  StaticJsonDocument<JSON_CAPACIDAD_TELEMETRIA> doc;
  doc["device_id"] = deviceId_;
  doc["raw_adc"] = s.gasRaw;
  doc["voltaje"] = s.gasVoltaje;
  doc["nivel"] = nivel;
  doc["alerta_gas"] = (s.gasRaw >= NH3_ALTO);

  String payload;
  serializeJson(doc, payload);
  return publicar(topico("telemetria/gases"), payload);
}

bool ClienteMQTT::publicarPeso(const SnapshotTelemetria &s)
{
  StaticJsonDocument<JSON_CAPACIDAD_TELEMETRIA> doc;
  doc["device_id"] = deviceId_;
  doc["peso_gramos"] = s.peso;
  doc["tolva_vacia"] = (s.peso < UMBRAL_ALIMENTO_BAJO);
  doc["sensor_ok"] = s.pesoOk;

  String payload;
  serializeJson(doc, payload);
  return publicar(topico("telemetria/peso"), payload);
}

bool ClienteMQTT::publicarObstaculo(bool detectado)
{
  StaticJsonDocument<JSON_CAPACIDAD_TELEMETRIA> doc;
  doc["device_id"] = deviceId_;
  doc["detectado"] = detectado;
  doc["evento"] = detectado ? "INGRESO_DETECTADO" : "PASO_DESPEJADO";

  String payload;
  serializeJson(doc, payload);
  return publicar(topico("telemetria/obstaculo"), payload);
}

bool ClienteMQTT::publicarNivelAgua(const SnapshotTelemetria &s)
{
  StaticJsonDocument<JSON_CAPACIDAD_TELEMETRIA> doc;
  doc["device_id"] = deviceId_;
  doc["distancia_cm"] = s.distanciaAgua;
  doc["estado_sensor"] = nombreEstadoUltrasonico(s.estadoAgua);
  doc["bomba_activa"] = s.bombaActiva;

  String payload;
  serializeJson(doc, payload);
  return publicar(topico("telemetria/nivel_agua"), payload);
}

bool ClienteMQTT::publicarDiagnostico(const SnapshotTelemetria &s)
{
  StaticJsonDocument<JSON_CAPACIDAD_DIAGNOSTICO> doc;
  doc["device_id"] = deviceId_;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["uptime_segundos"] = s.uptimeMs / 1000UL;
  doc["modo_operativo"] = nombreEstadoSistema(s.estadoSistema);
  doc["fallos_acumulados"] = s.fallosAcumulados;
  doc["reconexiones_mqtt"] = reconexiones_;

  String payload;
  serializeJson(doc, payload);
  return publicar(topico("telemetria/diagnostico"), payload);
}

bool ClienteMQTT::publicarEstadoActuadores(const EstadoActuadores &e)
{
  StaticJsonDocument<JSON_CAPACIDAD_ESTADO> doc;
  doc["device_id"] = deviceId_;

  JsonObject calefactor = doc.createNestedObject("calefactor");
  calefactor["estado"] = e.calefactor;
  calefactor["modo"] = e.manualCalefactor ? "MANUAL" : "AUTO";

  JsonObject ventilador = doc.createNestedObject("ventilador");
  ventilador["estado"] = e.ventilador;
  ventilador["modo"] = e.manualVentilador ? "MANUAL" : "AUTO";

  JsonObject extractor = doc.createNestedObject("extractor");
  extractor["estado"] = e.extractor;
  extractor["modo"] = e.manualExtractor ? "MANUAL" : "AUTO";

  JsonObject bomba = doc.createNestedObject("bomba");
  bomba["estado"] = e.bomba;
  bomba["modo"] = e.manualBomba ? "MANUAL" : "AUTO";

  JsonObject alimentador = doc.createNestedObject("alimentador");
  alimentador["estado"] = e.alimentadorActivo;
  alimentador["modo"] = "AUTO";
  alimentador["bloqueado"] = e.alimentadorBloqueado;

  JsonObject persiana = doc.createNestedObject("persiana");
  persiana["estado_fsm"] = nombreEstadoPersiana(e.persiana);
  persiana["modo"] = "AUTO";

  JsonObject puerta = doc.createNestedObject("puerta");
  puerta["estado_fsm"] = nombreEstadoPuerta(e.puerta);
  puerta["modo"] = "AUTO";

  String payload;
  serializeJson(doc, payload);
  return publicar(topico("actuadores/estado"), payload, true);
}

bool ClienteMQTT::publicarEventoFalla(const EventoFalla &ev)
{
  StaticJsonDocument<JSON_CAPACIDAD_EVENTO> doc;
  doc["device_id"] = deviceId_;
  doc["tipo"] = "FALLA_SENSOR";
  doc["origen"] = ev.origen;
  doc["mensaje"] = ev.mensaje;
  doc["nivel"] = ev.nivel;
  doc["fallos_acumulados"] = ev.fallosAcumulados;

  String payload;
  serializeJson(doc, payload);
  return publicar(topico("telemetria/eventos"), payload);
}
