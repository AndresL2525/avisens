

# Documento de Diseño del Sistema (SSD)
# AVÍSENS — Sistema IoT para Granjas Avícolas


---

## Tabla de Contenidos

1. [Introducción](#1-introducción)
2. [Descripción General del Sistema](#2-descripción-general-del-sistema)
3. [Arquitectura del Sistema](#3-arquitectura-del-sistema)
4. [Diseño Detallado del Backend](#4-diseño-detallado-del-backend)
5. [Diseño Detallado del Firmware IoT](#5-diseño-detallado-del-firmware-iot)
6. [Base de Datos](#6-base-de-datos)
7. [Seguridad](#7-seguridad)
8. [Interfaces y APIs](#8-interfaces-y-apis)
9. [Calidad, Robustez y Manejo de Errores](#9-calidad-robustez-y-manejo-de-errores)
10. [Despliegue e Infraestructura](#10-despliegue-e-infraestructura)
11. [Riesgos y Limitaciones](#11-riesgos-y-limitaciones)
12. [Roadmap y Próximos Pasos](#12-roadmap-y-próximos-pasos)
13. [Apéndices](#13-apéndices)

---

## 1. Introducción

### 1.1 Propósito

Este documento describe el diseño técnico completo del sistema **AVÍSENS**, una solución IoT para el monitoreo y automatización de galpones avícolas. El SSD cubre tanto el backend de servicios en la nube como el firmware embebido del dispositivo ESP32, definiendo la arquitectura, los componentes, las interfaces y las decisiones de diseño críticas.

### 1.2 Alcance

El documento abarca:
- Arquitectura de software del backend (FastAPI + MongoDB)
- Arquitectura de firmware del ESP32 (Arduino + FreeRTOS)
- Modelos de datos y esquemas de base de datos
- APIs REST y protocolos de comunicación
- Estrategias de seguridad y autenticación
- Mecanismos de robustez, fail-safe y manejo de errores
- Estrategia de despliegue con Docker

### 1.3 Definiciones y Acrónimos

| Término | Definición |
|---------|-----------|
| **SSD** | Software/System Design Document |
| **IoT** | Internet of Things |
| **ESP32** | Microcontrolador Wi-Fi/Bluetooth de Espressif |
| **FreeRTOS** | Sistema operativo en tiempo real |
| **FSM** | Finite State Machine (Máquina de Estado Finita) |
| **JWT** | JSON Web Token |
| **API REST** | Representational State Transfer |
| **MongoDB** | Base de datos NoSQL orientada a documentos |
| **Motor** | Driver async de MongoDB para Python |
| **Pydantic** | Biblioteca de validación de datos para Python |
| **WDT** | Watchdog Timer |
| **TRL** | Technology Readiness Level |
| **mTLS** | Mutual Transport Layer Security |
| **CORS** | Cross-Origin Resource Sharing |
| **FSM Mealy** | Máquina de estado donde las salidas dependen del estado actual y las entradas |

### 1.4 Referencias

- ESP32 Technical Reference Manual (Espressif)
- FreeRTOS Kernel Documentation
- FastAPI Documentation
- MongoDB Atlas Documentation
- IEC 61508 — Safety Integrity Levels
- MISRA C — Coding Standards

---

## 2. Descripción General del Sistema

### 2.1 Perspectiva del Producto

AVÍSENS es un sistema integrado compuesto por tres capas principales:

1. **Capa de Dispositivo (Edge):** Un ESP32 instalado en el galpón que lee sensores ambientales, controla actuadores (calefacción, ventilación, bomba, alimentador, persiana, puerta) y se comunica con el backend vía WiFi.
2. **Capa de Servicios (Cloud):** Un backend RESTful desarrollado en FastAPI que recibe datos de los dispositivos, los almacena en MongoDB Atlas, expone APIs para consulta y gestiona comandos hacia los actuadores.
3. **Capa de Cliente (Frontend):** Aplicaciones móviles (Kotlin) o web que consumen la API para visualizar datos y enviar comandos de control manual.

### 2.2 Funcionalidades del Sistema

| ID | Funcionalidad | Descripción | Responsable |
|----|--------------|-------------|-------------|
| F-01 | Monitoreo ambiental | Lectura de temperatura, humedad, calidad de aire, nivel de agua y peso | ESP32 |
| F-02 | Control automático | Activación de calefacción, ventilación, extractor y bomba basado en umbrales | ESP32 |
| F-03 | Control de acceso | Apertura/cierre automático de puerta por detección de presencia | ESP32 |
| F-04 | Alimentación automática | Dosificación periódica de alimento balanceado | ESP32 |
| F-05 | Ventilación programada | Ciclo de apertura/cierre de persiana cada 5 minutos | ESP32 |
| F-06 | Telemetría en la nube | Envío de lecturas de sensores al backend cada 5 segundos | ESP32 + Backend |
| F-07 | Consulta histórica | Apps consultan lecturas pasadas, estadísticas y eventos | Backend + App |
| F-08 | Control remoto | Apps envían comandos manuales a actuadores | Backend + ESP32 |
| **F-09** | **Alertas de Fallo y Datos Anómalos** | Detección inmediata de sensores desconectados, fallos continuos de lectura ($N \ge 3$) y valores fuera de límites físicos plausibles (outliers). Notificación automática como evento crítico al backend. | ESP32 + Backend |
| F-10 | Autenticación dual | Tokens JWT separados para dispositivos y usuarios | Backend |
| **F-11** | **Detección de Dispositivo Offline (Heartbeat/Watchdog en Nube)** | Monitoreo pasivo en el backend que genera una alerta de desconexión si un galpón no emite lecturas de telemetría en más de 30 segundos consecutivos. | Backend |

### 2.3 Características de los Usuarios

| Rol | Descripción | Interacción |
|-----|-------------|-------------|
| **Administrador del galpón** | Usuario final que monitorea y controla el galpón vía app móvil | Consulta datos, envía comandos manuales, revisa alertas |
| **Dispositivo IoT (ESP32)** | Cliente automatizado que envía datos y consulta comandos | POST sensores, GET comandos, POST estado actuadores |
| **Sistema de monitoreo** | Herramientas externas (Docker, K8s) que verifican salud | GET /health |

### 2.4 Restricciones

- El ESP32 tiene 520 KB de RAM total; el firmware debe operar dentro de este límite.
- El backend está diseñado como microservicio independiente; la autenticación de usuarios es un stub que debe integrarse con el backend de autenticación de compañeros.
- MongoDB Atlas gratuito (M0) tiene límite de 512 MB de almacenamiento.
- El ESP32 no implementa reintentos con backoff ni buffer circular para datos en caso de fallo de conexión WiFi.
- No se usa HTTPS en el ESP32 en la versión actual (HTTP plano).

### 2.5 Suposiciones y Dependencias

- El ESP32 tiene acceso a red WiFi estable.
- MongoDB Atlas está disponible y accesible desde el backend.
- El backend se despliega con Docker en un servidor con acceso a Internet.
- Las apps móviles/web consumen la API REST documentada.
- Los sensores están calibrados y conectados correctamente al ESP32.

---

## 3. Arquitectura del Sistema

### 3.1 Diagrama de Arquitectura General

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              CAPA CLIENTE                                    │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐                      │
│  │  App Móvil  │    │  App Web    │    │  Dashboard  │                      │
│  │   (Kotlin)  │    │  (React)    │    │  (Futuro)   │                      │
│  └──────┬──────┘    └──────┬──────┘    └──────┬──────┘                      │
└─────────┼──────────────────┼──────────────────┼──────────────────────────────┘
          │                  │                  │
          │  HTTPS / JWT     │  HTTPS / JWT     │  HTTPS / JWT
          │  (User Token)    │  (User Token)    │  (User Token)
          ▼                  ▼                  ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              CAPA SERVICIOS                                  │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    AVÍSENS Backend (FastAPI)                         │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐           │    │
│  │  │  Auth    │  │ Sensors  │  │ Actuators│  │  Events  │           │    │
│  │  │ Router   │  │ Router   │  │ Router   │  │ Router   │           │    │
│  │  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘           │    │
│  │       │             │             │             │                  │    │
│  │  ┌────┴─────────────┴─────────────┴─────────────┴─────┐           │    │
│  │  │              Middleware Layer                        │           │    │
│  │  │  (CORS, Security Headers, Rate Limiting, JWT)      │           │    │
│  │  └─────────────────────┬───────────────────────────────┘           │    │
│  │                        │                                          │    │
│  │  ┌─────────────────────┴───────────────────────────────┐           │    │
│  │  │              Services Layer                          │           │    │
│  │  │  (AuthService, SensorService, ActuatorService)      │           │    │
│  │  └─────────────────────┬───────────────────────────────┘           │    │
│  │                        │                                          │    │
│  │  ┌─────────────────────┴───────────────────────────────┐           │    │
│  │  │              Database Layer (Motor + MongoDB)        │           │    │
│  │  └──────────────────────────────────────────────────────┘           │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                              [Docker Container]                              │
└─────────────────────────────────────────────────────────────────────────────┘
          ▲
          │  HTTPS / JWT (Device Token)
          │  POST /sensors/readings
          │  GET  /actuators/commands
          │  POST /actuators/state
          │
┌─────────────────────────────────────────────────────────────────────────────┐
│                            CAPA DISPOSITIVO (EDGE)                           │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                         ESP32 DevKit                               │    │
│  │  ┌─────────────────────────────────────────────────────────────┐   │    │
│  │  │  Core 0 (FreeRTOS) — Sensores + Actuadores + Control        │   │    │
│  │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐          │   │    │
│  │  │  │  DHT22  │ │  MQ135  │ │ HC-SR04 │ │  KY032  │          │   │    │
│  │  │  │(Temp/Hum│ │ (Gases) │ │ (Agua)  │ │(Puerta) │          │   │    │
│  │  │  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘          │   │    │
│  │  │       │           │           │           │                │   │    │
│  │  │  ┌────┴───────────┴───────────┴───────────┴────┐          │   │    │
│  │  │  │         GestorActuadores (Lógica)            │          │   │    │
│  │  │  │  K1(Calefacción)  K2(Ventilador)            │          │   │    │
│  │  │  │  K3(Extractor)    K4(Bomba)                 │          │   │    │
│  │  │  │  Servo(Puerta)    Persiana(L293D)           │          │   │    │
│  │  │  │  Alimentador(L293D)  HX711(Peso)           │          │   │    │
│  │  │  └──────────────────────────────────────────────┘          │   │    │
│  │  └─────────────────────────────────────────────────────────────┘   │    │
│  │  ┌─────────────────────────────────────────────────────────────┐   │    │
│  │  │  Core 1 (FreeRTOS) — WiFi + Comunicación                   │   │    │
│  │  │  ┌─────────────────────────────────────────────────────┐   │   │    │
│  │  │  │  ConexionWiFi (Async) → HTTP REST → Backend       │   │   │    │
│  │  │  └─────────────────────────────────────────────────────┘   │   │    │
│  │  └─────────────────────────────────────────────────────────────┘   │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Decisiones Arquitectónicas Clave

| Decisión | Descripción | Justificación |
|----------|-------------|---------------|
| **FastAPI + Motor** | Backend async con MongoDB async | Alto rendimiento, validación automática, documentación OpenAPI |
| **JWT dual** | Tokens separados para usuarios y dispositivos | Los dispositivos IoT no pueden refrescar tokens fácilmente |
| **FreeRTOS Dual Core** | Core 0 para control, Core 1 para WiFi | Aislamiento de tareas críticas de comunicación no bloqueante |
| **FSM Mealy** | Máquinas de estado finitas para cada componente | Control predecible, sin bloqueos, fácil de depurar |
| **Media Móvil** | Filtro de suavizado para sensores analógicos | Reduce ruido ~80% en MQ135 y HC-SR04 |
| **Docker Multi-Stage** | Imagen final mínima (~150MB) | Seguridad (usuario no-root), despliegue rápido |
| **MongoDB Atlas** | Base de datos gestionada en la nube | Sin mantenimiento de infraestructura, escalable |

---

## 4. Diseño Detallado del Backend

### 4.1 Stack Tecnológico

| Capa | Tecnología | Versión | Propósito |
|------|-----------|---------|-----------|
| Framework | FastAPI | 0.111.0 | API REST async, validación Pydantic, docs automáticas |
| Servidor | Uvicorn | 0.30.0 | Servidor ASGI para FastAPI |
| Base de datos | MongoDB + Motor | 4.8.0 / 3.5.0 | Almacenamiento NoSQL async |
| Validación | Pydantic v2 | 2.8.0 | Validación de inputs, serialización |
| Configuración | Pydantic-Settings | 2.3.0 | Variables de entorno tipadas |
| Auth | PyJWT | 2.8.0 | Tokens firmados HS256 |
| Hashing | Passlib + Bcrypt | 1.7.4 | Hash de contraseñas (futuro) |
| Rate Limit | SlowAPI | 0.1.9 | Protección contra abuso |
| Logging | Structlog | 24.4.0 | Logs estructurados JSON/texto |
| Testing | Pytest + Async | 8.3.0 / 0.23.0 | Tests unitarios y de integración |
| Container | Docker | 3.8 | Empaquetamiento y despliegue |

### 4.2 Estructura de Carpetas

```
avisens-backend/
├── app/
│   ├── main.py              # Punto de entrada FastAPI
│   ├── config.py            # Configuración centralizada (Pydantic Settings)
│   ├── database.py          # Conexión async MongoDB (Motor)
│   ├── routers/
│   │   ├── auth.py          # Autenticación (device + user)
│   │   ├── sensors.py       # Lecturas de sensores
│   │   ├── actuators.py     # Comandos y estados de actuadores
│   │   └── events.py        # Registro y consulta de eventos
│   ├── models/
│   │   ├── auth.py          # Token, DeviceAuthRequest, UserAuthRequest
│   │   ├── sensor.py        # SensorReadingCreate, SensorReadingResponse, SensorStats
│   │   ├── actuator.py      # ActuatorState, ActuatorCommand, ActuatorCommandResponse
│   │   └── event.py         # EventCreate, EventResponse
│   ├── services/
│   │   ├── auth_service.py  # JWT: crear, decodificar, validar
│   │   ├── sensor_service.py # CRUD + agregaciones de lecturas
│   │   └── actuator_service.py # Comandos y estados de actuadores
│   ├── middleware/
│   │   ├── rate_limit.py    # SlowAPI setup
│   │   └── security.py      # Headers de seguridad HTTP
│   └── utils/
│       ├── logger.py        # Configuración structlog
│       └── exceptions.py    # Excepciones personalizadas + handlers
├── Dockerfile               # Multi-stage build
├── docker-compose.yml       # Orquestación de contenedor
├── requirements.txt         # Dependencias Python
├── .env.example             # Variables de entorno de ejemplo
└── README.md                # Documentación de uso
```

### 4.3 Modelos de Datos (Pydantic)

#### 4.3.1 SensorReadingCreate

```python
class SensorReadingCreate(BaseModel):
    device_id: str          # ID del dispositivo (alfanumérico, _, -)
    temperatura: float      # °C, rango [-10, 60]
    humedad: float          # % RH, rango [0, 100]
    peso: float             # Gramos, rango [0, 5000]
    obstaculo: bool         # Detección de obstáculo
    calidad_aire: int       # Raw ADC [0, 4095]
    voltaje_aire: float     # Voltaje [0.0, 3.3]
    timestamp: datetime     # Opcional (usa servidor si null)
```

**Validaciones implementadas:**
- `device_id`: solo alfanumérico, guiones y guiones bajos
- Rangos físicos para cada sensor
- Rechazo de NaN e Inf en valores float
- Sanitización de strings (strip + lowercase)

#### 4.3.2 ActuatorCommand

```python
class ActuatorCommand(BaseModel):
    device_id: str
    nombre: Literal["calefactor", "extractor", "humidificador", "alimentador"]
    modo: Literal["AUTO", "MANUAL"]
    orden_manual: bool | None  # Obligatorio si modo=MANUAL
```

#### 4.3.3 EventCreate

```python
class EventCreate(BaseModel):
    device_id: str
    tipo: Literal["ACTUADOR", "FALLA_SENSOR", "SISTEMA", "ALERTA", "INFO"]
    origen: str             # Módulo que generó el evento
    mensaje: str            # Max 500 caracteres, sanitizado
    nivel: Literal["info", "advertencia", "alerta", "critico"]
    metadata: dict          # Datos adicionales estructurados
```

### 4.4 Flujo de Autenticación JWT

```
┌─────────┐                    ┌─────────────┐                    ┌─────────┐
│  ESP32  │──POST /auth/device/│   Backend   │──Genera JWT──────→│  ESP32  │
│         │   login            │  (FastAPI)  │   (1 año)          │ (flash) │
└─────────┘                    └─────────────┘                    └─────────┘
       │                              │
       │  Bearer <token>              │  Bearer <token>
       ▼                              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  POST /sensors/readings          GET /actuators/commands                    │
│  (Solo device token)             (Solo device token)                        │
│                                                                             │
│  GET /sensors/readings           POST /actuators/commands                   │
│  (Solo user token)               (Solo user token)                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Claims del JWT:**
- `sub`: device_id o username
- `exp`: fecha de expiración
- `iat`: fecha de emisión
- `type`: "device" | "user"
- `jti`: UUID único para revocación futura

**Seguridad anti-spoofing:**
El endpoint `POST /sensors/readings` compara el `device_id` del token JWT con el `device_id` del body. Si no coinciden, sobrescribe el body con el valor autenticado del token.

### 4.5 Servicios

#### 4.5.1 AuthService

| Método | Descripción |
|--------|-------------|
| `create_access_token(subject, token_type)` | Genera JWT firmado con HS256 |
| `decode_token(token)` | Valida y decodifica JWT, maneja expiración |
| `get_current_device(credentials)` | Dependencia FastAPI para endpoints de dispositivo |
| `get_current_user(credentials)` | Dependencia FastAPI para endpoints de usuario |
| `authenticate_device(auth_request)` | Valida device_id contra lista de .env (stub) |

#### 4.5.2 SensorService

| Método | Descripción |
|--------|-------------|
| `create_reading(reading)` | Inserta lectura en MongoDB con timestamp del servidor |
| `get_latest_readings(device_id, limit)` | Consulta las N lecturas más recientes |
| `get_readings_by_time_range(device_id, hours, limit)` | Historial filtrado por tiempo |
| `get_stats(device_id, hours)` | Agregaciones: avg, min, max (pipeline MongoDB) |

#### 4.5.3 ActuatorService

| Método | Descripción |
|--------|-------------|
| `send_command(command)` | Registra comando pendiente en MongoDB |
| `get_pending_commands(device_id)` | ESP32 consulta comandos no ejecutados |
| `mark_command_executed(command_id)` | ESP32 confirma ejecución |
| `report_actuator_state(state)` | ESP32 reporta estado real |
| `get_actuator_state(device_id, nombre)` | App consulta estado actual |

#### 4.5.4 AlertService (Detección de Inactividad y Anomalías)

| Método | Descripción | Criterio de Disparo |
|:---|:---|:---|
| `check_sensor_outliers(reading)` | Evalúa si las métricas recibidas violan gradientes térmicos abruptos (ej. $\Delta T > 10^\circ\text{C}$ en 5 s) o incoherencias físicas. | Genera registro `POST /events` nivel `alerta` |
| `evaluate_device_heartbeat(device_id)` | Tarea en background / cron que verifica `timestamp` de la última lectura en `sensor_readings`. | Dispara alerta `critico` si `now - last_seen > 30s` |
| `dispatch_critical_notification(event)` | Envía notificación push / webhook inmediato a los administradores ante eventos de falla. | Nivel `alerta` o `critico` |

### 4.6 Middleware

| Middleware | Función |
|-----------|---------|
| **CORS** | Permite orígenes configurables (dev: *, prod: dominios específicos) |
| **SecurityHeaders** | Añade X-Content-Type-Options, X-Frame-Options, HSTS, CSP |
| **RateLimiting** | 60 req/min por IP (públicos), 120 req/min (dispositivos) |

### 4.7 Manejo de Excepciones

| Excepción | Código HTTP | Escenario |
|-----------|-------------|-----------|
| `InvalidTokenException` | 401 | Token JWT inválido, expirado o tipo incorrecto |
| `DeviceNotAuthorizedException` | 403 | Device ID no está en la lista de autorizados |
| `SensorValidationException` | 422 | Datos de sensor fuera de rangos o malformados |
| `RateLimitExceededException` | 429 | Demasiadas peticiones desde una IP |
| `DatabaseConnectionException` | 503 | Fallo de conexión a MongoDB |
| `RequestValidationError` | 422 | Error de validación Pydantic (inputs maliciosos) |
| `Exception` genérica | 500 | Error interno no controlado (sin traceback en prod) |

---

## 5. Diseño Detallado del Firmware IoT

### 5.1 Hardware

| Componente | Modelo | Pin GPIO | Función |
|-----------|--------|----------|---------|
| Microcontrolador | ESP32 DevKit | — | Procesamiento + WiFi |
| Temp/Humedad | DHT22 | GPIO 4 | Temperatura y humedad relativa |
| Calidad de aire | MQ135 | GPIO 34 (ADC) | Detección de NH3/CO2 |
| Nivel de agua | HC-SR04 | TRIG: 13, ECHO: 35 | Distancia ultrasónica |
| Presencia | KY-032 | GPIO 33 | Sensor IR para puerta |
| Peso | HX711 | DT: 15, SCK: 16 | Celda de carga para tolva |
| Calefacción | Relé K1 | GPIO 32 | Control calefacción/bombillos IR |
| Ventilador | Relé K2 | GPIO 25 | Ventilación general |
| Extractor | Relé K3 | GPIO 27 | Extracción de gases |
| Bomba agua | Relé K4 | GPIO 14 | Bombeo de agua |
| Puerta | Servo SG90 | GPIO 2 | Apertura/cierre automático |
| Persiana | L293D Canal A | EN1:5, IN1:18, IN2:19 | Motor DC ventilación |
| Alimentador | L293D Canal B | EN2:21, IN3:22, IN4:23 | Motor tornillo sinfín |

### 5.2 Arquitectura de Software Embebido

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  Capa de Aplicación (main.cpp)                                              │
│  ├─ Máquina de estado global (INIT → CALIBRATION → MONITORING → ERROR)    │
│  ├─ Orquestación de tareas FreeRTOS                                        │
│  └─ Watchdog Timer (10s timeout)                                           │
└─────────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│  Capa de Servicios (GestorActuadores)                                       │
│  ├─ Lógica de control con umbrales (temp, hum, gases, nivel agua)         │
│  ├─ Histéresis de bomba (ON >6cm, OFF ≤3cm)                              │
│  └─ Fail-safe coordinado (desactiva todo, bomba ON emergencia)            │
└─────────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│  Capa de Dispositivos (Sensores + Actuadores individuales)                 │
│  ├─ SensorDHT, SensorMQ135, SensorUltrasonico, SensorKY032, SensorPeso   │
│  ├─ ControlServo (FSM: CERRADA→ABRIENDO→ABIERTA→CERRANDO→CERRADA)       │
│  ├─ Persiana (FSM: QUIETA→ABRIENDO→PAUSA→CERRANDO→QUIETA)               │
│  ├─ Alimentador (FSM: APAGADO→ENCENDIDO→APAGADO, ciclo 5min)            │
│  └─ Actuador (clase base para relés K1-K4)                               │
└─────────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│  Capa de Periféricos (GPIO, ADC, PWM, I2C)                                │
│  ├─ ESP32 pins, interfaces hardware                                        │
│  └─ Filtro de Media Móvil (template C++, buffer circular)                 │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 5.3 Máquinas de Estado Finitas (FSM)

#### 5.3.1 FSM Global del Sistema

**Estados:** INIT → CALIBRATION → MONITORING ↔ ERROR → SHUTDOWN

**Variables de entrada:**
- C: Contador de ciclos de arranque (≥ 10)
- T: Calibración HX711 completada
- F: Fallos acumulados ≥ 3
- R: Comando de rearme manual

**Salidas:**
- 00: Standby
- 01: Operación normal
- 10: Fail-safe activo

| Estado Actual | Entradas | Estado Siguiente | Salida | Acción |
|---------------|----------|------------------|--------|--------|
| INIT | C=0 | INIT | 00 | Espera estabilización |
| INIT | C=1 | CALIBRATION | 00 | Inicia tara HX711 |
| CALIBRATION | T=0 | CALIBRATION | 00 | Espera tara o timeout 15s |
| CALIBRATION | T=1 | MONITORING | 01 | Habilita lazo de control |
| MONITORING | F=0 | MONITORING | 01 | Muestreo cada 2s |
| MONITORING | F=1 | ERROR | 10 | Disparo fail-safe |
| ERROR | R=0 | ERROR | 10 | Mantiene posición segura |
| ERROR | R=1 | INIT | 00 | Rearme del sistema |

#### 5.3.2 FSM de Puerta (ControlServo)

**Estados:** CERRADA → ABRIENDO → ABIERTA → CERRANDO → CERRADA

**Entradas:**
- P: Presencia detectada (LOW)
- T300: Timer 300ms cumplido
- T2s: Timer 2s cumplido

**Salidas (posición servo):**
- 00: Neutro (93°)
- 01: Giro apertura (0°)
- 10: Giro cierre (180°)

| Estado | Entradas | Siguiente | Salida | Acción |
|--------|----------|-----------|--------|--------|
| CERRADA | P=1 | CERRADA | 00 | Sin presencia, reposo |
| CERRADA | P=0 | ABRIENDO | 01 | Presencia detectada |
| ABRIENDO | T300=0 | ABRIENDO | 01 | Continúa giro |
| ABRIENDO | T300=1 | ABIERTA | 00 | Detiene en neutro |
| ABIERTA | T2s=0 | ABIERTA | 00 | Espera paso |
| ABIERTA | T2s=1 | CERRANDO | 10 | Inicia cierre |
| CERRANDO | T300=0 | CERRANDO | 10 | Continúa giro |
| CERRANDO | T300=1 | CERRADA | 00 | Cierre completado |

#### 5.3.3 FSM de Persiana (L293D Canal A)

**Estados:** QUIETA → ABRIENDO → PAUSA → CERRANDO → QUIETA

**Ciclo:** Cada 5 minutos, apertura 3s → pausa 500ms → cierre 3s

| Estado | Entradas | Siguiente | Salida | Acción Motor |
|--------|----------|-----------|--------|--------------|
| QUIETA | T5m=0 | QUIETA | 00 | Reposo |
| QUIETA | T5m=1 | ABRIENDO | 01 | Giro apertura |
| ABRIENDO | T3s=0 | ABRIENDO | 01 | Desplazamiento |
| ABRIENDO | T3s=1 | PAUSA | 00 | Frena motor |
| PAUSA | T500=0 | PAUSA | 00 | Amortiguación |
| PAUSA | T500=1 | CERRANDO | 10 | Giro cierre |
| CERRANDO | T3s=0 | CERRANDO | 10 | Desplazamiento |
| CERRANDO | T3s=1 | QUIETA | 00 | Fin recorrido |

#### 5.3.4 FSM del Alimentador (L293D Canal B)

**Estados:** APAGADO ↔ ENCENDIDO

**Ciclo:** Cada 5 minutos, encendido al 50% PWM durante 5 minutos

| Estado | Entradas | Siguiente | Salida | Acción |
|--------|----------|-----------|--------|--------|
| APAGADO | T5m=0 | APAGADO | 0 | Espera ciclo |
| APAGADO | T5m=1 ∧ E=1 | ENCENDIDO | 1 | Arranca motor 50% |
| ENCENDIDO | T5m=0 ∧ E=1 | ENCENDIDO | 1 | Dosificando |
| ENCENDIDO | T5m=1 ∨ E=0 | APAGADO | 0 | Detiene motor |

### 5.4 Lógica de Control de Actuadores (GestorActuadores)

**Umbrales configurables (config.h):**
- `TEMP_FRIO = 27.0°C` → Activa calefacción
- `TEMP_CALOR = 32.0°C` → Activa ventilación forzada
- `HUM_EXTRACTORES = 65.0%` → Activa extractor
- `NH3_ALTO = 1500` → Ventilación de emergencia
- `NH3_MODERADO = 800` → Nivel de advertencia
- `NIVEL_BOMBA_ON = 6.0 cm` → Bomba ON (tanque bajo)
- `NIVEL_BOMBA_OFF = 3.0 cm` → Bomba OFF (tanque lleno)

**Lógica de decisión:**
```
gasesAltos = (rawNH3 >= NH3_ALTO)
activarCalefaccion = !gasesAltos && (temperatura < TEMP_FRIO)
activarVentilacion = !activarCalefaccion && (
    temperatura >= TEMP_CALOR ||
    humedad > HUM_EXTRACTORES ||
    gasesAltos
)
activarBomba = histéresis(distanciaAgua, NIVEL_BOMBA_ON, NIVEL_BOMBA_OFF)
```

**Histéresis de bomba:**
- ON si distancia > 6.0 cm
- OFF si distancia ≤ 3.0 cm
- Entre 3.0 y 6.0 cm: mantiene estado anterior
- En error de sensor: mantiene último estado conocido

### 5.5 Filtro de Media Móvil

**Implementación:** Template C++ con buffer circular

```cpp
template <typename T, uint16_t SIZE = 10>
class MovingAverage {
    T add(T value);      // Agrega valor, retorna promedio
    T getAverage();      // Promedio actual
    bool isFilled();     // Buffer lleno
};
```

**Aplicación:**
| Sensor | Tamaño Buffer | Latencia | Efecto |
|--------|--------------|----------|--------|
| MQ135 | 10 muestras | ~20ms | Estabiliza lectura gaseosa |
| HC-SR04 | 10 muestras | ~20ms | Elimina picos de distancia |

### 5.6 Watchdog Timer (WDT)

**Configuración:**
- Timeout: 10 segundos
- Panic: true (genera core dump + reinicio)
- Monitoreo: Solo tareaGalpon() en Core 0

**Flujo de protección:**
```
Ciclo normal: esp_task_wdt_reset() ✓
Ciclo con deadlock: WDT cuenta 1s...2s...10s → TIMEOUT
→ Genera panic → Stack trace → Reinicio automático ESP32
```

### 5.7 Distribución FreeRTOS Dual Core

| Tarea | Core | Prioridad | Stack | Período | Función |
|-------|------|-----------|-------|---------|---------|
| `tareaGalpon` | 0 | 2 | 16KB | ~10ms yield | Sensores, actuadores, FSMs, WDT reset |
| `tareaWiFi` | 1 | 1 | 4KB | 1000ms yield | Conexión WiFi, reconexión automática |
| `loop()` | — | 0 (Idle) | — | 1000ms | Vacío, scheduler gestiona los cores |

**Principio clave:** `vTaskDelay()` en lugar de `delay()` bloqueante para permitir yield al scheduler.

### 5.8 Detección de Errores y Diagnóstico de Sensores

El firmware clasifica los errores de sensado en tres categorías antes de reportar la alerta:

1. **Fallo de Bus / Sin Respuesta (Hardware Error):**  
   Ocurre si el sensor no responde a los pulsos de inicio (ej. DHT22 timeout $\approx 2.25\text{ ms}$, HC-SR04 ECHO $> 30\text{ ms}$). Si se acumulan 3 fallos consecutivos, se marca `enError = true` y se despacha un payload al endpoint `/events`.
2. **Lectura Fuera de Rango Físico (Out of Range):**  
   Valores como temperatura $< -10^\circ\text{C}$ o $> 60^\circ\text{C}$, humedad $= 0\%$ o $100\%$ sostenida, o distancias $< 0.5\text{ cm}$ o $> 400\text{ cm}$. El dato se descarta del cálculo de lazos de control y se envía con bandera de advertencia.
3. **Picos de Ruido / Gradiente Excesivo:**  
   El filtro de media móvil circular (10 muestras) descarta lecturas cuyo delta supere 3 desviaciones estándar de la ventana actual.

### 5.9 Flujo de Envío de Alerta desde ESP32

Cuando el firmware detecta la falla:
1. Conmuta la máquina de estado global a `S3: ERROR` (Fail-Safe local).
2. Construye un mensaje de evento en formato JSON.
3. Emite una petición `POST /events` de alta prioridad en Core 1:

```json
{
  "device_id": "galpon_01",
  "tipo": "FALLA_SENSOR",
  "origen": "SensorDHT",
  "mensaje": "DHT22 desconectado o sin respuesta (3 timeouts seguidos)",
  "nivel": "critico",
  "metadata": {
    "fallos_consecutivos": 3,
    "ultimo_valor_valido": { "temperatura": 25.1, "humedad": 61.4 }
  }
}
```

---

## 6. Base de Datos

### 6.1 Esquema MongoDB

**Base de datos:** `avisens`

### 6.2 Colecciones

#### 6.2.1 `sensor_readings`

Almacena las lecturas periódicas enviadas por los ESP32.

```json
{
  "_id": ObjectId("..."),
  "device_id": "galpon_01",
  "temperatura": 24.5,
  "humedad": 65.0,
  "peso": 125.3,
  "obstaculo": false,
  "calidad_aire": 1200,
  "voltaje_aire": 0.96,
  "timestamp": ISODate("2026-08-30T20:00:00Z"),
  "received_at": ISODate("2026-08-30T20:00:01Z")
}
```

**Índices recomendados:**
- `{ device_id: 1, timestamp: -1 }` — Consultas por dispositivo y tiempo
- `{ timestamp: 1 }` — Para TTL (auto-eliminación de datos antiguos)

#### 6.2.2 `actuator_commands`

Comandos enviados desde las apps hacia los ESP32.

```json
{
  "_id": ObjectId("..."),
  "device_id": "galpon_01",
  "nombre": "calefactor",
  "modo": "MANUAL",
  "orden_manual": true,
  "queued_at": ISODate("2026-08-30T20:00:00Z"),
  "executed": false,
  "executed_at": null
}
```

#### 6.2.3 `actuator_states`

Estado actual reportado por los ESP32.

```json
{
  "_id": ObjectId("..."),
  "device_id": "galpon_01",
  "nombre": "calefactor",
  "estado": true,
  "modo": "MANUAL",
  "orden_manual": true,
  "updated_at": ISODate("2026-08-30T20:00:00Z")
}
```

#### 6.2.4 `events`

Registro inmutable de eventos del sistema.

```json
{
  "_id": ObjectId("..."),
  "device_id": "galpon_01",
  "tipo": "FALLA_SENSOR",
  "origen": "SensorDHT",
  "mensaje": "DHT22 no responde después de 3 intentos",
  "nivel": "alerta",
  "timestamp": ISODate("2026-08-30T20:00:00Z"),
  "received_at": ISODate("2026-08-30T20:00:01Z"),
  "metadata": { "intentos_fallidos": 3 }
}
```

### 6.3 Pipeline de Agregación (Estadísticas)

```javascript
db.sensor_readings.aggregate([
  { $match: { device_id: "galpon_01", timestamp: { $gte: new Date(Date.now() - 24*60*60*1000) } } },
  { $group: {
      _id: "$device_id",
      count: { $sum: 1 },
      avg_temperatura: { $avg: "$temperatura" },
      min_temperatura: { $min: "$temperatura" },
      max_temperatura: { $max: "$temperatura" },
      avg_humedad: { $avg: "$humedad" },
      min_humedad: { $min: "$humedad" },
      max_humedad: { $max: "$humedad" },
      avg_calidad_aire: { $avg: "$calidad_aire" }
  }}
])
```

---

## 7. Seguridad

### 7.1 Backend

| Medida | Implementación | Nivel |
|--------|---------------|-------|
| Validación de inputs | Pydantic con rangos físicos | Crítico |
| Sanitización device_id | Solo alfanumérico + `_` `-` | Crítico |
| JWT firmado | HS256 con clave de 256 bits | Crítico |
| Anti-spoofing | Comparación token device_id vs body device_id | Crítico |
| Rate limiting | 60 req/min por IP (SlowAPI) | Importante |
| Headers de seguridad | HSTS, CSP, X-Frame-Options | Importante |
| Usuario no-root en Docker | `USER avisens` | Buena práctica |
| Sin secrets en código | Todo vía `.env` | Crítico |
| CORS restringido | Orígenes configurables | Importante |
| NaN/Inf rejection | Previene corrupción de DB | Buena práctica |
| Límites de query | Max 1000-5000 registros | Importante |
| Multi-stage Docker | Imagen mínima, sin build tools | Buena práctica |

### 7.2 IoT / Firmware

| Medida | Implementación | Nivel |
|--------|---------------|-------|
| Watchdog Timer | Reinicio automático ante deadlock | Crítico |
| Fail-safe automático | Desactiva todo en error de sensores | Crítico |
| Validación de sensores | Rangos físicos, contador de fallos | Crítico |
| Histéresis en bomba | Evita oscilación ON/OFF | Importante |
| Filtro de media móvil | Reduce ruido en sensores analógicos | Importante |
| Máquinas de estado | Control predecible, sin bloqueos | Importante |
| FreeRTOS dual core | Aislamiento de tareas críticas | Importante |

### 7.3 Comunicación

| Aspecto | Estado Actual | Recomendación Producción |
|---------|--------------|--------------------------|
| Protocolo | HTTP (texto plano) | HTTPS obligatorio |
| Autenticación | JWT Bearer | JWT + mTLS o AWS IoT Core |
| Token dispositivo | 1 año de duración | Rotación periódica o tokens cortos |
| Device secret | Comparación en texto plano (stub) | Hash bcrypt en MongoDB |
| Revocación de tokens | No implementada | Blacklist en Redis/MongoDB |

---

## 8. Interfaces y APIs

### 8.1 API REST del Backend

| Método | Ruta | Auth | Descripción | Cliente |
|--------|------|------|-------------|---------|
| `POST` | `/auth/device/login` | Público | ESP32 obtiene token JWT | ESP32 |
| `POST` | `/auth/user/login` | Público | App obtiene token de usuario | App |
| `POST` | `/sensors/readings` | Device JWT | ESP32 envía datos de sensores | ESP32 |
| `GET` | `/sensors/readings` | User JWT | App consulta últimas lecturas | App |
| `GET` | `/sensors/readings/history` | User JWT | Historial por rango de tiempo | App |
| `GET` | `/sensors/stats` | User JWT | Estadísticas agregadas | App |
| `POST` | `/actuators/commands` | User JWT | App envía comando manual | App |
| `GET` | `/actuators/commands` | Device JWT | ESP32 lee comandos pendientes | ESP32 |
| `POST` | `/actuators/commands/{id}/executed` | Device JWT | ESP32 confirma ejecución | ESP32 |
| `POST` | `/actuators/state` | Device JWT | ESP32 reporta estado real | ESP32 |
| `GET` | `/actuators/state` | User JWT | App consulta estado actual | App |
| `POST` | `/events` | Device JWT | Registrar evento/alerta | ESP32 |
| `GET` | `/events` | User JWT | Consultar historial de eventos | App |
| `GET` | `/health` | Público | Health check para monitoreo | Docker/K8s |

### 8.2 Flujo de Comunicación ESP32 ↔ Backend

```
ESP32 (Core 0 / Core 1)                         Backend (FastAPI)
│                                                     │
│── POST /auth/device/login ─────────────────────────→│
│←── { access_token: "eyJ...", token_type: "bearer" }─│
│                                                     │
│  [Cada 5 segundos]                                  │
│── POST /sensors/readings + Bearer token ───────────→│
│   { device_id, temperatura, humedad, ... }          │
│←── { success: true, id: "..." }────────────────────│
│                                                     │
│  [Si sensor falla x3 consecutivos]                  │
│── POST /events (Device JWT) ───────────────────────→│
│   { tipo: "FALLA_SENSOR", nivel: "critico" }        │
│←── HTTP 201 Created ────────────────────────────────│
│                                                     │
│                                          [Evalúa regla de alerta]
│                                          [Notifica a App / Admin]
│                                                     │
│                                                     ▼
│                                                App Móvil
│                                                     │
│←── Push Notification / Polling GET /events ─────────│
│                                                     │
│  [Cada 10 segundos]                                 │
│── GET /actuators/commands + Bearer token ──────────→│
│←── [ { _id, nombre, modo, orden_manual } ]─────────│
│                                                     │
│  [Después de ejecutar comando]                      │
│── POST /actuators/commands/{id}/executed ─────────→│
│←── { success: true }────────────────────────────────│
│                                                     │
│  [Después de cambiar estado actuador]               │
│── POST /actuators/state + Bearer token ───────────→│
│   { device_id, nombre, estado, modo }               │
│←── { success: true }────────────────────────────────│
```

---

## 9. Calidad, Robustez y Manejo de Errores

### 9.1 Backend

**Logging estructurado:**
- Formato JSON en producción (parseable por ELK, Datadog)
- Formato legible en desarrollo (colores en consola)
- Nunca loguea tokens, contraseñas ni datos PII
- Incluye contexto: device_id, user_id, path, timestamps

**Manejo de excepciones:**
- Handlers globales para excepciones de negocio, validación y errores internos
- En producción: mensajes genéricos al cliente, detalles solo en logs
- En desarrollo: detalles completos para debugging

**Rate limiting:**
- Protección contra fuerza bruta en login
- Protección contra spam de datos del ESP32
- Límite: 60 req/min IP pública, 120 req/min para dispositivos

### 9.2 IoT / Firmware

**Robustez de sensores:**
- DHT22: 3 fallos consecutivos → error crítico → fail-safe
- HC-SR04: Timeout >30ms → estado TIMEOUT → mantiene último valor
- MQ135: Filtro de media móvil de 10 muestras
- HX711: Calibración manual vía serial (comando "TARA") o factor por defecto

**Robustez de actuadores:**
- Servo: timeout 300ms para evitar bloqueo mecánico
- Persiana: pausa 500ms entre cambio de dirección (protege L293D)
- Alimentador: habilitación general puede desactivar el ciclo

**Watchdog Timer:**
- Detecta deadlocks en tareaGalpon()
- Reinicio automático con core dump para debugging

---

## 10. Despliegue e Infraestructura

### 10.1 Docker Multi-Stage

**Fase 1 (Build):**
- Imagen base: `python:3.11-slim`
- Instala dependencias de build
- Compila/instala requirements

**Fase 2 (Runtime):**
- Imagen final mínima
- Usuario no-root (`avisens`)
- Sin herramientas de build
- Tamaño objetivo: ~150MB

### 10.2 Docker Compose

```yaml
services:
  backend:
    build: .
    container_name: avisens-backend
    restart: unless-stopped
    ports:
      - "8000:8000"
    env_file: .env
    environment:
      - ENVIRONMENT=production
      - LOG_FORMAT=json
    networks:
      - avisens-network
    deploy:
      resources:
        limits:
          cpus: '1.0'
          memory: 512M
        reservations:
          cpus: '0.25'
          memory: 128M
```

### 10.3 Variables de Entorno

| Variable | Descripción | Ejemplo |
|----------|-------------|---------|
| `MONGODB_URI` | URI de conexión MongoDB Atlas | `mongodb+srv://...` |
| `MONGODB_DB_NAME` | Nombre de la base de datos | `avisens` |
| `JWT_SECRET_KEY` | Clave secreta para firmar JWT (≥32 chars) | `secrets.token_hex(32)` |
| `JWT_ALGORITHM` | Algoritmo de firma | `HS256` |
| `JWT_ACCESS_TOKEN_EXPIRE_MINUTES` | Expiración token usuario | `30` |
| `JWT_DEVICE_TOKEN_EXPIRE_DAYS` | Expiración token dispositivo | `365` |
| `API_HOST` | Host del servidor | `0.0.0.0` |
| `API_PORT` | Puerto del servidor | `8000` |
| `API_WORKERS` | Workers Uvicorn | `1` |
| `ENVIRONMENT` | Entorno de ejecución | `development` / `production` |
| `RATE_LIMIT_PER_MINUTE` | Límite req/min (público) | `60` |
| `DEVICE_RATE_LIMIT_PER_MINUTE` | Límite req/min (dispositivo) | `120` |
| `CORS_ORIGINS` | Orígenes permitidos | `http://localhost:3000` |
| `AUTHORIZED_DEVICE_IDS` | IDs de ESP32 autorizados | `galpon_01,galpon_02` |
| `LOG_LEVEL` | Nivel de logging | `INFO` |
| `LOG_FORMAT` | Formato de logs | `json` / `text` |

---

## 11. Riesgos y Limitaciones

### 11.1 Riesgos de Seguridad

| Riesgo | Severidad | Mitigación Actual | Mitigación Futura |
|--------|-----------|-------------------|-------------------|
| Token de dispositivo de larga duración (1 año) | 🔴 Alta | HTTPS en prod | mTLS, rotación de tokens, tokens cortos + refresh |
| Device secret en texto plano (stub) | 🔴 Alta | Lista en .env | Hash bcrypt en MongoDB |
| Sin lista de revocación de tokens | 🟡 Media | — | Blacklist en Redis/MongoDB |
| Rate limiting en memoria (no compartido entre workers) | 🟡 Media | — | RedisBackend de SlowAPI |
| Sin HTTPS en ESP32 (versión actual) | 🔴 Alta | — | Implementar TLS con certificados ECDSA |

### 11.2 Limitaciones Técnicas

| Limitación | Impacto | Solución Propuesta |
|-----------|---------|-------------------|
| MongoDB Atlas M0 (512MB) | Llenado en ~1.5 meses (datos cada 5s) | Implementar TTL (auto-eliminación) o agregación |
| ESP32 RAM limitada (520KB) | HTTPS puede fallar o ser lento | Usar certificados ECDSA, proxy local HTTP→HTTPS |
| Sin reintentos con backoff en ESP32 | Pérdida de datos si API cae | Cola en SPIFFS + reintentos exponenciales |
| Sin particionamiento de datos | Queries lentas a escala | Time-series collection MongoDB 5.0+ |
| Auth de usuarios es stub | No hay registro/login real | Integrar con backend de autenticación de compañeros |
| Sin buffer circular en ESP32 | Pérdida de datos en desconexión | Implementar cola circular en SPIFFS |

---

## 12. Roadmap y Próximos Pasos

### 12.1 Backend

| Versión | Tarea | Prioridad |
|---------|-------|-----------|
| v1.1 | Implementar TTL en MongoDB para auto-borrar datos antiguos | Alta |
| v1.2 | Integrar autenticación real con backend de compañeros | Alta |
| v1.3 | Agregar tests con pytest (tests/ ya creado) | Media |
| v1.4 | Configurar GitHub Actions para CI/CD | Media |
| v1.5 | Implementar notificaciones push (FCM) | Baja |
| v1.6 | Agregar dashboard web con gráficas de tendencia | Baja |
| v2.0 | Soporte WebSockets para actualizaciones en tiempo real | Futuro |

### 12.2 IoT / Firmware

| Versión | Tarea | Prioridad |
|---------|-------|-----------|
| v7.1 | Testing con hardware real y calibración de sensores | Alta |
| v8.0 | Implementar `ServicioAPI.cpp` — HTTP REST hacia backend | Alta |
| v8.0 | MQTT — Publicar datos a broker (Home Assistant) | Media |
| v9.0 | API REST — Endpoints locales para control remoto | Media |
| v9.5 | OTA Updates — Actualización remota de firmware | Baja |
| v10.0 | Sensor de peso HX711 completamente funcional | Media |
| v10.0 | Control PID para regulación fina de temperatura | Futuro |
| v10.0 | Persiana adaptativa basada en luz solar | Futuro |

---

## 13. Apéndices

### Apéndice A: Códigos de Estado HTTP

| Código | Uso en AVÍSENS |
|--------|---------------|
| 200 OK | Respuesta exitosa (GET, POST) |
| 201 Created | Recurso creado (lectura, comando, evento) |
| 401 Unauthorized | Token inválido, expirado o faltante |
| 403 Forbidden | Dispositivo no autorizado |
| 422 Unprocessable Entity | Validación Pydantic fallida |
| 429 Too Many Requests | Rate limit excedido |
| 500 Internal Server Error | Error no controlado |
| 503 Service Unavailable | Base de datos no disponible |

### Apéndice B: Estructura de Topics MQTT (Futuro)

```
galpon/{device_id}/sensores/temperatura
galpon/{device_id}/sensores/humedad
galpon/{device_id}/sensores/gases
galpon/{device_id}/sensores/agua
galpon/{device_id}/sensores/peso
galpon/{device_id}/actuadores/k1/comando
galpon/{device_id}/actuadores/k1/estado
galpon/{device_id}/eventos/error
galpon/{device_id}/eventos/fallo_sensor
galpon/{device_id}/system/heartbeat
```

### Apéndice C: Referencias de Estándares

- **IEC 61508** — Safety Integrity Levels (SIL)
- **MISRA C:2012** — Guidelines for the use of the C language in critical systems
- **TRL 5** — Technology Readiness Level (Validación en entorno relevante)
- **OWASP Top 10** — Seguridad en aplicaciones web
- **ISO/IEC 25010** — Calidad de software y sistemas

### Apéndice D: Glosario de Términos Técnicos

| Término | Definición |
|---------|-----------|
| **ASGI** | Asynchronous Server Gateway Interface |
| **BSON** | Binary JSON, formato de serialización de MongoDB |
| **CSP** | Content Security Policy |
| **HSTS** | HTTP Strict Transport Security |
| **mDNS** | Multicast DNS, resolución de nombres en red local |
| **OTA** | Over-The-Air, actualización de firmware remota |
| **SPIFFS** | SPI Flash File System, sistema de archivos en flash del ESP32 |
| **TTL** | Time-To-Live, auto-eliminación de documentos en MongoDB |

---


