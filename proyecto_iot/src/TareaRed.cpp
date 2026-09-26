#include "TareaRed.h"

#include <Arduino.h>
#include <freertos/task.h>

#include "config.h"
#include "Nodo.h"
#include "Mensajeria.h"

void tareaRed(void *pvParameters)
{
  unsigned long ultimaRapida = 0;
  unsigned long ultimaLenta = 0;
  unsigned long ultimoDiagnostico = 0;

  bool obstaculoPrevio = false;
  bool obstaculoInicializado = false;
  EstadoActuadores actuadoresPrevios = {};
  bool actuadoresInicializados = false;

  for (;;)
  {
    conexionWiFi.actualizar();
    clienteMQTT.actualizar();

    if (clienteMQTT.estaConectado())
    {
      unsigned long ahora = millis();
      SnapshotTelemetria snapshot;

      if (Mensajeria::leerTelemetria(snapshot))
      {
        if (ahora - ultimaRapida >= INTERVALO_TELEMETRIA_RAPIDA)
        {
          ultimaRapida = ahora;
          clienteMQTT.publicarDHT(snapshot);
          clienteMQTT.publicarGases(snapshot);
        }

        if (ahora - ultimaLenta >= INTERVALO_TELEMETRIA_LENTA)
        {
          ultimaLenta = ahora;
          clienteMQTT.publicarPeso(snapshot);
          clienteMQTT.publicarNivelAgua(snapshot);
        }

        if (ahora - ultimoDiagnostico >= INTERVALO_DIAGNOSTICO)
        {
          ultimoDiagnostico = ahora;
          clienteMQTT.publicarDiagnostico(snapshot);
        }

        if (!obstaculoInicializado || snapshot.obstaculo != obstaculoPrevio)
        {
          obstaculoPrevio = snapshot.obstaculo;
          obstaculoInicializado = true;
          clienteMQTT.publicarObstaculo(snapshot.obstaculo);
        }
      }

      EstadoActuadores actuadores;
      if (Mensajeria::leerActuadores(actuadores))
      {
        if (!actuadoresInicializados ||
            memcmp(&actuadores, &actuadoresPrevios, sizeof(EstadoActuadores)) != 0)
        {
          actuadoresPrevios = actuadores;
          actuadoresInicializados = true;
          clienteMQTT.publicarEstadoActuadores(actuadores);
        }
      }

      EventoFalla evento;
      while (Mensajeria::recibirEvento(evento))
      {
        clienteMQTT.publicarEventoFalla(evento);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(PERIODO_TAREA_RED_MS));
  }
}
