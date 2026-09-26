#ifndef NODO_H
#define NODO_H

#include "SensorDHT.h"
#include "SensorMQ135.h"
#include "SensorKY032.h"
#include "SensorUltrasonico.h"
#include "SensorPeso.h"
#include "GestorActuadores.h"
#include "ControlServo.h"
#include "Alimentador.h"
#include "Persiana.h"
#include "ConexionWiFi.h"
#include "ClienteMQTT.h"
#include "SistemaFSM.h"
#include "DetectorGradiente.h"

extern SensorDHT sensorDHT;
extern SensorMQ135 sensorMQ135;
extern SensorKY032 sensorKY032;
extern SensorUltrasonico sensorUltrasonico;
extern SensorPeso sensorPeso;

extern GestorActuadores gestorActuadores;
extern ControlServo controlServo;
extern Alimentador alimentador;
extern Persiana persiana;

extern ConexionWiFi conexionWiFi;
extern ClienteMQTT clienteMQTT;

extern SistemaFSM sistemaFSM;
extern DetectorGradiente detectorGradiente;

void iniciarPerifericos();

#endif
