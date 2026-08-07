# 🐔 AVÍSENS — Sistema IoT de Monitoreo y Automatización para Granjas Avícolas

**AVÍSENS** es un sistema embebido completo que monitorea condiciones microclimáticas en galpones de crianza avícola y automatiza los actuadores (calefactor, extractor, humidificador, alimentador) en tiempo real. Basado en **ESP32** con respaldo en **Firebase**, permite visualizar datos remotamente desde apps Android/Web mientras el dispositivo mantiene **autonomía total** en modo automático.

---

## 📊 Características Principales

- ✅ **Monitoreo de 5 parámetros** en tiempo real:
  - 🌡️ Temperatura y humedad (DHT11)
  - ⚖️ Peso del alimento en plato (HX711 + celda de carga 1kg)
  - 🚫 Detector de obstáculos (sensor KY-032 IR)
  - 💨 Calidad del aire (sensor MQ-135)

- ✅ **4 Actuadores automatizados**:
  - 🔥 Calefactor (< 18°C)
  - 🌬️ Extractor (> 28°C ó aire malo)
  - 💧 Humidificador (50-70% humedad con histéresis)
  - 🌾 Alimentador automático (peso objetivo + intervalo de seguridad)

- ✅ **Autonomía inteligente**:
  - El ESP32 decide **sin esperar a la nube**
  - Modo AUTO: decisiones automáticas
  - Modo MANUAL: las apps pueden forzar encendido/apagado vía Firebase

- ✅ **Interfaz local + remota**:
  - 📟 Pantalla OLED SH1106 (I2C, 128x64) con datos en tiempo real
  - ☁️ Firebase Realtime Database para sincronización y eventos
  - 📱 Compatible con apps Android/Web

- ✅ **Arquitectura no bloqueante**:
  - Sin `delay()` en el loop principal
  - Uso de `millis()` para tareas concurrentes
  - 4 tareas independientes ejecutándose "en paralelo"

---

## 🏗️ Arquitectura del Sistema

```
┌─────────────────────────────────────────────────────────────┐
│                       ESP32 (microcontrolador)              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────────┐      ┌──────────────────┐             │
│  │   SENSORES       │      │   ACTUADORES     │             │
│  │  ─────────────   │      │  ──────────────   │             │
│  │ • DHT11 (T/H)   │      │ • Calefactor     │             │
│  │ • HX711 (peso)  │      │ • Extractor      │ ─→ Relés    │
│  │ • KY-032 (obs)  │      │ • Humidificador  │             │
│  │ • MQ-135 (aire) │      │ • Alimentador    │             │
│  └──────────────────┘      └──────────────────┘             │
│           │                        │                        │
│           ▼                        ▼                        │
│  ┌──────────────────────────────────────────┐              │
│  │    GestorActuadores (CEREBRO AUTÓNOMO)   │              │
│  │  Decides qué encender/apagar en AUTO     │              │
│  └──────────────────────────────────────────┘              │
│           │                        │                        │
│           ▼                        ▼                        │
│  ┌──────────────────┐      ┌──────────────────┐            │
│  │  PantallaOLED    │      │ ServicioFirebase │            │
│  │  Datos locales   │      │  Nube RTDB       │ ◄─►  WiFi  │
│  └──────────────────┘      └──────────────────┘            │
│                                     │                       │
│           main.cpp (ORQUESTADOR)    │                       │
│     Ejecuta 4 tareas en paralelo    │                       │
│                                     │                       │
└─────────────────────────────────────┼───────────────────────┘
                                      │
                      ┌───────────────┴───────────────┐
                      ▼                               ▼
                 ☁️ Firebase RTDB            📱 Apps Remotas
                 • Sensores                 (Android/Web)
                 • Actuadores
                 • Eventos
```

---

## 📁 Estructura de Carpetas

