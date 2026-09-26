#ifndef CONEXION_WIFI_H
#define CONEXION_WIFI_H

#include <Arduino.h>
#include <WiFi.h>

class ConexionWiFi
{
public:
  ConexionWiFi(const char *ssid, const char *password);

  void comenzar();
  void actualizar();
  bool estaConectado() const;
  IPAddress getIP() const;
  String getHostname() const { return hostname_; }
  void setHostname(const char *hostname);
  void desconectar();

private:
  const char *ssid_;
  const char *password_;
  String hostname_;
  bool conectado_;
  unsigned long ultimoIntento_;
  unsigned long intervaloReconexion_;

  static constexpr unsigned long INTERVALO_RECONEXION_MS = 30000;
};

#endif
