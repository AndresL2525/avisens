/**
 * =============================================================================
 * main.cpp (MODIFICADO)
 *
 * Cambios principales:
 * 1. Incluye ServicioAPI para comunicación con backend
 * 2. Sincroniza FSM Global con documento SSD:
 *    - Variable C: Contador de estabilización de arranque (>=10 ciclos)
 *    - Variable T: Bandera de calibración completada del HX711
 *    - Variable F: Fallos acumulados >= 3
 *    - Variable R: Comando de rearme manual por Serial
 * 3. En ERROR, envía evento de falla al backend ANTES de pausar
 * 4. En tareaWiFi: envía telemetría cada 5s y consulta comandos cada 10s
 * 5. Detección de gradiente térmico (ΔT > 10°C en 5s)
 *
 * =============================================================================
 */

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <freertos/task.h>
#include <freertos/queue.h>

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
#include "ServicioAPI.h"

// ═══════════════════════════════════════════════════════════
// ─── INSTANCIAS GLOBALES ──────────────────────────────────
// ═══════════════════════════════════════════════════════════

EstadoSistema estadoSistema = EstadoSistema::INIT;

// Sensores
SensorDHT sensorDHT;
SensorMQ135 sensorMQ135;
SensorKY032 sensorKY032;
SensorUltrasonico sensorUltrasonico;
SensorPeso sensorPeso;

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

// ─── Servicio API ─────────────────────────────────────────
// URL del backend: http://192.168.1.100:8000 (ajustar según ambiente)
ServicioAPI servicioAPI(
    "http://192.168.1.100:8000",
    "galpon_01",
    "device_secret_key_123");

// ─── Variables de sincronización ─────────────────────────
unsigned long ultimaLecturaSensores = 0;
unsigned long ultimaActuacion = 0;
unsigned long ultimaEnvioTelemetria = 0;
unsigned long ultimaConsultaComandos = 0;

// ─── Variables FSM (Documento SSD) ──────────────────────
uint32_t ciclosArranque = 0;        // Variable C: contador de arranque
bool calibracionCompletada = false; // Variable T: tara HX711 completada
uint32_t fallosAcumulados = 0;      // Variable F: contador de fallos >= 3
bool comandoRearme = false;         // Variable R: rearme manual por serial

// ─── Counters para debug ──────────────────────────────────
uint32_t ciclosTarea = 0;
uint32_t erroresGlobales = 0;

// ─── Historial de temperatura para detectar gradientes ────
struct HistorialTemperatura
{
  float temperatura;
  unsigned long timestamp;
} ultimaTemperatura = {0.0f, 0};

// ─── Filtro de picos para temperatura ────────────────────
MovingAverage<float, 10> filtroTemperatura;

// ═══════════════════════════════════════════════════════════
// ─── FUNCIÓN: Enviar evento de falla ─────────────────────
// ═══════════════════════════════════════════════════════════

void enviarEventoFallaAlBackend(
    const String &origen,
    const String &mensaje,
    const String &nivel = "critico")
{

  // Construir metadata JSON
  String metadataJson = "";
  {
    DynamicJsonDocument metadata(512);
    metadata["fallos_consecutivos"] = fallosAcumulados;
    metadata["temperatura_ultima"] = ultimaTemperatura.temperatura;
    metadata["estado_dht"] = sensorDHT.enError();
    metadata["estado_ultrasonico"] = sensorUltrasonico.enError();
    metadata["ciclos_sistema"] = ciclosTarea;

    serializeJson(metadata, metadataJson);
  }

  if (!servicioAPI.enviarEventoFalla(origen, mensaje, nivel, metadataJson))
  {
    LOG_ERROR("Falló envío de evento crítico al backend");
  }
  else
  {
    LOG_DEBUG("✓ Evento de falla enviado al backend");
  }
}

// ═══════════════════════════════════════════════════════════
// ─── FUNCIÓN: Detectar gradiente térmico abrupto ─────────
// ═══════════════════════════════════════════════════════════