```
proyecto_iot/
│
├── 📄 README.md                    ← Tú estás aquí
├── 📄 platformio.ini               ← Configuración PlatformIO
│
├── 📂 include/                     ← HEADERS (.h)
│   ├── config.h.example            ← Plantilla de configuración
│   ├── config.h                    ← Config local (NO pushear a Git)
│   │
│   ├── ConexionWiFi.h              ← Manejo de WiFi
│   ├── SensorDHT.h                 ← Temperatura/humedad
│   ├── SensorPeso.h                ← Peso (HX711)
│   ├── SensorKY032.h               ← Detector de obstáculos
│   ├── SensorMQ135.h               ← Calidad del aire
│   │
│   ├── Actuador.h                  ← Clase base para relés
│   ├── Alimentador.h               ← Lógica especial del alimentador
│   ├── GestorActuadores.h          ← CEREBRO: decide encendidos
│   │
│   ├── PantallaOLED.h              ← Renderizado OLED
│   ├── ServicioFirebase.h          ← Envío de datos (PUT/POST)
│   └── ServicioActuadoresFirebase.h← Control remoto (GET/eventos)
│
└── 📂 src/                         ← IMPLEMENTACIONES (.cpp)
    ├── main.cpp                    ← Loop principal + orquestación
    ├── ConexionWiFi.cpp
    ├── SensorDHT.cpp
    ├── SensorPeso.cpp
    ├── SensorKY032.cpp
    ├── SensorMQ135.cpp
    ├── Actuador.cpp
    ├── Alimentador.cpp
    ├── GestorActuadores.cpp
    ├── PantallaOLED.cpp
    ├── ServicioFirebase.cpp
    └── ServicioActuadoresFirebase.cpp
```

---

## ⚙️ Componentes de Hardware

### Microcontrolador
| Componente | Modelo | Detalles |
|-----------|--------|----------|
| **Placa** | ESP32-DevKitC | 38 pines, WiFi + Bluetooth, 240 MHz |
| **Memoria RAM** | 520 KB | Suficiente para los módulos |
| **Flash** | 4 MB | Código + datos del proyecto |

### Sensores
| Sensor | Modelo | Interfaz | Parámetro |
|--------|--------|----------|-----------|
| **Temp/Humedad** | DHT11 | 1-Wire digital | Ambiente |
| **Peso** | HX711 + Celda 1kg | SPI-like (DOUT/SCK) | Alimento |
| **Obstáculos** | KY-032 | GPIO digital | Infrarrojos |
| **Aire** | MQ-135 | ADC analógico (0-4095) | Calidad |

### Actuadores
| Actuador | Controlado por | Carga | Uso |
|----------|---|------|-----|
| **Calefactor** | Relé GPIO25 | 110V/220V | Temperatura < 18°C |
| **Extractor** | Relé GPIO26 | 110V/220V | Temp > 28°C ó aire malo |
| **Humidificador** | Relé GPIO27 | 110V/220V | 50% < Humedad < 70% |
| **Alimentador** | Relé GPIO32 | Motorreductor | Peso objetivo |

### Salida Visual
| Componente | Modelo | Interfaz | Datos |
|-----------|--------|----------|-------|
| **OLED** | SH1106 | I2C (GPIO21/22) | Sensor + estado |

---

## 🔌 Pinout Rápido del ESP32

```
┌─────────────────────────────────────────┐
│            ESP32 PINOUT                 │
├─────────────────────────────────────────┤
│  SENSORES:                              │
│  • GPIO5    ← DHT11 (datos)            │
│  • GPIO16   ← HX711 DOUT               │
│  • GPIO17   ← HX711 SCK                │
│  • GPIO13   ← KY-032 (sensor IR)       │
│  • GPIO34   ← MQ-135 (ADC)             │
│                                         │
│  PANTALLA OLED:                        │
│  • GPIO21   ← SDA (I2C)                │
│  • GPIO22   ← SCL (I2C)                │
│                                         │
│  ACTUADORES:                           │
│  • GPIO25   → Relé Calefactor          │
│  • GPIO26   → Relé Extractor           │
│  • GPIO27   → Relé Humidificador       │
│  • GPIO32   → Relé Alimentador         │
│                                         │
│  OTROS:                                │
│  • GPIO0    ← BOOT (pullup)            │
│  • GPIO2    ← LED Azul                 │
│  • GND/3.3V → Alimentación             │
└─────────────────────────────────────────┘
```

---

## 🧠 Módulos Principales

### 1. **main.cpp** — Orquestador
El corazón del proyecto. No contiene lógica de negocio, solo **coordina cuándo** se ejecutan las 4 tareas:

```cpp
// 4 tareas independientes en paralelo (sin delay bloqueante):
1. Leer sensores + evaluar actuadores  (cada 2000 ms)
2. Refrescar pantalla OLED             (cada 1000 ms)
3. Enviar a Firebase                   (cada 5000 ms)
4. Sincronizar órdenes remotas         (cada 10000 ms)
```

**Características:**
- ✅ Usa `millis()` en lugar de `delay()`
- ✅ Tareas corren "en paralelo" sin bloquearse
- ✅ Genera eventos en Firebase **solo cuando hay cambios reales**

---

### 2. **GestorActuadores.h/cpp** — Cerebro Autónomo

Aquí está **toda** la lógica de decisión. Implementa los umbrales de automatización:

