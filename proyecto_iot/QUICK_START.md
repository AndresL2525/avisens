# ⚡ Guía Rápida — Galpón Modular

## 1️⃣ Instalación (3 min)

### Opción A: CLI (Recomendado)
```bash
# Instalar PlatformIO
pip install platformio

# Navegar al proyecto
cd galpon_modular

# Compilar
pio run -e esp32

# Cargar
pio run -e esp32 -t upload

# Monitor
pio device monitor -e esp32
```

### Opción B: VS Code
1. Instalar extensión **PlatformIO IDE**
2. Abrir carpeta `galpon_modular`
3. Botón **Build** → **Upload**
4. Botón **Serial Monitor**

---

## 2️⃣ Configuración Inicial (5 min)

### Editar `include/config.h`

**Cambiar pines** (si usas hardware diferente):
```cpp
#define DHTPIN        4
#define MQ135_PIN     34
#define TRIG_AGUA     13
#define ECHO_AGUA     35
// ... etc
```

**Cambiar umbrales** (según tu galpón):
```cpp
#define TEMP_FRIO      27.0    // Activa calefacción
#define TEMP_CALOR     32.0    // Activa ventilación
#define HUM_EXTRACTORES 65.0   // % Humedad crítica
#define NH3_ALTO       1500    // Nivel gases crítico
```

**Cambiar tiempos** (en milisegundos):
```cpp
#define INTERVALO_SENSORES 2000    // Leer cada 2 seg
#define INTERVALO_PERSIANA 300000  // Abrir persiana cada 5 min
#define INTERVALO_ALIMENTO 300000  // Dar alimento cada 5 min
```

### Editar `src/main.cpp` — WiFi

Línea ~48:
```cpp
ConexionWiFi conexionWiFi("tu_ssid", "tu_password");
```

---

## 3️⃣ Estructura de Clases

### 🔌 Sensores (Lectura)

```cpp
// DHT22 (Temp + Humedad)
sensorDHT.leer();           // → LecturaDHT
sensorDHT.getUltimaLectura();
sensorDHT.enError();        // true si falla

// MQ135 (Gases con filtro)
sensorMQ135.leer();         // → LecturaMQ135 (filtrada)
sensorMQ135.getNivelGas(); // → "NORMAL"/"MODERADO"/"ALTO"

// KY032 (Presencia)
sensorKY032.leer();         // → LecturaKY032
sensorKY032.hayPresencia(); // true si LOW

// HC-SR04 (Nivel agua con filtro)
sensorUltrasonico.leer();        // → LecturaUltrasonico
sensorUltrasonico.enError();     // true si timeout x3
```

### ⚙️ Actuadores (Escritura)

```cpp
// Relés (K1-K4)
gestorActuadores.actualizar(temp, hum, rawNH3, dist, estado_sensor, error_dht, error_ultra);
gestorActuadores.getK1();  // true/false

// Servo (Puerta)
controlServo.actualizar(hayPresencia);  // FSM automática
controlServo.getEstado();               // EstadoPuerta::ABIERTA

// Motor alimento
alimentador.actualizar();               // FSM automática
alimentador.setHabilitado(false);       // Deshabilitar temporalmente

// Persiana
persiana.actualizar();                  // FSM automática
persiana.abrirManual();                 // Control manual
```

---

## 4️⃣ Máquinas de Estado (FSM)

Cada componente maneja su propio estado sin bloqueos:

### ✅ Puerta (ControlServo)
```
CERRADA --[presencia]--> ABRIENDO --[300ms]--> ABIERTA --[2s]--> CERRANDO --[300ms]--> CERRADA
```

### ✅ Persiana (Persiana)
```
QUIETA --[5min]--> ABRIENDO --[3s]--> PAUSA --[500ms]--> CERRANDO --[3s]--> QUIETA
```

### ✅ Alimentador (Alimentador)
```
APAGADO --[5min]--> ENCENDIDO --[5min]--> APAGADO
```

### ✅ Sistema Global (main.cpp)
```
INIT --[1-2 ciclos]--> MONITORING --> [entra ERROR si sensor falla]
                       ↓
                    [Fail-Safe]
```

---

## 5️⃣ Logging y Debug

### Habilitar logs
En `config.h`:
```cpp
#define LOG_DEBUG(msg)  Serial.println(msg)
#define LOG_WARN(msg)   { Serial.print("⚠ "); Serial.println(msg); }
#define LOG_ERROR(msg)  { Serial.print("❌ "); Serial.println(msg); }
```

### Ver en Serial Monitor
```
pio device monitor -e esp32 --echo --eol LF
```

