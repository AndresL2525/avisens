#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ─── SENSORES ────────────────────────────────────────────
#define DHTPIN 4
#define DHTTYPE DHT22
#define MQ135_PIN 34
#define TRIG_AGUA 13
#define ECHO_AGUA 35
#define KY032_PIN 33

// ─── RELAY ───────────────────────────────────────────────
#define K1_PIN 32 // Calefacción/Bombillos
#define K2_PIN 25 // Ventilador
#define K3_PIN 27 // Extractor
#define K4_PIN 14 // Bomba agua

// ─── L293D CANAL A (Persiana) ────────────────────────────
#define EN1_PIN 5
#define IN1_PIN 18
#define IN2_PIN 19

// ─── L293D CANAL B (Tornillo sinfín) ─────────────────────
#define EN2_PIN 21
#define IN3_PIN 22
#define IN4_PIN 23

// ─── SERVO (Puerta) ──────────────────────────────────────
#define SERVO_PIN 2
#define SERVO_NEUTRO 93
#define SERVO_DURACION_GIRO 300
#define SERVO_TIEMPO_ABIERTA 2000
#define ANGULO_CERRADA 0
#define ANGULO_ABIERTA 90

// ─── HX711 (Celda de Carga) ──────────────────────────────
#define HX711_DT 15
#define HX711_SCK 16
#define HX711_FACTOR_ESCALA 0.453592 // Gramos/unidad
#define UMBRAL_ALIMENTO_BAJO 500.0   // Gramos
#define HX711_TIMEOUT_MS 150
#define HX711_MUESTRAS_TARA 10
#define HX711_ESPERA_MUESTRA_MS 100
#define HX711_BITS 24
#define HX711_SATURACION_POS 8388607L
#define HX711_SATURACION_NEG -8388608L
#define HX711_ADC_FONDO_ESCALA 16777216.0f

// ─── CONVERSIÓN ADC ──────────────────────────────────────
#define ADC_VREF 3.3f
#define ADC_MAX_CUENTAS 4095.0f

// ─── HC-SR04 ─────────────────────────────────────────────
#define TRIG_PULSO_US 10
#define ECHO_TIMEOUT_US 30000
#define VELOCIDAD_SONIDO_CM_US 0.0343f
#define MIN_DISTANCIA_AGUA 0.5 // cm, zona muerta del transductor

// ─── RANGOS VÁLIDOS DHT22 ────────────────────────────────
#define TEMP_MIN_VALIDA -10.0
#define TEMP_MAX_VALIDA 60.0
#define HUM_MIN_VALIDA 0.0
#define HUM_MAX_VALIDA 100.0

// ─── PWM DE MOTORES ──────────────────────────────────────
#define PWM_ALIMENTADOR 128 // 50 % de ciclo útil

// ─── UMBRALES DE CONTROL ────────────────────────────────
#define TEMP_FRIO 27.0
#define TEMP_CALOR 32.0
#define HUM_EXTRACTORES 65.0
#define NH3_ALTO 1500
#define NH3_MODERADO 800
#define NIVEL_BOMBA_ON 6.0       // cm
#define NIVEL_BOMBA_OFF 3.0      // cm
#define MAX_FALLOS_SENSOR 3
#define MAX_DISTANCIA_AGUA 400.0 // cm

// ─── KY-032 (Presencia) ──────────────────────────────────
#define KY032_DEBOUNCE_MS 50
#define KY032_TRABADO_MS 120000 // 2 min en LOW continuo

// ─── TEMPORIZACIÓN (ms) ──────────────────────────────────
#define INTERVALO_SENSORES 2000
#define INTERVALO_PERSIANA 300000 // 5 min
#define DURACION_PERSIANA 3000
#define PAUSA_PERSIANA 500
#define INTERVALO_ALIMENTO 300000 // 5 min
#define DURACION_ALIMENTO 300000  // 5 min

// ─── FILTRO MEDIA MÓVIL ─────────────────────────────────
#define MOVING_AVG_SIZE 10

// ─── WATCHDOG TIMER ─────────────────────────────────────
#define WDT_TIMEOUT_S 10

// ─── SERIAL ─────────────────────────────────────────────
#define BAUD_RATE 115200
#define SERIAL_TIMEOUT_MS 50

// ─── MQTT ───────────────────────────────────────────────
#ifndef MQTT_BROKER_HOST
#define MQTT_BROKER_HOST "192.168.1.100"
#endif
#ifndef MQTT_BROKER_PORT
#define MQTT_BROKER_PORT 1883
#endif
#ifndef MQTT_DEVICE_ID
#define MQTT_DEVICE_ID "galpon_01"
#endif
#ifndef MQTT_USUARIO
#define MQTT_USUARIO ""
#endif
#ifndef MQTT_CLAVE
#define MQTT_CLAVE ""
#endif