```
CALEFACTOR:
  Si temperatura < 18°C  → ENCENDER
  Si temperatura ≥ 18°C  → APAGAR

EXTRACTOR:
  Si (temperatura > 28°C) OR (calidad_aire > 2000)  → ENCENDER
  Si (temperatura ≤ 28°C) AND (calidad_aire ≤ 2000) → APAGAR

HUMIDIFICADOR (con histéresis):
  Si humedad < 50%  → ENCENDER
  Si humedad > 70%  → APAGAR
  [En el rango 50-70% mantiene estado anterior]

ALIMENTADOR:
  Si (peso < 300g) AND (≥ 4 horas desde última dosis) → DISPENSAR
```

**Independencia:**
- Cada actuador se evalúa de forma aislada
- Es válido que calefactor Y extractor estén encendidos a la vez (ej. temp baja + aire malo)

---

### 3. **SensorDHT, SensorPeso, SensorKY032, SensorMQ135**

Cada sensor es un módulo independiente con:
- Inicialización específica
- Lectura con **manejo de errores**
- Promediado de múltiples lecturas (reduce ruido)
- Timeout automático si el sensor falla

Ejemplo (SensorDHT):
```cpp
LecturaDHT leer() {
  // Intenta leer, retorna struct con:
  // - temperatura (float)
  // - humedad (float)
  // - valida (bool) ← indica si la lectura fue exitosa
}
```

---

### 4. **PantallaOLED.h/cpp**

Renderiza en tiempo real en la pantalla OLED 128×64:
```
┌─────────────────────────┐
│ T:22.5°C  H:65%  🌐    │ ← Temp, Humedad, WiFi conectado
│ P:125g  Aire:1200      │ ← Peso, Calidad aire
│─────────────────────────│
│ ☐Cal ☑Ext ☐Hum ☑Ali   │ ← Estado de actuadores (checked=ON)
│─────────────────────────│
│ Modo: AUTO             │ ← Modo operativo
│ Obs:NO                 │ ← Obstáculo detectado
└─────────────────────────┘
```

---

### 5. **ServicioFirebase.h/cpp** — Envío de Datos

Envía **periódicamente** (cada 5 segundos) un snapshot completo a Firebase:

```json
{
  "sensores": {
    "temperatura": 22.5,
    "humedad": 65,
    "peso": 125.3,
    "calidadAire": 1200,
    "obstaculos": false,
    "timestamp": 1657924800000
  },
  "actuadores": {
    "calefactor": { "estado": false, "modo": "AUTO" },
    "extractor": { "estado": true, "modo": "AUTO" },
    "humidificador": { "estado": false, "modo": "AUTO" },
    "alimentador": { "estado": false }
  },
  "eventos": [
    { "tipo": "ACTUADOR", "nombre": "extractor", "mensaje": "activado", "nivel": "info", "ts": ... }
  ]
}
```

---

### 6. **ServicioActuadoresFirebase.h/cpp** — Control Remoto + Eventos

**Descarga** órdenes remotas desde Firebase (cada 10 segundos):
- Cambios de MODO (AUTO ↔ MANUAL)
- Órdenes manuales de encendido/apagado

**Registra eventos** automáticamente en `/eventos` cuando:
- Un actuador cambia de estado
- Un sensor falla
- Se reciben órdenes remotas

Esto permite que las apps vean el historial de cambios.

---

## ⚙️ Configuración

### Paso 1: Clonar + Preparar

```bash
git clone https://github.com/AndresL2525/proyecto_iot.git
cd proyecto_iot
cp include/config.h.example include/config.h
```

### Paso 2: Editar `config.h`

```cpp
// WiFi
#define WIFI_SSID "Tu_Red_WiFi"
#define WIFI_PASSWORD "Tu_Contraseña"

// Firebase
#define FIREBASE_HOST "https://mi-proyecto-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "Tu_Secret_Firebase"

// Pines (opcional si usas los estándares)
#define DHT_PIN 5
#define HX711_DOUT_PIN 16
#define HX711_SCK_PIN 17

// Umbrales de automatización
#define UMBRAL_TEMP_BAJA 18.0
#define UMBRAL_TEMP_ALTA 28.0
#define UMBRAL_HUMEDAD_BAJA 50
#define UMBRAL_HUMEDAD_ALTA 70
```

### Paso 3: Calibrar Celda de Carga

