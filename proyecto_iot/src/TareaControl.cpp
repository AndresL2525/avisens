#include "TareaControl.h"

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <freertos/task.h>

#include "config.h"
#include "Nodo.h"
#include "Mensajeria.h"
#include "ConsolaSerie.h"

static void aplicarComandosPendientes()
{
  ComandoActuador comando;
  while (Mensajeria::recibirComando(comando))
  {
    // En fail-safe ningún comando remoto puede volver a energizar un relé
    if (sistemaFSM.enFailSafe())
    {
      LOG_WARN("Comando remoto descartado: sistema en fail-safe");
      continue;
    }

    if (comando.modoManual)
    {
      gestorActuadores.establecerManual(comando.rele, comando.estado);
    }
    else
    {
      gestorActuadores.establecerAutomatico(comando.rele);
    }
  }
}

static void vigilarPresencia(bool trabado)
{
  static bool trabadoNotificado = false;

  if (trabado && !trabadoNotificado)
  {
    trabadoNotificado = true;
    Mensajeria::encolarEvento(
        "PresenciaTrabada",
        "KY-032 en LOW mas de 120 s, puerta inhabilitada",
        "advertencia",
        0);
    LOG_WARN("KY-032 trabado en LOW: puerta inhabilitada");
  }
  else if (!trabado && trabadoNotificado)
  {
    trabadoNotificado = false;
    Mensajeria::encolarEvento(
        "PresenciaTrabada",
        "KY-032 liberado, puerta habilitada",
        "info",
        0);
    LOG_WARN("KY-032 liberado: puerta habilitada");
  }
}

static void publicarSnapshot(const LecturaDHT &dht,
                             const LecturaMQ135 &gas,
                             const LecturaPeso &peso,
                             const LecturaKY032 &presencia,
                             const LecturaUltrasonico &agua)
{
  SnapshotTelemetria snapshot = {};
  LecturaDHT dhtVigente = dht.valida ? dht : sensorDHT.getUltimaLectura();
  snapshot.temperatura = dhtVigente.temperatura;
  snapshot.humedad = dhtVigente.humedad;
  snapshot.dhtOk = !sensorDHT.enError();
  snapshot.gasRaw = gas.rawValue;
  snapshot.gasVoltaje = gas.voltaje;
  LecturaPeso pesoVigente = peso.valida ? peso : sensorPeso.getUltimaLectura();
  snapshot.peso = pesoVigente.peso;
  snapshot.pesoOk = !sensorPeso.enError();
  snapshot.obstaculo = presencia.presencia;
  snapshot.distanciaAgua = agua.distancia;
  snapshot.estadoAgua = agua.estado;
  snapshot.bombaActiva = gestorActuadores.getK4();
  snapshot.fallosAcumulados = sistemaFSM.fallosAcumulados();
  snapshot.estadoSistema = sistemaFSM.estado();
  snapshot.uptimeMs = millis();

  Mensajeria::publicarTelemetria(snapshot);

  EstadoActuadores actuadores = {};
  actuadores.calefactor = gestorActuadores.getK1();
  actuadores.ventilador = gestorActuadores.getK2();
  actuadores.extractor = gestorActuadores.getK3();
  actuadores.bomba = gestorActuadores.getK4();
  actuadores.manualCalefactor = gestorActuadores.esManual(1);
  actuadores.manualVentilador = gestorActuadores.esManual(2);
  actuadores.manualExtractor = gestorActuadores.esManual(3);
  actuadores.manualBomba = gestorActuadores.esManual(4);
  actuadores.alimentadorActivo = (alimentador.getEstado() == EstadoAlimentador::ENCENDIDO);
  actuadores.alimentadorBloqueado = !alimentador.isHabilitado();
  actuadores.persiana = persiana.getEstado();
  actuadores.puerta = controlServo.getEstado();

  Mensajeria::publicarActuadores(actuadores);
}

void tareaControl(void *pvParameters)
{
  esp_task_wdt_add(NULL);

  unsigned long ultimaLecturaSensores = 0;

  for (;;)
  {
    unsigned long ahora = millis();
    esp_task_wdt_reset();

    ConsolaSerie::procesar();
    aplicarComandosPendientes();
    sistemaFSM.avanzar();

    LecturaKY032 lecturaKY032 = sensorKY032.leer();
    bool presenciaTrabada = sensorKY032.enTrabado();
    vigilarPresencia(presenciaTrabada);

    if (sistemaFSM.estado() == EstadoSistema::MONITORING)
    {
      // Con el sensor trabado la puerta no puede seguir a la presencia: se le
      // presenta el campo despejado para que cierre y quede inhabilitada.
      controlServo.actualizar(lecturaKY032.presencia && !presenciaTrabada);
    }

    if (ahora - ultimaLecturaSensores >= INTERVALO_SENSORES)
    {
      ultimaLecturaSensores = ahora;

      LecturaDHT lecturaDHT = sensorDHT.leer();
      LecturaMQ135 lecturaMQ135 = sensorMQ135.leer();
      LecturaUltrasonico lecturaUltrasonico = sensorUltrasonico.leer();
      LecturaPeso lecturaPeso = sensorPeso.leer();

      // Una lectura inválida no puede degradarse a 0.0: con TEMP_FRIO en 27 °C
      // eso encendería el calefactor. Se arrastra la última lectura buena y,
      // si nunca la hubo, se declara el sensor en error para forzar el fail-safe.
      LecturaDHT dhtVigente = lecturaDHT.valida ? lecturaDHT : sensorDHT.getUltimaLectura();
      float temperatura = dhtVigente.temperatura;
      float humedad = dhtVigente.humedad;
      bool dhtInutilizable = !dhtVigente.valida;

      if (lecturaDHT.valida)
      {
        detectorGradiente.evaluar(temperatura, ahora);
      }

      if (sistemaFSM.estado() == EstadoSistema::MONITORING)
      {
        gestorActuadores.actualizar(
            temperatura,
            humedad,
            lecturaMQ135.rawValue,
            lecturaUltrasonico.distancia,
            lecturaUltrasonico.estado,
            sensorDHT.enError() || dhtInutilizable,
            sensorUltrasonico.enError());

        alimentador.actualizar();
        persiana.actualizar();
      }

      sistemaFSM.evaluarSensoresCriticos(sensorDHT.enError() || sensorUltrasonico.enError());

      if (lecturaPeso.valida && lecturaPeso.peso < UMBRAL_ALIMENTO_BAJO)
      {
        LOG_WARN("Alimento bajo en tolva");
      }

      publicarSnapshot(lecturaDHT, lecturaMQ135, lecturaPeso, lecturaKY032, lecturaUltrasonico);
    }

    vTaskDelay(pdMS_TO_TICKS(PERIODO_TAREA_CONTROL_MS));
  }
}
