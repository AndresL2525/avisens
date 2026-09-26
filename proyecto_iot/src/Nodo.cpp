#include "Nodo.h"

SensorDHT sensorDHT;
SensorMQ135 sensorMQ135;
SensorKY032 sensorKY032;
SensorUltrasonico sensorUltrasonico;
SensorPeso sensorPeso;

GestorActuadores gestorActuadores;
ControlServo controlServo;
Alimentador alimentador;
Persiana persiana;

#if defined(WIFI_SSID) && defined(WIFI_PASS)
ConexionWiFi conexionWiFi(WIFI_SSID, WIFI_PASS);
#else
ConexionWiFi conexionWiFi(
    "prueba",     // Nombre de red
    "123456789"); // Contraseña
#endif

ClienteMQTT clienteMQTT(MQTT_BROKER_HOST, MQTT_BROKER_PORT, MQTT_DEVICE_ID);

SistemaFSM sistemaFSM;
DetectorGradiente detectorGradiente;

void iniciarPerifericos()
{
  sensorDHT.begin();
  sensorMQ135.begin();
  sensorKY032.begin();
  sensorUltrasonico.begin();
  sensorPeso.begin();

  gestorActuadores.begin();
  controlServo.begin();
  alimentador.begin();
  persiana.begin();

  sensorPeso.setFactor(HX711_FACTOR_ESCALA);
}
