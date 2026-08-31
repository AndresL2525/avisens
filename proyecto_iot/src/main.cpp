#include <Arduino.h>
#include <esp_task_wdt.h>
#include <freertos/task.h>

#include "config.h"
#include "MovingAverage.h"
#include "SensorDHT.h"
#include "SensorMQ135.h"
#include "SensorKY032.h"
#include "SensorUltrasonico.h"
#include "SensorPeso.h"
#include "Actuador.h"
#include "GestorActuadores.h"
#include "ControlServo.h"
#include "Alimentador.h"
#include "Persiana.h"
#include "ConexionWiFi.h"

// ═══════════════════════════════════════════════════════════
// ─── INSTANCIAS GLOBALES (Memoria compartida Core 0) ──────
// ═══════════════════════════════════════════════════════════

EstadoSistema estadoSistema = EstadoSistema::INIT;

// Sensores
SensorDHT sensorDHT;
SensorMQ135 sensorMQ135;
SensorKY032 sensorKY032;
SensorUltrasonico sensorUltrasonico;
SensorPeso sensorPeso; // HX711 — Celda de carga

// Actuadores
GestorActuadores gestorActuadores;
ControlServo controlServo;
Alimentador alimentador;
Persiana persiana;

// WiFi (Core 1)
#if defined(WIFI_SSID) && defined(WIFI_PASS)
ConexionWiFi conexionWiFi(WIFI_SSID, WIFI_PASS);
#else
ConexionWiFi conexionWiFi("prueba", "123456789");
#endif

// ─── Variables de sincronización ─────────────────────────
unsigned long ultimaLecturaSensores = 0;
unsigned long ultimaActuacion = 0;

// ─── Counters para debug ──────────────────────────────────
uint32_t ciclosTarea = 0;
uint32_t erroresGlobales = 0;

// ═══════════════════════════════════════════════════════════
// ─── TAREA PRINCIPAL — Core 0 (FreeRTOS) ────────────────
// ═══════════════════════════════════════════════════════════

