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
// SensorPeso (futuro): GPIO opcionales

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

// ─── HX711 (Celda de Carga) ──────────────────────────────
#define HX711_DT 15                  // DATA pin (GPIO15)
#define HX711_SCK 16                 //
#define HX711_FACTOR_ESCALA 0.453592 // Gramos/unidad (ejemplo: 20kg)
#define UMBRAL_ALIMENTO_BAJO 500.0   // Gramos — Alerta

// ─── UMBRALES DE CONTROL ────────────────────────────────
#define TEMP_FRIO 27.0
#define TEMP_CALOR 32.0
#define HUM_EXTRACTORES 65.0
#define NH3_ALTO 1500
#define NH3_MODERADO 800
#define NIVEL_BOMBA_ON 6.0       // cm
#define NIVEL_BOMBA_OFF 3.0      // cm
#define MAX_FALLOS_SENSOR 3      // Reintentos antes de fail-safe
#define MAX_DISTANCIA_AGUA 400.0 // cm (rango HC-SR04)

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
#define WDT_TIMEOUT_S 10 // segundos

// ─── SERIAL ─────────────────────────────────────────────
#define BAUD_RATE 115200

// ═══════════════════════════════════════════════════════════
// ─── MÁQUINAS DE ESTADO (enum class) ──────────────────────
// ═══════════════════════════════════════════════════════════

enum class EstadoSistema : uint8_t
{
  INIT,        // Inicialización (lectura pins, setup básico)
  CALIBRATION, // Calibración (tara HX711 + ajustes)
  MONITORING,  // Monitoreo activo (lectura sensores)
  ACTUATION,   // Actuación en curso (control actuadores)
  ERROR,       // Error crítico — Fail-Safe activado
  SHUTDOWN     // Apagado controlado
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

// ═══════════════════════════════════════════════════════════
// ─── ESTRUCTURA DE DATOS PARA LECTURAS ────────────────────
// ═══════════════════════════════════════════════════════════

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

// ═══════════════════════════════════════════════════════════
// ─── CONSTANTES Y MACROS ÚTILES ───────────────────────────
// ═══════════════════════════════════════════════════════════

#define LOG_DEBUG(msg) Serial.println(msg)
#define LOG_WARN(msg) \
  Serial.print("⚠ "); \
  Serial.println(msg)
#define LOG_ERROR(msg) \
  Serial.print("❌ "); \
  Serial.println(msg)

#endif // CONFIG_H
