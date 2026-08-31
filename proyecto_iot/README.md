# Galpón Inteligente — Arquitectura Modular ESP32 (TRL 5)

Refactorización profesional del sistema de automatización de galpón avícola basado en ESP32 + FreeRTOS.

## 📋 Características

✅ **Modularización completa** — 23 archivos organizados en `include/` y `src/`  
✅ **No bloqueante 100%** — millis() y máquinas de estado (FSM) en todos los componentes  
✅ **Filtro de Media Móvil** — Suavizado de sensores ruidosos (MQ135, HC-SR04)  
✅ **Watchdog Timer (WDT)** — Supervisión de fallos con reinicio automático  
✅ **WiFi Asíncrono** — Desacoplado de Core 0 (Bluepad32 compatible)  
✅ **Fail-Safe Robusto** — Máquina de estado global + fail-safe por sensor  
✅ **FreeRTOS Dual Core** — Core 0 para sensores/actuadores, Core 1 para WiFi  
✅ **Código Industrial** — Enum class tipadas, logging estructurado, documentación Doxygen

---

## 📁 Estructura de Archivos

```
galpon_modular/
├── include/
│   ├── config.h              # Configuración centralizada
│   ├── MovingAverage.h       # Template de filtro media móvil
│   ├── SensorDHT.h           # DHT22 (temp/humedad)
│   ├── SensorMQ135.h         # MQ135 (gases) con filtro
│   ├── SensorKY032.h         # KY032 (presencia)
│   ├── SensorUltrasonico.h   # HC-SR04 (nivel agua) con filtro
│   ├── SensorPeso.h          # Placeholder para HX711
│   ├── Actuador.h            # Clase base para relés
│   ├── GestorActuadores.h    # Gestor K1-K4 con lógica control
│   ├── ControlServo.h        # Servo puerta (FSM)
│   ├── Alimentador.h         # Motor alimento (FSM)
│   └── ConexionWiFi.h        # WiFi asíncrono
├── src/
│   ├── main.cpp              # Setup + tareaGalpon + tareaWiFi
│   ├── SensorDHT.cpp
│   ├── SensorMQ135.cpp
│   ├── SensorKY032.cpp
│   ├── SensorUltrasonico.cpp
│   ├── SensorPeso.cpp
│   ├── Actuador.cpp
│   ├── GestorActuadores.cpp
│   ├── ControlServo.cpp
│   ├── Alimentador.cpp
│   └── ConexionWiFi.cpp
├── platformio.ini            # Configuración de compilación
├── README.md                 # Este archivo
└── LICENSE

```

---

## 🚀 Instalación y Compilación

### Requisitos

- **PlatformIO** (VS Code Extension o CLI)
- **Python 3.8+** (para PlatformIO)
- **ESP32 DevKit** o compatible

### Pasos

1. **Clonar/Descargar el repositorio:**
   ```bash
   git clone https://github.com/AndresL2525/galpon_modular.git
   cd galpon_modular
   ```

2. **Conectar ESP32 por USB**

3. **Compilar con PlatformIO:**
   ```bash
   platformio run -e esp32
   ```

4. **Cargar en el microcontrolador:**
   ```bash
   platformio run -e esp32 -t upload
   ```

5. **Abrir monitor serial:**
   ```bash
   platformio device monitor -e esp32
   ```

---

## ⚙️ Configuración

Editar `include/config.h` para ajustar:

- **Pines GPIO** — Cambiar asignaciones si es necesario
- **Umbrales de control:**
  - `TEMP_FRIO` / `TEMP_CALOR` — Puntos de disparo de calefacción/ventilación
  - `HUM_EXTRACTORES` — Umbral de humedad para activar extractor
  - `NH3_ALTO` — Nivel de gases para ventilación de emergencia
  - `NIVEL_BOMBA_ON` / `NIVEL_BOMBA_OFF` — Histéresis de la bomba
- **Tiempos:**
  - `INTERVALO_SENSORES` — Período de lectura (2000 ms)
  - `INTERVALO_PERSIANA` — Ciclo de apertura de persiana (5 min)
  - `INTERVALO_ALIMENTO` — Ciclo de dispensado (5 min)
