#ifndef MENSAJERIA_H
#define MENSAJERIA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "config.h"

class Mensajeria
{
public:
  static void begin();

  static void encolarComando(const ComandoActuador &comando);
  static bool recibirComando(ComandoActuador &comando);

  static void encolarEvento(const char *origen, const char *mensaje, const char *nivel,
                            uint32_t fallosAcumulados);
  static bool recibirEvento(EventoFalla &evento);

  static void publicarTelemetria(const SnapshotTelemetria &snapshot);
  static bool leerTelemetria(SnapshotTelemetria &snapshot);

  static void publicarActuadores(const EstadoActuadores &estado);
  static bool leerActuadores(EstadoActuadores &estado);

private:
  static QueueHandle_t colaComandos_;
  static QueueHandle_t colaEventos_;
  static QueueHandle_t buzonTelemetria_;
  static QueueHandle_t buzonActuadores_;
};

#endif
