#include "ConsolaSerie.h"
#include "Nodo.h"

void ConsolaSerie::begin()
{
  Serial.begin(BAUD_RATE);
  Serial.setTimeout(SERIAL_TIMEOUT_MS);
  delay(500);

  Serial.println("\n================================");
  Serial.println("GALPON INTELIGENTE - v9.0.0");
  Serial.println("FreeRTOS + MQTT");
  Serial.println("================================");
}

void ConsolaSerie::mostrarAyuda()
{
  Serial.println("Comandos disponibles:");
  Serial.println("  TARA   - Calibrar celda de carga");
  Serial.println("  REARME - Reiniciar FSM desde INIT");
  Serial.println("");
}

void ConsolaSerie::procesar()
{
  if (!Serial.available())
  {
    return;
  }

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "REARME" || cmd == "RESET")
  {
    sistemaFSM.rearmar();
  }
  else if (cmd == "TARA")
  {
    LOG_DEBUG("Comando de tara recibido");
    sensorPeso.setFactor(HX711_FACTOR_ESCALA);
    sensorPeso.tara();
    sistemaFSM.marcarCalibracionCompletada();
  }
}