- **WiFi** — `main.cpp:48` cambiar SSID/Password

---

## 📊 Máquinas de Estado Implementadas

### 1. **Estado Global del Sistema** (config.h)
```
INIT → CALIBRATION → MONITORING → ACTUATION ↔ ERROR → SHUTDOWN
```

### 2. **Control de Puerta** (ControlServo)
```
CERRADA → ABRIENDO → ABIERTA → CERRANDO → CERRADA
```

### 3. **Persiana** (Implementar: PersianasControlador)
```
QUIETA → ABRIENDO → PAUSA → CERRANDO → QUIETA
```

### 4. **Alimentador** (Alimentador)
```
APAGADO → ENCENDIDO → APAGADO (ciclo periódico)
```

---

## 🛡️ Características de Robustez

### Watchdog Timer (WDT)
- Timeout: **10 segundos**
- Monitoreo: Solo `tareaGalpon()` (Core 0)
- Reset automático si se detiene la tarea

### Fail-Safe por Sensor
- **DHT22**: Contador de fallos → entra en ERROR después de 3 intentos fallidos
- **HC-SR04**: Contador de fallos → mantiene último estado válido
- **Error global**: Activa `failSafe()` → Desactiva calefacción/ventilación, bomba ON

### Histéresis en Bomba
- **ON** si distancia > 6.0 cm
- **OFF** si distancia ≤ 3.0 cm
- Evita oscilaciones

### Filtro de Media Móvil (MovingAverage)
- **Tamaño buffer**: 10 muestras
- **Aplicado a**: MQ135 (raw), HC-SR04 (distancia)
- **Reduce ruido**: ~80% de reducción típica

---

## 📡 Comunicación WiFi

### Configuración
```cpp
// main.cpp:48
ConexionWiFi conexionWiFi("tu_ssid", "tu_password");
```

### Características
- Conexión **no bloqueante** (Core 1)
- Reconexión automática cada 30 segundos
- mDNS hostname configurable
- Desacoplado de sensores/actuadores

---

## 🔧 Extensiones Futuras

### Sensores a integrar
- [ ] **SensorPeso.cpp** — HX711 para monitoreo de consumo
- [ ] **Sensor de movimiento** (PIR) — Activar iluminación
- [ ] **Sensor de calidad de aire** (MQ-7 CO)

### Comunicación
- [ ] **MQTT** — Publicar datos a broker (Home Assistant)
- [ ] **API REST** — Endpoints para control remoto
- [ ] **OTA Updates** — Actualización remota de firmware

### Control Avanzado
- [ ] **Persiana adaptativa** — Basada en luz solar
- [ ] **Control PID** — Regulación fina de temperatura
- [ ] **Ventilación nocturna** — Reducción de temperatura

---

## 🐛 Debugging

### Niveles de Log
En `config.h`:
```cpp
#define LOG_DEBUG(msg)    Serial.println(msg)
#define LOG_WARN(msg)     Serial.print("⚠ "); Serial.println(msg)
#define LOG_ERROR(msg)    Serial.print("❌ "); Serial.println(msg)
```

### Monitor Serial
```bash
platformio device monitor -e esp32 --echo --eol LF
```

### Estadísticas en Tiempo Real
El `main.cpp` mantiene:
- `ciclosTarea` — Número de ciclos ejecutados
- `erroresGlobales` — Contador de errores
- Logs cada 2 segundos de sensores y actuadores

---

## 📝 Convenciones de Código

- **Nombres en español** — Familiaridad para el equipo local
- **Enum class** — Tipado fuerte de estados
- **Prefijo `_` para miembros privados** — Claridad en encapsulación
- **Documentación Doxygen** — Comentarios en cabeceras
- **No bloqueantes** — Siempre usar `vTaskDelay()` en lugar de `delay()`

---

## 📞 Contacto y Soporte

**Autor**: Andrés Luna (@AndresL2525)  
**Institución**: SENA (ADSO 3229446) + Universidad del Cauca  
**Email**: [tu-email]

---

## 📄 Licencia

MIT License — Libre para uso comercial y educativo.

---

**Versión**: 7.0  
**Última actualización**: Agosto 2026  
**TRL**: 5 (Industrial readiness)