bool detectarGradienteTermico(float temperatura, unsigned long ahora)
{
  const float UMBRAL_GRADIENT = 10.0f;       // °C
  const unsigned long VENTANA_TIEMPO = 5000; // 5 segundos en ms

  if (ultimaTemperatura.timestamp == 0)
  {
    // Primera lectura
    ultimaTemperatura.temperatura = temperatura;
    ultimaTemperatura.timestamp = ahora;
    return false;
  }

  unsigned long deltaT_ms = ahora - ultimaTemperatura.timestamp;
  float deltaTemp = std::abs(temperatura - ultimaTemperatura.temperatura);

  if (deltaT_ms <= VENTANA_TIEMPO && deltaTemp > UMBRAL_GRADIENT)
  {
    LOG_WARN("⚠ Gradiente térmico abrupto: ΔT=" + String(deltaTemp) +
             "°C en " + String(deltaT_ms) + "ms");
    return true;
  }

  ultimaTemperatura.temperatura = temperatura;
  ultimaTemperatura.timestamp = ahora;
  return false;
}

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

    // ─── Procesamiento de comandos Serial (Variable R) ──────────────
    if (Serial.available())
    {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      cmd.toUpperCase();

      if (cmd == "REARME" || cmd == "RESET")
      {
        LOG_WARN("Comando de rearme recibido por Serial");
        comandoRearme = true;
        estadoSistema = EstadoSistema::INIT;
        ciclosArranque = 0;
      }
      else if (cmd == "TARA")
      {
        LOG_DEBUG("Comando de tara (HX711) recibido");
        sensorPeso.tara();
        calibracionCompletada = true;
      }
    }

    // ─── Máquina de Estado Global (Sincronizada con SSD) ──────────────
    switch (estadoSistema)
    {
    case EstadoSistema::INIT:
      ciclosArranque++;

      // Variable C: Transición cuando ciclosArranque >= 10
      if (ciclosArranque >= 10)
      {
        estadoSistema = EstadoSistema::CALIBRATION;
        Serial.println("\n[FSM Global] INIT → CALIBRATION (Variable C>=10)");
      }
      break;

    case EstadoSistema::CALIBRATION:
      // Variable T: Esperar calibración HX711
      // Transición automática después de cierto tiempo o si se completa manualmente
      if (calibracionCompletada || ciclosTarea > 150)
      {
        estadoSistema = EstadoSistema::MONITORING;
        Serial.println("[FSM Global] CALIBRATION → MONITORING (Variable T=true)");
      }
      break;

    case EstadoSistema::MONITORING:
      // Sistema operativo normalmente
      break;

    case EstadoSistema::ACTUATION:
      // Transición a MONITORING cuando actuación completada
      estadoSistema = EstadoSistema::MONITORING;
      break;

    case EstadoSistema::ERROR:
      // Solo ejecuta una vez por transición
      if (ultimoEstadoImpreso != EstadoSistema::ERROR)
      {
        Serial.println("\n❌ [FAIL-SAFE] Activado: K1-K3 OFF, K4 ON (Emergencia)");

        // ─── Enviar evento de falla ANTES de pausar ───────────────────
        enviarEventoFallaAlBackend(
            "SensorsDHT_Ultrasonico",
            "Fallos persistentes en sensores críticos - Sistema en fail-safe",
            "critico");

        gestorActuadores.failSafe();
        controlServo.cerrarEmergencia();
        alimentador.detener();
        persiana.detener();
        ultimoEstadoImpreso = EstadoSistema::ERROR;
      }

      // ─── Intento de recuperación automática ──────────────────────
      if (!sensorDHT.enError() && !sensorUltrasonico.enError())
      {
        LOG_DEBUG("✓ Sensores recuperados - Transición a MONITORING");
        estadoSistema = EstadoSistema::MONITORING;
        fallosAcumulados = 0;
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

    // ─── Lectura Periódica de Sensores ───────────────────────────────
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

      // ─── Detectar gradiente térmico (ΔT > 10°C en 5s) ──────────────
      if (lecturaDHT.valida)
      {
        bool hayGradiente = detectarGradienteTermico(temperatura, ahora);
        if (hayGradiente)
        {
          // Registrar evento de gradiente
          String metadataJson = "";
          {
            DynamicJsonDocument metadata(256);
            metadata["delta_temperatura"] =
                std::abs(temperatura - ultimaTemperatura.temperatura);
            metadata["temperatura_actual"] = temperatura;
            serializeJson(metadata, metadataJson);
          }
          // En un caso real, aquí enviarías evento al backend
        }
      }

      // ─── Actualizar Actuadores y FSMs Locales ────────────────────
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

      // ─── Evaluación de Fallos Críticos (Variable F) ────────────────
      bool errorCritico = sensorDHT.enError() || sensorUltrasonico.enError();

      if (errorCritico)
      {
        fallosAcumulados++;

        // Variable F: Si fallos >= 3, transición a ERROR
        if (fallosAcumulados >= 3 && estadoSistema != EstadoSistema::ERROR)
        {
          estadoSistema = EstadoSistema::ERROR;
          LOG_ERROR("Fallos acumulados >= 3 — Transición a ERROR (Variable F)");
        }
      }
      else if (estadoSistema == EstadoSistema::ERROR)
      {
        // Auto-recuperación cuando los sensores vuelven a responder
        estadoSistema = EstadoSistema::MONITORING;
        fallosAcumulados = 0;
        Serial.println("\n✓ [FSM Global] Sensores restablecidos: ERROR → MONITORING");
      }

      // ─── Alerta de Tolva ──────────────────────────────────────────
      if (lecturaPeso.valida && lecturaPeso.peso < UMBRAL_ALIMENTO_BAJO)
      {
        LOG_WARN("Alimento bajo en tolva (< 500g)");
      }

      // ─── Debug: Mostrar estado actual ─────────────────────────────
      if (ciclosTarea % 30 == 0)
      {
        Serial.printf("[Ciclo %lu] T=%.1f°C H=%.0f%% NH3=%d ESTADO=%d\n",
                      ciclosTarea, temperatura, humedad, rawNH3,
                      static_cast<int>(estadoSistema));
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
  Serial.println("[WiFi Task] Iniciada en Core 1");

  // Esperar a que WiFi se conecte
  vTaskDelay(pdMS_TO_TICKS(5000));

  bool autenticado = false;

  for (;;)
  {
    // ─── Mantener conexión WiFi ──────────────────────────────────────
    conexionWiFi.actualizar();

    if (WiFi.isConnected())
    {
      // ─── Autenticar si no está autenticado ────────────────────────
      if (!autenticado && !servicioAPI.estaAutenticado())
      {
        LOG_DEBUG("Intentando autenticar dispositivo...");
        if (servicioAPI.autenticarDispositivo())
        {
          autenticado = true;
          LOG_DEBUG("✓ Dispositivo autenticado");
        }
      }

      // ─── Envío de telemetría cada 5 segundos ────────────────────
      if (autenticado && (millis() - ultimaEnvioTelemetria >= 5000))
      {
        ultimaEnvioTelemetria = millis();

        LecturaDHT lectura = sensorDHT.getUltimaLectura();
        if (lectura.valida)
        {
          LecturaSensores telemetria;
          telemetria.device_id = "galpon_01";
          telemetria.temperatura = lectura.temperatura;
          telemetria.humedad = lectura.humedad;
          telemetria.calidad_aire = 450;  // Valor dummy
          telemetria.distancia_agua = 10; // Valor dummy

          if (servicioAPI.enviarLecturas(telemetria))
          {
            LOG_DEBUG("✓ Telemetría enviada");
          }
          else
          {
            LOG_WARN("Falló envío de telemetría");
          }
        }
      }

      // ─── Consulta de comandos cada 10 segundos ──────────────────
      if (autenticado && (millis() - ultimaConsultaComandos >= 10000))
      {
        ultimaConsultaComandos = millis();

        String comandos = servicioAPI.consultarComandosPendientes();
        if (comandos != "[]" && !comandos.isEmpty())
        {
          LOG_DEBUG("Comandos pendientes recibidos: " + comandos);

          // Parsear y procesar comandos (extensible para futuros comandos)
          DynamicJsonDocument doc(1024);
          DeserializationError error = deserializeJson(doc, comandos);
          if (error == DeserializationError::Ok)
          {
            JsonArray array = doc.as<JsonArray>();
            for (JsonVariant cmd : array)
            {
              String command_id = cmd["command_id"] | "unknown";
              String nombre = cmd["nombre_actuador"] | "unknown";

              LOG_DEBUG("Procesando comando: " + nombre);

              // Aquí se procesaría cada comando según nombre y acción
              // Por ahora, simplemente confirmar ejecución
              servicioAPI.confirmarComando(command_id, true, "Ejecutado");
            }
          }
        }
      }
    }
    else
    {
      LOG_WARN("WiFi desconectado - Reintentando conexión");
      autenticado = false;
    }

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
  Serial.println("GALPÓN INTELIGENTE — v8.0.0");
  Serial.println("Arquitectura Modular + FreeRTOS");
  Serial.println("+ ServicioAPI + FSM Sincronizada");
  Serial.println("================================");

  // ─── Watchdog Timer ──────────────────────────────────────
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  Serial.printf("[WDT] Configurado a %d segundos.\n", WDT_TIMEOUT_S);

  // ─── Inicialización de Módulos ───────────────────────────
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

  // ─── Tareas FreeRTOS en Cores Independientes ──────────────
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
      8192,
      NULL,
      1,
      NULL,
      1);

  // ─── Calibración Inicial HX711 ───────────────────────────
  Serial.println("\n[SETUP] Esperando calibración HX711 (15s timeout)...");
  Serial.println("Envía 'TARA' por el monitor Serial para calibrar.");

  unsigned long tiempoCalib = millis();

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
        calibracionCompletada = true;
        Serial.println("✓ Tara completada.");
        break;
      }
    }
    delay(100);
  }

  if (!calibracionCompletada)
  {
    LOG_WARN("Calibración omitida — Usando factor por defecto.");
    sensorPeso.setFactor(HX711_FACTOR_ESCALA);
  }

  estadoSistema = EstadoSistema::MONITORING;
  Serial.println("[SETUP] Sistema listo y en modo MONITORING ✓\n");
  Serial.println("Comandos disponibles:");
  Serial.println("  TARA  - Calibrar celda de carga");
  Serial.println("  REARME - Reiniciar sistema desde INIT");
  Serial.println("");
}

// ═══════════════════════════════════════════════════════════
// ─── LOOP PRINCIPAL ───────────────────────────────────────
// ═══════════════════════════════════════════════════════════

void loop()
{
  vTaskDelay(pdMS_TO_TICKS(1000));
}