void tareaGalpon(void *pvParameters)
{
  esp_task_wdt_add(NULL);
  Serial.println("[FreeRTOS] Watchdog Timer registrado en Core 0.");

  static EstadoSistema ultimoEstadoImpreso = EstadoSistema::INIT;

  for (;;)
  {
    unsigned long ahora = millis();
    esp_task_wdt_reset();
    ciclosTarea++;

    // ─── Máquina de Estado Global ────────────────────────
    switch (estadoSistema)
    {
    case EstadoSistema::INIT:
      if (ciclosTarea > 10)
      {
        estadoSistema = EstadoSistema::CALIBRATION;
        Serial.println("\n[FSM Global] INIT → CALIBRATION");
      }
      break;

    case EstadoSistema::CALIBRATION:
      if (ciclosTarea > 100)
      {
        estadoSistema = EstadoSistema::MONITORING;
        Serial.println("[FSM Global] CALIBRATION → MONITORING");
      }
      break;

    case EstadoSistema::MONITORING:
      break;

    case EstadoSistema::ACTUATION:
      break;

    case EstadoSistema::ERROR:
      // Solo ejecuta y loguea una vez por transición para no inundar el puerto serial
      if (ultimoEstadoImpreso != EstadoSistema::ERROR)
      {
        Serial.println("\n❌ [FAIL-SAFE] Activado: K1-K3 OFF, K4 ON (Emergencia)");
        gestorActuadores.failSafe();
        controlServo.cerrarEmergencia();
        alimentador.detener();
        persiana.detener();
        ultimoEstadoImpreso = EstadoSistema::ERROR;
      }
      break;

    case EstadoSistema::SHUTDOWN:
      gestorActuadores.failSafe();
      alimentador.setHabilitado(false);
      persiana.setHabilitado(false);
      vTaskDelay(pdMS_TO_TICKS(1000));
      break;

    default:
      estadoSistema = EstadoSistema::MONITORING;
    }

    if (estadoSistema != EstadoSistema::ERROR)
    {
      ultimoEstadoImpreso = estadoSistema;
    }

    // ─── Lectura Periódica de Sensores ───────────────────
    if (ahora - ultimaLecturaSensores >= INTERVALO_SENSORES)
    {
      ultimaLecturaSensores = ahora;

      LecturaDHT lecturaDHT = sensorDHT.leer();
      LecturaMQ135 lecturaMQ135 = sensorMQ135.leer();
      LecturaKY032 lecturaKY032 = sensorKY032.leer();
      LecturaUltrasonico lecturaUltrasonico = sensorUltrasonico.leer();
      LecturaPeso lecturaPeso = sensorPeso.leer();

      float temperatura = lecturaDHT.valida ? lecturaDHT.temperatura : 0.0f;
      float humedad = lecturaDHT.valida ? lecturaDHT.humedad : 0.0f;
      int rawNH3 = lecturaMQ135.rawValue;

      // ─── Actualizar Actuadores y FSMs Locales ────────────
      if (estadoSistema == EstadoSistema::MONITORING)
      {
        gestorActuadores.actualizar(
            temperatura,
            humedad,
            rawNH3,
            lecturaUltrasonico.distancia,
            lecturaUltrasonico.estado,
            sensorDHT.enError(),
            sensorUltrasonico.enError());

        controlServo.actualizar(lecturaKY032.presencia);
        alimentador.actualizar();
        persiana.actualizar();
      }

      // ─── Evaluación de Fallos Críticos ───────────────────
      bool errorCritico = sensorDHT.enError() || sensorUltrasonico.enError();

      if (errorCritico)
      {
        erroresGlobales++;
        if (estadoSistema != EstadoSistema::ERROR)
        {
          estadoSistema = EstadoSistema::ERROR;
          LOG_ERROR("Fallo persistente en sensores — Transición a ERROR.");
        }
      }
      else if (estadoSistema == EstadoSistema::ERROR)
      {
        // Auto-recuperación cuando los sensores vuelven a responder
        estadoSistema = EstadoSistema::MONITORING;
        Serial.println("\n✓ [FSM Global] Sensores restablecidos: ERROR → MONITORING");
      }

      // ─── Alerta de Tolva ─────────────────────────────────
      if (lecturaPeso.valida && lecturaPeso.peso < UMBRAL_ALIMENTO_BAJO)
      {
        LOG_WARN("Alimento bajo en tolva (< 500g)");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ═══════════════════════════════════════════════════════════
// ─── TAREA WiFi — Core 1 (FreeRTOS) ───────────────────────
// ═══════════════════════════════════════════════════════════

void tareaWiFi(void *pvParameters)
{
  for (;;)
  {
    conexionWiFi.actualizar();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ═══════════════════════════════════════════════════════════
// ─── SETUP ───────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════

void setup()
{
  Serial.begin(BAUD_RATE);
  delay(500);

  Serial.println("\n================================");
  Serial.println("GALPÓN INTELIGENTE — v7.0.1");
  Serial.println("Arquitectura Modular + FreeRTOS");
  Serial.println("================================");

  // ─── Watchdog Timer ───────────────────────────────────
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  Serial.printf("[WDT] Configurado a %d segundos.\n", WDT_TIMEOUT_S);

  // ─── Inicialización de Módulos ────────────────────────
  Serial.println("\n[SETUP] Inicializando sensores...");
  sensorDHT.begin();
  sensorMQ135.begin();
  sensorKY032.begin();
  sensorUltrasonico.begin();
  sensorPeso.begin();

  Serial.println("[SETUP] Inicializando actuadores...");
  gestorActuadores.begin();
  controlServo.begin();
  alimentador.begin();
  persiana.begin();

  Serial.println("[SETUP] Iniciando enlace WiFi asíncrono...");
  conexionWiFi.comenzar();

  // ─── Tareas FreeRTOS en Cores Independientes ──────────
  Serial.println("[SETUP] Desplegando tarea de control en Core 0...");
  xTaskCreatePinnedToCore(
      tareaGalpon,
      "tareaGalpon",
      16384,
      NULL,
      2,
      NULL,
      0);

  Serial.println("[SETUP] Desplegando tarea WiFi en Core 1...");
  xTaskCreatePinnedToCore(
      tareaWiFi,
      "tareaWiFi",
      4096,
      NULL,
      1,
      NULL,
      1);

  // ─── Calibración Inicial HX711 ─────────────────────────
  Serial.println("\n[SETUP] Esperando calibración HX711 (15s timeout)...");
  Serial.println("Envía 'TARA' por el monitor Serial para calibrar.");

  unsigned long tiempoCalib = millis();
  bool taraEjecutada = false;

  while ((millis() - tiempoCalib) < 15000)
  {
    if (Serial.available())
    {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      cmd.toUpperCase();

      if (cmd == "TARA")
      {
        Serial.println("⏳ Ejecutando tara del HX711...");
        sensorPeso.setFactor(HX711_FACTOR_ESCALA);
        sensorPeso.tara();
        taraEjecutada = true;
        Serial.println("✓ Tara completada.");
        break;
      }
    }
    delay(100);
  }

  if (!taraEjecutada)
  {
    LOG_WARN("Calibración omitida — Usando factor por defecto.");
    sensorPeso.setFactor(HX711_FACTOR_ESCALA);
  }

  estadoSistema = EstadoSistema::MONITORING;
  Serial.println("[SETUP] Sistema listo y en modo MONITORING ✓\n");
}

// ═══════════════════════════════════════════════════════════
// ─── LOOP PRINCIPAL ───────────────────────────────────────
// ═══════════════════════════════════════════════════════════

void loop()
{
  vTaskDelay(pdMS_TO_TICKS(1000));
}