### Ejemplo de output esperado
```
================================
GALPÓN INTELIGENTE — v7.0
Arquitectura Modular + FreeRTOS
================================

[SETUP] Inicializando sensores...
SensorDHT inicializado
SensorMQ135 inicializado
...
[SETUP] Inicialización completada ✓
================================

================================
Temp: 28.5°C | Hum: 72.3%
NH3: 850 [NORMAL]
Agua: 4.2 cm
--- ACTUADORES ---
K1 (Calef): off
K2 (Ventil): ON
K3 (Extract): ON
K4 (Bomba): off
================================
```

---

## 6️⃣ Extensiones Personalizadas

### Agregar un nuevo sensor

1. **Crear `include/MiSensor.h`**:
```cpp
#ifndef MI_SENSOR_H
#define MI_SENSOR_H
#include "config.h"

class MiSensor {
 public:
  MiSensor();
  void begin();
  float leer();
  // ...
};
#endif
```

2. **Crear `src/MiSensor.cpp`**:
```cpp
#include "MiSensor.h"

MiSensor::MiSensor() { }

void MiSensor::begin() {
  pinMode(MI_SENSOR_PIN, INPUT);
}

float MiSensor::leer() {
  return analogRead(MI_SENSOR_PIN) * 0.1f;
}
```

3. **Incluir en `main.cpp`**:
```cpp
#include "MiSensor.h"

MiSensor miSensor;

void setup() {
  // ...
  miSensor.begin();
}

void tareaGalpon(...) {
  // ...
  float lectura = miSensor.leer();
}
```

### Agregar lógica de control personalizada

Editar `GestorActuadores::actualizar()` en `src/GestorActuadores.cpp`:

```cpp
void GestorActuadores::actualizar(...) {
  // ... código existente ...

  // NUEVA LÓGICA PERSONALIZADA
  bool condicionNueva = (temperatura > 35.0) && (humedad < 40.0);
  
  // Aplicar acción
  if (condicionNueva) {
    k2_.activar();  // Activar ventilador extra
  }

  // ... resto del código ...
}
```

---

## 7️⃣ Troubleshooting

| Problema | Solución |
|----------|----------|
| **Error de compilación** | `pio run -e esp32 -t clean` luego compilar |
| **Puerto USB no encontrado** | `pio device list` para listar puertos |
| **Monitor sin output** | Verificar baud rate: 115200 |
| **Sensor DHT falla** | Revisar pines, cable de datos, pull-up |
| **HC-SR04 timeout** | Verificar conexión TRIG/ECHO, distancia < 4m |
| **Servo no se mueve** | Ajustar valores `SERVO_NEUTRO`, `SERVO_DURACION_GIRO` |
| **Sistema reinicia** | Watchdog timeout → aumentar `WDT_TIMEOUT_S` |

---

## 8️⃣ Flujo de Ejecución

```
Core 1 (WiFi)          Core 0 (Sensores/Actuadores)
────────────────      ─────────────────────────────
loop() → idle         tareaGalpon():
  │                     │
  └─ tareaWiFi()        ├─ Lee DHT22
     │                  ├─ Lee MQ135 (filtro)
     ├─ Conecta WiFi    ├─ Lee KY032
     ├─ MQTT (futuro)   ├─ Lee HC-SR04 (filtro)
     └─ OTA (futuro)    ├─ Actualiza GestorActuadores
                        ├─ Actualiza ControlServo (FSM)
                        ├─ Actualiza Alimentador (FSM)
                        ├─ Actualiza Persiana (FSM)
                        ├─ Reset Watchdog Timer
                        └─ vTaskDelay(10ms)
```

---

## ❓ Preguntas Frecuentes

**P: ¿Puedo cambiar los tiempos sin recompilar?**  
R: Actualmente no (compilados). Futuro: guardar en EEPROM/SPIFFS.

**P: ¿Qué pasa si falla el WiFi?**  
R: Core 0 sigue funcionando. WiFi se reconecta automáticamente.

**P: ¿Cómo monitoreo desde fuera?**  
R: Futuro: MQTT o API REST (v8.0).

**P: ¿Máximo número de sensores?**  
R: Limitado por pines GPIO (34 en ESP32). Usar multiplexores para más.

**P: ¿Stack size suficiente?**  
R: Asignado 16KB a tareaGalpon. Si crece, aumentar en main.cpp:165.

---

## 📚 Referencias

- **ESP32 Pinout**: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
- **FreeRTOS**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html
- **PlatformIO Docs**: https://docs.platformio.org/en/latest/
- **DHT22**: https://learn.adafruit.com/dht/overview
- **HC-SR04**: https://randomnerdtutorials.com/complete-guide-for-ultrasonic-sensor-hc-sr04/

---

**¡Listo!** 🚀 Tu galpón inteligente está operativo.

Preguntas: [@AndresL2525](https://github.com/AndresL2525)
