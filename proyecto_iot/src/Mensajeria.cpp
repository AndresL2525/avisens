#include "Mensajeria.h"

QueueHandle_t Mensajeria::colaComandos_ = nullptr;
QueueHandle_t Mensajeria::colaEventos_ = nullptr;
QueueHandle_t Mensajeria::buzonTelemetria_ = nullptr;
QueueHandle_t Mensajeria::buzonActuadores_ = nullptr;

void Mensajeria::begin()
{
  colaComandos_ = xQueueCreate(LONGITUD_COLA_COMANDOS, sizeof(ComandoActuador));
  colaEventos_ = xQueueCreate(LONGITUD_COLA_EVENTOS, sizeof(EventoFalla));
  buzonTelemetria_ = xQueueCreate(LONGITUD_BUZON, sizeof(SnapshotTelemetria));
  buzonActuadores_ = xQueueCreate(LONGITUD_BUZON, sizeof(EstadoActuadores));
}

void Mensajeria::encolarComando(const ComandoActuador &comando)
{
  if (colaComandos_ != nullptr)
  {
    xQueueSend(colaComandos_, &comando, 0);
  }
}

bool Mensajeria::recibirComando(ComandoActuador &comando)
{
  return colaComandos_ != nullptr && xQueueReceive(colaComandos_, &comando, 0) == pdTRUE;
}

void Mensajeria::encolarEvento(const char *origen, const char *mensaje, const char *nivel,
                               uint32_t fallosAcumulados)
{
  if (colaEventos_ == nullptr)
  {
    return;
  }

  EventoFalla evento;
  strncpy(evento.origen, origen, sizeof(evento.origen) - 1);
  evento.origen[sizeof(evento.origen) - 1] = '\0';
  strncpy(evento.mensaje, mensaje, sizeof(evento.mensaje) - 1);
  evento.mensaje[sizeof(evento.mensaje) - 1] = '\0';
  strncpy(evento.nivel, nivel, sizeof(evento.nivel) - 1);
  evento.nivel[sizeof(evento.nivel) - 1] = '\0';
  evento.fallosAcumulados = fallosAcumulados;

  xQueueSend(colaEventos_, &evento, 0);
}

bool Mensajeria::recibirEvento(EventoFalla &evento)
{
  return colaEventos_ != nullptr && xQueueReceive(colaEventos_, &evento, 0) == pdTRUE;
}

void Mensajeria::publicarTelemetria(const SnapshotTelemetria &snapshot)
{
  if (buzonTelemetria_ != nullptr)
  {
    xQueueOverwrite(buzonTelemetria_, &snapshot);
  }
}

bool Mensajeria::leerTelemetria(SnapshotTelemetria &snapshot)
{
  return buzonTelemetria_ != nullptr && xQueuePeek(buzonTelemetria_, &snapshot, 0) == pdTRUE;
}

void Mensajeria::publicarActuadores(const EstadoActuadores &estado)
{
  if (buzonActuadores_ != nullptr)
  {
    xQueueOverwrite(buzonActuadores_, &estado);
  }
}

bool Mensajeria::leerActuadores(EstadoActuadores &estado)
{
  return buzonActuadores_ != nullptr && xQueuePeek(buzonActuadores_, &estado, 0) == pdTRUE;
}
