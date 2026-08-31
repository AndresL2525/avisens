#include "ConexionWiFi.h"
#include <esp_log.h>

static const char *TAG = "WiFi";

ConexionWiFi::ConexionWiFi(const char *ssid, const char *password)
    : ssid_(ssid),
      password_(password),
      hostname_("galponsmart"),
      conectado_(false),
      ultimoIntento_(0),
      intervaloReconexion_(INTERVALO_RECONEXION_MS) {}

void ConexionWiFi::comenzar()
{
  ESP_LOGI(TAG, "Iniciando conexión WiFi asíncrona...");

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname_.c_str());
  WiFi.begin(ssid_, password_);

  Serial.print("[WiFi] Conectando a SSID: ");
  Serial.println(ssid_);
}

void ConexionWiFi::actualizar()
{
  unsigned long ahora = millis();

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!conectado_)
    {
      conectado_ = true;
      Serial.print("✓ WiFi conectado. IP: ");
      Serial.println(WiFi.localIP());
    }
  }
  else
  {
    if (conectado_)
    {
      conectado_ = false;
      ESP_LOGW(TAG, "WiFi desconectado");
      Serial.println("⚠ WiFi desconectado");
    }

    // Reintentar reconexión periódica
    if (ahora - ultimoIntento_ >= intervaloReconexion_)
    {
      ultimoIntento_ = ahora;
      Serial.print("[WiFi] Reconectando... Estado: ");
      Serial.println(static_cast<int>(WiFi.status()));
      WiFi.reconnect();
    }
  }
}

bool ConexionWiFi::estaConectado() const
{
  return conectado_ && (WiFi.status() == WL_CONNECTED);
}

IPAddress ConexionWiFi::getIP() const
{
  return WiFi.localIP();
}

void ConexionWiFi::setHostname(const char *hostname)
{
  hostname_ = hostname;
  if (estaConectado())
  {
    WiFi.setHostname(hostname);
  }
}

void ConexionWiFi::desconectar()
{
  conectado_ = false;
  WiFi.disconnect(true);
  ESP_LOGI(TAG, "WiFi desconectado manualmente");
  Serial.println("[WiFi] Desconectado.");
}