1. Sube el proyecto con `FACTOR_CALIBRACION_CELDA = -1`
2. Abre Monitor Serie → ejecuta `scale.tare()`
3. Coloca un peso conocido (ej. 500g) en la celda
4. Lee el valor crudo en Monitor
5. Calcula: `factor = valor_crudo / peso_real`
6. Actualiza `FACTOR_CALIBRACION_CELDA` en `config.h`

### Paso 4: Compilar y Subir

```bash
# Con PlatformIO CLI
pio run -t upload

# O en VSCode: Ctrl+Alt+U
```

---

## 📱 Flujo de Datos

### En tiempo real (cada 5 seg):

```
Sensores
   ↓
main.cpp (evalúa)
   ↓
GestorActuadores (decide)
   ↓
Relés (actuadores físicos)
   ├→ PantallaOLED (muestra localmente)
   └→ Firebase (almacena en nube)
```

### Desde la app remota (cada 10 seg):

```
Firebase (órdenes)
   ↓
main.cpp (sincroniza)
   ↓
GestorActuadores (aplica si MANUAL)
   ↓
Relés (cambian)
   ↓
Firebase (reporta cambio)
```

---

## 🔒 Seguridad + Buenas Prácticas

| Aspecto | Implementación |
|--------|---|
| **Credenciales** | Centralizadas en `config.h`, NO en Git (.gitignore) |
| **Relés** | Aislados electricamente (220V nunca toca ESP32) |
| **Alimentador** | Timeout de 8 seg + peso objetivo (evita atascos) |
| **Sensores** | Con timeout y reintento automático si fallan |
| **WiFi** | Timeout de 15 seg para no bloquear setup |
| **Firebase** | Usa autenticación por secret (mejorable a Auth para prod) |

---

## 🚀 Próximas Mejoras

- [ ] Migrar de Firebase Secret a Firebase Authentication
- [ ] Agregar autenticación de usuario en apps
- [ ] Historial persistente de eventos (base de datos local)
- [ ] Machine Learning para predicción de necesidades
- [ ] Notificaciones push si sensores fallan
- [ ] Control de energía (monitoreo de consumo)
- [ ] Múltiples galpones en una sola Firebase (multi-tenancy)

---

## 📚 Librerías Usadas

```
lib_deps =
    adafruit/DHT sensor library@^1.4.6
    adafruit/Adafruit Unified Sensor@^1.1.14
    bogde/HX711@^0.7.5
    olikraus/U8g2@^2.35.19
    bblanchon/ArduinoJson@^6.21.3
    beegee-tokyo/DHT sensor library for ESPx@^1.19
    madhephaestus/ESP32Servo@^3.0.6
```

---

## 🐛 Troubleshooting

### Pantalla OLED no aparece
- ✓ Verifica I2C: GPIO21 (SDA), GPIO22 (SCL)
- ✓ Confirmad dirección I2C 0x3C
- ✓ Usa comando: `i2cdetect -y 1` en Raspberry Pi conectada

### DHT11 no da lecturas
- ✓ Revisa pin GPIO5 (o el configurado)
- ✓ DHT necesita 2-3 segundos entre lecturas
- ✓ Mira Monitor Serie para ver mensajes de error

### HX711 lee valores raros
- ✓ Recalibra el factor (ver Paso 3 arriba)
- ✓ Asegúrate de que no hay peso en la celda al tarear
- ✓ Verifica cables DOUT (GPIO16) y SCK (GPIO17)

### Firebase no recibe datos
- ✓ Confirma que WiFi está conectado (OLED muestra 🌐)
- ✓ Revisa credenciales en `config.h`
- ✓ Mira Monitor Serie (velocidad 115200 baud)
- ✓ Firebase → Rules: asegúrate que `.read` y `.write` están habilitados

---

## 📖 Documentación Adicional

| Documento | Contenido |
|----------|----------|
| `config.h.example` | Explicación de cada parámetro |
| `main.cpp` | Comentarios sobre orquestación |
| `GestorActuadores.cpp` | Lógica detallada de decisiones |
| `src/*.cpp` | Docstrings en cada función |

---

## 👤 Autor

**André**  
SENA — Ficha 3229446  
Centro de Teleinformática y Producción Industrial, Región Cauca  
Carrera: Análisis y Desarrollo de Software (ADSO)

**GitHub**: [AndresL2525](https://github.com/AndresL2525)

---

## 📜 Licencia

Este proyecto es académico y está disponible bajo licencia MIT.

---

## 🤝 Contribuciones

Si encuentras bugs o tienes mejoras, abre un **Issue** o **Pull Request** en GitHub.

---

**Última actualización:** Julio 2026  
**Estado:** ✅ Funcional — Firmware completo, Apps Android/Web en desarrollo
