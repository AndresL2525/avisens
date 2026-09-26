#ifndef CLIENTE_MQTT_H
#define CLIENTE_MQTT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "config.h"

class ClienteMQTT
{
public:
  typedef void (*ManejadorComando)(const ComandoActuador &comando);

  ClienteMQTT(const char *host, uint16_t puerto, const char *deviceId);

  void begin(ManejadorComando manejador);
  void actualizar();
  bool estaConectado();

  bool publicarDHT(const SnapshotTelemetria &s);
  bool publicarGases(const SnapshotTelemetria &s);
  bool publicarPeso(const SnapshotTelemetria &s);
  bool publicarObstaculo(bool detectado);
  bool publicarNivelAgua(const SnapshotTelemetria &s);
  bool publicarDiagnostico(const SnapshotTelemetria &s);
  bool publicarEstadoActuadores(const EstadoActuadores &e);
  bool publicarEventoFalla(const EventoFalla &ev);

private:
  static ClienteMQTT *instancia_;
  static void callbackEstatico(char *topic, uint8_t *payload, unsigned int length);

  void procesarComando(const String &topic, const String &payload);
  bool conectar();
  String topico(const char *sufijo) const;
  bool publicar(const String &topic, const String &payload, bool retener = false);

  WiFiClient wifiClient_;
  PubSubClient mqtt_;
  String host_;
  uint16_t puerto_;
  String deviceId_;
  String baseTopic_;
  String clientId_;
  String topicoLwt_;
  ManejadorComando manejador_;
  unsigned long ultimoIntento_;
  uint32_t reconexiones_;
};

const char *nombreEstadoPersiana(EstadoPersiana estado);
const char *nombreEstadoPuerta(EstadoPuerta estado);
const char *nombreEstadoSistema(EstadoSistema estado);
const char *nombreEstadoUltrasonico(EstadoSensorUltrasonico estado);

#endif