#define MQTT_KEEPALIVE_S 15
#define MQTT_BUFFER_SIZE 768
#define MQTT_REINTENTO_MS 5000
#define MQTT_QOS_COMANDOS 1

// ─── CADENCIAS DE PUBLICACIÓN (ms) ──────────────────────
#define INTERVALO_TELEMETRIA_RAPIDA 5000
#define INTERVALO_TELEMETRIA_LENTA 10000
#define INTERVALO_DIAGNOSTICO 60000

// ─── CAPACIDAD DE DOCUMENTOS JSON (bytes) ───────────────
#define JSON_CAPACIDAD_TELEMETRIA 192
#define JSON_CAPACIDAD_COMANDO 256
#define JSON_CAPACIDAD_DIAGNOSTICO 256
#define JSON_CAPACIDAD_EVENTO 384
#define JSON_CAPACIDAD_ESTADO 640

// ─── TAREAS FreeRTOS ────────────────────────────────────
#define CORE_CONTROL 1 // El driver WiFi esta clavado en el core 0 por el SDK
#define CORE_RED 0
#define STACK_TAREA_CONTROL 16384
#define STACK_TAREA_RED 8192
#define PRIORIDAD_TAREA_CONTROL 2
#define PRIORIDAD_TAREA_RED 1
#define PERIODO_TAREA_CONTROL_MS 10
#define PERIODO_TAREA_RED_MS 100

// ─── COLAS ENTRE CORES ──────────────────────────────────
#define LONGITUD_COLA_COMANDOS 8
#define LONGITUD_COLA_EVENTOS 4
#define LONGITUD_BUZON 1

// ─── MÁQUINA DE ESTADO GLOBAL ───────────────────────────
#define CICLOS_ARRANQUE_MIN 10
#define TIMEOUT_CALIBRACION_MS 15000

// ─── DETECTOR DE GRADIENTE TÉRMICO ──────────────────────
#define UMBRAL_GRADIENTE_TERMICO 10.0f // °C
#define VENTANA_GRADIENTE_MS 5000

// ─── MÁQUINAS DE ESTADO ─────────────────────────────────

enum class EstadoSistema : uint8_t
{
  INIT,
  CALIBRATION,
  MONITORING,
  ACTUATION,
  ERROR,
  SHUTDOWN
};

enum class EstadoPuerta : uint8_t
{
  CERRADA,
  ABRIENDO,
  ABIERTA,
  CERRANDO
};

enum class EstadoPersiana : uint8_t
{
  QUIETA,
  ABRIENDO,
  PAUSA,
  CERRANDO
};

enum class EstadoAlimentador : uint8_t
{
  APAGADO,
  ENCENDIDO
};

enum class EstadoSensorUltrasonico : uint8_t
{
  OK = 0,
  TIMEOUT = 1,
  OUT_OF_RANGE = 2,
  ERROR = 3
};

// ─── ESTRUCTURAS DE LECTURA ─────────────────────────────

struct LecturaDHT
{
  float temperatura;
  float humedad;
  bool valida;
  unsigned long timestamp;
};

struct LecturaMQ135
{
  int rawValue;
  float voltaje;
  bool valida;
  unsigned long timestamp;
};

struct LecturaUltrasonico
{
  float distancia; // cm
  EstadoSensorUltrasonico estado;
  unsigned long timestamp;
};

struct LecturaKY032
{
  bool presencia;
  unsigned long timestamp;
};

// ─── MENSAJES ENTRE CORES (colas FreeRTOS) ──────────────

struct ComandoActuador
{
  uint8_t rele;    // 1-4 para K1-K4; 0 si el actuador no es un relé
  bool modoManual;
  bool estado;
};

struct SnapshotTelemetria
{
  float temperatura;
  float humedad;
  bool dhtOk;
  int gasRaw;
  float gasVoltaje;
  float peso;
  bool pesoOk;
  bool obstaculo;
  float distanciaAgua;
  EstadoSensorUltrasonico estadoAgua;
  bool bombaActiva;
  uint32_t fallosAcumulados;
  EstadoSistema estadoSistema;
  unsigned long uptimeMs;
};

struct EstadoActuadores
{
  bool calefactor;
  bool ventilador;
  bool extractor;
  bool bomba;
  bool manualCalefactor;
  bool manualVentilador;
  bool manualExtractor;
  bool manualBomba;
  bool alimentadorActivo;
  bool alimentadorBloqueado;
  EstadoPersiana persiana;
  EstadoPuerta puerta;
};

struct EventoFalla
{
  char origen[32];
  char mensaje[96];
  char nivel[16];
  uint32_t fallosAcumulados;
};

// ─── MACROS DE TRAZA ────────────────────────────────────

#define LOG_DEBUG(msg) Serial.println(msg)
#define LOG_WARN(msg) \
  Serial.print("⚠ "); \
  Serial.println(msg)
#define LOG_ERROR(msg) \
  Serial.print("❌ "); \
  Serial.println(msg)

#endif // CONFIG_H
