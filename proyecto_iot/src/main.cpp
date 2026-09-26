#include <Arduino.h>
#include <esp_task_wdt.h>
#include <freertos/task.h>

#include "config.h"
#include "Nodo.h"
#include "Mensajeria.h"
#include "ConsolaSerie.h"
#include "TareaControl.h"
#include "TareaRed.h"

void setup()
{
  ConsolaSerie::begin();
  esp_task_wdt_init(WDT_TIMEOUT_S, true);

  iniciarPerifericos();
  Mensajeria::begin();

  conexionWiFi.comenzar();
  clienteMQTT.begin(Mensajeria::encolarComando);

  xTaskCreatePinnedToCore(
      tareaControl,
      "tareaControl",
      STACK_TAREA_CONTROL,
      NULL,
      PRIORIDAD_TAREA_CONTROL,
      NULL,
      CORE_CONTROL);

  xTaskCreatePinnedToCore(
      tareaRed,
      "tareaRed",
      STACK_TAREA_RED,
      NULL,
      PRIORIDAD_TAREA_RED,
      NULL,
      CORE_RED);

  ConsolaSerie::mostrarAyuda();
}

void loop()
{
  vTaskDelay(pdMS_TO_TICKS(1000));
}
