# Documento de Diseño de Software (SDD)
# AVÍSENS — Sistema IoT para Granjas Avícolas (Enfoque Edge & MQTT)

---

## Tabla de Contenidos

1. [Introducción](#1-introducción)
2. [Descripción General del Sistema y Alcance IoT](#2-descripción-general-del-sistema-y-alcance-iot)
3. [Arquitectura del Sistema y Flujo de Comunicación](#3-arquitectura-del-sistema-y-flujo-de-comunicación)
4. [Diseño Detallado del Firmware Embebido (ESP32)](#4-diseño-detallado-del-firmware-embebido-esp32)
5. [Lógica de Control Local y Algoritmos PID](#5-lógica-de-control-local-y-algoritmos-pid)
6. [Contrato de Integración y Parámetros MQTT (Backend ↔ IoT)](#6-contrato-de-integración-y-parámetros-mqtt-backend--iot)
7. [Manejo de Errores, Robustez y Fail-Safe](#7-manejo-de-errores-robustez-y-fail-safe)
8. [Estándar de Documentación Externa y Calidad de Código](#8-estándar-de-documentación-externa-y-calidad-de-código)
9. [Plan de Pruebas y Validación](#9-plan-de-pruebas-y-validación)
10. [Apéndices](#10-apéndices)

---

## 1. Introducción

### 1.1 Propósito
Este documento define el diseño técnico formal del subsistema **IoT / Edge** del proyecto **AVÍSENS**. Establece la arquitectura de hardware y firmware del microcontrolador ESP32, los lazos de control autónomos (incluyendo algoritmos PID), el esquema de comunicaciones mediante el protocolo MQTT y el estándar de documentación desacoplada para garantizar código limpio y mantenible.

### 1.2 Alcance
El alcance de este diseño se delimita estrictamente al desarrollo embebido y su frontera de comunicación:
- Adquisición de señales de sensores ambientales y físicos (DHT22, MQ-135, HX711, HC-SR04 de nivel de agua, sensor de presencia/obstáculo KY-032).
- Control de potencia mediante relés optoacoplados de lógica invertida para calefacción, ventilación, extracción forzada y bombeo de agua.
- Accionamiento de mecanismos mediante puente H L293D y servomotor: persiana de renovación de aire, tornillo sinfín de alimento y puerta automática de acceso.
- Lógica de control en lazo cerrado y operación autónoma en caso de fallo de red.
- Protocolo de enlace telemétrico bidireccional cliente-broker basado en MQTT.
- Esquema de integración hacia el Backend / Frontend (contrato de mensajería).
- Estándar de documentación técnica externa para el firmware.

*Nota de exclusión:* Se excluye la implementación interna de microservicios de backend (FastAPI, Django, MongoDB), diseño de interfaces móviles/web (Kotlin, React) y pasarelas de autenticación de usuario (JWT).

### 1.3 Definiciones y Acrónimos

| Término | Definición |
|:---|:---|
| **SDD** | Software Design Document (Documento de Diseño de Software) |
| **Edge Computing** | Procesamiento de control y filtrado directamente en el nodo sensor/actuador |
| **Broker MQTT** | Servidor central de enrutamiento y despacho de mensajes Publish/Subscribe |
| **Topic** | Cadena de caracteres jerárquica que actúa como canal de distribución en MQTT |
| **Payload** | Contenido útil transmitido en un paquete MQTT (serializado en JSON) |
| **QoS** | Quality of Service (Calidad de Servicio en entrega de paquetes MQTT) |
| **LWT** | Last Will and Testament (Mensaje de última voluntad emitido por desconexión no deseada) |
| **PID** | Proporcional - Integral - Derivativo (Control de lazo cerrado analógico/discreto) |
| **ADC** | Analog to Digital Converter (Conversor Analógico a Digital) |
| **WDT** | Watchdog Timer (Temporizador de reinicio de seguridad ante bloqueo) |

---

## 2. Descripción General del Sistema y Alcance IoT

### 2.1 Perspectiva del Producto
AVÍSENS opera como una solución modular de automatización avícola donde el ESP32 actúa como cerebro operativo del galpón. El dispositivo garantiza el bienestar animal manteniendo microclimas óptimos mediante lecturas periódicas y algoritmos de control local, publicando periódicamente su telemetría e informando su estado a una pasarela central (Broker MQTT) a la que acceden los servicios de backend y aplicaciones cliente.

```
                    AVÍSENS - Subsistema IoT
                               │
                ┌──────────────┴──────────────┐
                ▼                             ▼
       Sensores Ambientales          Actuadores de Potencia
       (DHT22, MQ-135, HX711,        (Calefactor, Ventilador,
        HC-SR04, Obstáculo IR)        Extractor, Bomba, Sinfín,
                                       Persiana, Puerta)
                │                             │
                └──────────► ESP32 ◄──────────┘
                               │
                       WiFi / MQTT (TCP/IP)
                               │
                               ▼
                          Broker MQTT
                               ▲
                               │  (Integración externa)
                        Backend Central
                               ▲
                               │
                     App Móvil / Web Admin
```

### 2.2 Funcionalidades Clave del Nodo IoT

| ID | Funcionalidad | Descripción | Modo Operativo |
|:---|:---|:---|:---|
| **F-IoT-01** | Telemetría Ambiental | Muestreo de temperatura, humedad relativa, gases (NH3/CO2), peso de tolva y presencia cada 5 s. | Autónomo |
| **F-IoT-02** | Control Térmico / PID | Modulación del ciclo útil de calefactores y extractores según consigna dinámica. | Auto / Manual |
| **F-IoT-03** | Control de Humedad | Extracción forzada por bandas de histéresis cuando la humedad relativa supera el umbral configurado. | Auto / Manual |
| **F-IoT-04** | Ventilación y Calidad de Aire | Purga automática del galpón ante acumulación de concentraciones de gas nocivo. | Prioritario |
| **F-IoT-05** | Dosificación de Alimento | Accionamiento temporizado o por comando remoto del tornillo sinfín / tolva. | Periódico / Remoto |
| **F-IoT-06** | Enlace MQTT Bidireccional | Publicación de telemetría, eventos críticos y consumo de tópicos de comando con QoS 1. | Conectado |
| **F-IoT-07** | Fail-Safe Desconectado | Continuidad total del control ambiental local si el broker o la red Wi-Fi no están disponibles. | Resiliente |
| **F-IoT-08** | Nivel de Agua | Accionamiento de la bomba por histéresis sobre la distancia medida con el HC-SR04. | Autónomo |
| **F-IoT-09** | Puerta Automática | Apertura del servomotor ante detección de presencia y cierre temporizado tras despejarse el paso. | Autónomo |
| **F-IoT-10** | Renovación de Aire | Ciclo periódico de apertura, pausa y cierre de la persiana sobre puente H. | Periódico |

---

## 3. Arquitectura del Sistema y Flujo de Comunicación

### 3.1 Flujo Integral de Comunicación: App ↔ Backend ↔ Broker ↔ ESP32
Para permitir el control remoto sin comprometer la seguridad de la red local del galpón ni exponer el ESP32 a direccionamiento IP público directo:

1. **Interacción de Usuario (App/Web):** El usuario pulsa un botón de activación o ajusta un setpoint. La aplicación emite una petición HTTP/WebSocket autenticada hacia el **Backend**.
2. **Validación y Publicación (Backend):** El backend valida credenciales, permisos y reglas de negocio, persiste el registro en base de datos y publica un payload JSON en el topic de comando correspondiente del **Broker MQTT**.
3. **Recepción y Ejecución (ESP32):** El ESP32, que mantiene una conexión TCP persistente hacia el Broker MQTT, recibe el paquete en su callback de suscripción, decodifica el payload y aplica el cambio al hardware (relés/PWM).
4. **Confirmación y Estado Real (ESP32 → App):** El microcontrolador publica de vuelta su estado consolidado. El backend escucha este cambio y notifica a la aplicación cliente para refrescar la interfaz.

```
┌──────────────┐                 ┌──────────────┐
│  App Móvil   │                 │   Backend    │
│  / Panel Web │                 │  (Servicios) │
└──────┬───────┘                 └──────┬───────┘
       │                                │
       │ 1. POST /actuador/on (HTTP)    │
       ├───────────────────────────────►│
       │                                │ 2. Valida y publica
       │                                │    avisens/.../set (QoS 1)
       │                                ├────────────────────────┐
       │                                │                        ▼
       │                                │                 ┌──────────────┐
       │                                │                 │ Broker MQTT  │
       │                                │                 │(Mosquitto/EMQ│
       │                                │                 └──────┬───────┘
       │                                │                        │
       │                                │                        │ 3. Forward MQTT
       │                                │                        ▼
       │                                │                 ┌──────────────┐
       │                                │                 │    ESP32     │
       │                                │                 │ (Firmware)   │
       │                                │                 └──────┬───────┘
       │                                │                        │ 4. Acciona relé
       │                                │ 5. Publica nuevo       │
       │                                │    estado real         ▼
       │                                │◄───────────────────────┘
       │ 6. Actualiza UI (WebSocket/SSE)│
       │◄───────────────────────────────┤
```

---

## 4. Diseño Detallado del Firmware Embebido (ESP32)

### 4.1 Asignación de Pines Hardware (Pinout)

Fuente normativa de esta tabla: `proyecto_iot/include/config.h`.

| Componente | Tipo | Pin GPIO | Modo / Señal | Parámetros de Operación |
|:---|:---|:---|:---|:---|
| **DHT22** | Sensor | GPIO 4 | Digital One-Wire | Temp: -40 a 80 °C, Hum: 0 a 100% |
| **MQ-135** | Sensor | GPIO 34 | Analógico (ADC1_CH6) | Solo entrada. ADC1 para permanecer operativo con Wi-Fi activo |
| **HC-SR04 (TRIG)** | Sensor | GPIO 13 | Digital Output | Pulso de disparo de 10 µs |
| **HC-SR04 (ECHO)** | Sensor | GPIO 35 | Digital Input | Solo entrada. Requiere divisor 5 V → 3.3 V |
| **HX711 (DT)** | Sensor | GPIO 15 | Digital (Data) | Celda de carga tolva de alimento. Pin de strapping (MTDO) |
| **HX711 (SCK)** | Sensor | GPIO 16 | Digital (Reloj) | Pulsos de sincronismo. Libre solo en módulos sin PSRAM |
| **Obstáculo IR (KY-032)** | Sensor | GPIO 33 | Digital Input (pull-up interno) | LOW: Detección, HIGH: Despejado |
| **Calefactor** | Actuador | GPIO 32 | Digital Output | Relé K1 optoacoplado, activo en LOW |
| **Ventilador** | Actuador | GPIO 25 | Digital Output | Relé K2 optoacoplado, activo en LOW |
| **Extractor** | Actuador | GPIO 27 | Digital Output | Relé K3 optoacoplado, activo en LOW |
| **Bomba de agua** | Actuador | GPIO 14 | Digital Output | Relé K4 optoacoplado, activo en LOW |
| **Persiana (EN1/IN1/IN2)** | Actuador | GPIO 5 / 18 / 19 | L293D canal A | EN1 es pin de strapping y emite pulso en el arranque |
| **Alimentador (EN2/IN3/IN4)** | Actuador | GPIO 21 / 22 / 23 | L293D canal B (PWM) | Motor sinfín al 50 % de ciclo útil |
| **Puerta (Servo)** | Actuador | GPIO 2 | PWM | Pin de strapping y LED integrado. 0° cerrada, 90° abierta |

**Consideraciones eléctricas de obligado cumplimiento:**

- **Lógica de relés invertida.** Los cuatro módulos de relé son activos en `LOW`: `LOW` energiza la bobina y `HIGH` la libera. El estado seguro es `HIGH`.
- **Pull-up de arranque.** En el reset los GPIO quedan en alta impedancia, por lo que K1-K4 permanecen indefinidos hasta que el firmware ejecuta su inicialización. Se exigen resistencias de pull-up de 10 kΩ a 3.3 V en las cuatro líneas de control para garantizar el estado apagado durante el arranque.
- **Pines de strapping.** GPIO 2, 5 y 15 condicionan el arranque del ESP32. GPIO 5 emite un pulso durante el boot que puede provocar un movimiento breve de la persiana.
- **JTAG no disponible.** GPIO 13, 14 y 15 están ocupados, de modo que la depuración se realiza exclusivamente por puerto serie.

### 4.2 Filtrado y Acondicionamiento de Señales
- **Filtro de Media Móvil (Moving Average):** Aplicado sobre el canal analógico del MQ-135 con ventana circular de 10 muestras para filtrar oscilaciones de red y ruido electromagnético de los motores:
  $$V_{\text{filtrado}} = \frac{1}{N} \sum_{i=0}^{N-1} V_{\text{raw}}[i]$$
- **Rechazo de Dispersión en Peso (HX711):** Muestreo de 5 lecturas de la celda de carga con descarte de extremos y valor promedio central, transformando a masa mediante constantes de tara y factor de escala calibrados en flash NVS:
  $$\text{Peso (g)} = \frac{\text{LecturaActual} - \text{Tara}}{\text{FactorEscala}}$$
- **Debounce por Software:** Validación en la señal de presencia de obstáculo de 50 ms continuos para prevenir falsos disparos por interferencias ópticas transitorias.

---

## 5. Lógica de Control Local y Algoritmos PID

### 5.1 Ecuación Discreta del Controlador PID
Para actuadores que requieren estabilización térmica precisa (ej. calefacción resistiva modulada por relé de estado sólido o control variable de extractores), se implementa un algoritmo PID en tiempo discreto:

$$e[k] = \text{SP} - \text{PV}[k]$$

$$u[k] = K_p \, e[k] + K_i \, T_s \sum_{j=0}^{k} e[j] + K_d \, \frac{e[k] - e[k-1]}{T_s}$$

Donde:
- $\text{SP}$: Setpoint o valor consigna deseado.
- $\text{PV}[k]$: Variable del proceso medida (temperatura o humedad).
- $e[k]$: Error de seguimiento instantáneo.
- $K_p, K_i, K_d$: Constantes de ganancia del lazo cerrado.
- **Anti-Windup Integral:** Se satura la suma acumulada de error dentro del intervalo $[0, 100]\%$ para evitar sobreimpulsos tras transitorios de arranque.
- **Ventana de Tiempo PWM (Time-Proportioning PWM):** Para relés mecánicos o electromecánicos, el porcentaje de salida $u[k]$ se mapea a una ventana de ciclo útil temporal fija de 10 segundos (ej. $60\%$ = 6 s encendido, 4 s apagado) minimizando el desgaste físico de los contactos.

### 5.2 Estrategia de Autonomía
Si la bandera `mqtt_connected == false` o `wifi_connected == false`:
1. El ESP32 conmuta inmediatamente a modo **Local Fallback**.
2. Continúa ejecutando el lazo de control con los últimos Setpoints válidos almacenados en memoria no volátil (NVS/Flash).
3. Los datos de telemetría prioritarios se colocan en un buffer temporal en memoria RAM hasta que el enlace sea restablecido.

---

## 6. Contrato de Integración y Parámetros MQTT (Backend ↔ IoT)

Esta sección define formalmente el contrato de interfaz necesario para que el equipo de Backend y Servicios configure sus escuchas y despachadores:

### 6.1 Parámetros de Conexión al Broker

| Parámetro | Valor / Especificación | Notas |
|:---|:---|:---|
| **Protocolo** | MQTT v3.1.1 / v5.0 | TCP estándar |
| **Puerto Estándar (No TLS)** | `1883` | Entorno de desarrollo local / pruebas |
| **Puerto Seguro (TLS/SSL)** | `8883` | Obligatorio para producción |
| **Keep-Alive** | `15` segundos | El ESP32 enviará PINGREQ si no hay tráfico |
| **Esquema de Client ID** | `esp32_galpon_{id}` | Ej: `esp32_galpon_01` (Único por dispositivo) |
| **Formato de Payloads** | JSON UTF-8 estricto | Serializado mediante ArduinoJson |
| **Codificación Numérica** | Floats con 1 o 2 decimales | Evitar strings en valores analógicos |

### 6.2 Jerarquía de Topics

Convención raíz: `avisens/{device_id}/...`  
*(Donde `{device_id}` corresponde al identificador del galpón, ej. `galpon_01`).*

```
avisens/{device_id}/
├── telemetria/
│   ├── dht22                  [ESP32 -> Backend] Lectura Higrotérmica
│   ├── gases                  [ESP32 -> Backend] Calidad de Aire / NH3
│   ├── peso                   [ESP32 -> Backend] Celda de Carga / Tolva
│   ├── obstaculo              [ESP32 -> Backend] Sensor de proximidad
│   ├── nivel_agua             [ESP32 -> Backend] Distancia HC-SR04 / Estado bomba
│   └── diagnostico            [ESP32 -> Backend] Heap, RSSI, Fallas
├── actuadores/
│   ├── {actuador}/set         [Backend -> ESP32] Comando de Control
│   ├── config/pid             [Backend -> ESP32] Calibración de Setpoints
│   └── estado                 [ESP32 -> Backend] Telemetría de Estado Real
└── status/
    └── lwt                    [ESP32 -> Broker -> Backend] Presencia Online/Offline
```

### 6.3 Esquemas de Payloads JSON

#### A. Publicaciones del ESP32 hacia el Backend (Telemetría)

##### Lectura Higrotérmica (`telemetria/dht22`)
- **Frecuencia:** Cada 5 segundos.
- **QoS:** 1. Retain: `false`.
```json
{
  "device_id": "galpon_01",
  "uptime_ms": 86400000,
  "temperatura": 25.4,
  "humedad": 63.2,
  "sensor_ok": true
}
```
*Nota:* el nodo **no dispone de RTC ni cliente NTP**, por lo que no puede generar una marca de tiempo absoluta. Se publica `uptime_ms` (milisegundos desde el arranque) y el sellado temporal corresponde al backend. `millis()` desborda a los ~49 días de operación continua.

##### Calidad del Aire (`telemetria/gases`)
- **Frecuencia:** Cada 5 segundos.
- **QoS:** 1. Retain: `false`.
```json
{
  "device_id": "galpon_01",
  "raw_adc": 1250,
  "voltaje": 1.01,
  "nivel": "MODERADO",
  "alerta_gas": false
}
```
*Campos:*
- `nivel`: `"NORMAL"` (`raw_adc < 800`) | `"MODERADO"` (`800 ≤ raw_adc < 1500`) | `"ALTO"` (`raw_adc ≥ 1500`).
- `alerta_gas`: `true` cuando el nivel es `ALTO`, condición que dispara ventilación prioritaria.

*Nota:* no se publica una estimación en ppm. Obtenerla exige calibrar la resistencia de referencia $R_0$ del MQ-135 en aire limpio y aplicar la curva $R_s/R_0$ del fabricante; mientras esa calibración no se realice, el contrato expone únicamente el valor crudo filtrado y su clasificación.

##### Peso de Tolva (`telemetria/peso`)
- **Frecuencia:** Cada 10 segundos.
- **QoS:** 1. Retain: `false`.
```json
{
  "device_id": "galpon_01",
  "peso_gramos": 3420.5,
  "tolva_vacia": false
}
```

##### Sensor de Obstáculo (`telemetria/obstaculo`)
- **Frecuencia:** Por evento (inmediato al detectar cambio de estado).
- **QoS:** 1. Retain: `false`.
```json
{
  "device_id": "galpon_01",
  "detectado": true,
  "evento": "INGRESO_DETECTADO"
}
```

##### Nivel de Agua (`telemetria/nivel_agua`)
- **Frecuencia:** Cada 10 segundos.
- **QoS:** 1. Retain: `false`.
```json
{
  "device_id": "galpon_01",
  "distancia_cm": 4.2,
  "estado_sensor": "OK",
  "bomba_activa": false
}
```
*Campos:*
- `estado_sensor`: `"OK"` | `"TIMEOUT"` | `"OUT_OF_RANGE"` | `"ERROR"`.
- `bomba_activa`: estado real del relé K4 tras aplicar la histéresis.

##### Diagnóstico y Salud (`telemetria/diagnostico`)
- **Frecuencia:** Cada 60 segundos.
- **QoS:** 0. Retain: `false`.
```json
{
  "device_id": "galpon_01",
  "free_heap": 184520,
  "wifi_rssi": -65,
  "uptime_segundos": 86400,
  "modo_operativo": "AUTONOMO_OK"
}
```

---

#### B. Comandos del Backend hacia el ESP32 (Control de Actuadores)

Los actuadores `{actuador}` disponibles se agrupan según su etapa de potencia:

| Literal | Etapa | Elemento físico |
|:---|:---|:---|
| `calefactor` | Relé K1 | Bombillos infrarrojos / resistencia |
| `ventilador` | Relé K2 | Ventilador de recirculación |
| `extractor` | Relé K3 | Extractor de aire |
| `bomba` | Relé K4 | Bomba de agua |
| `alimentador` | L293D canal B | Tornillo sinfín |
| `persiana` | L293D canal A | Persiana de renovación |
| `puerta` | Servomotor | Puerta automática de acceso |

> **Nota sobre `humidificador`.** El galpón no dispone de humidificador: el elemento instalado en K2 es un ventilador. El literal canónico es `ventilador`; el firmware acepta `humidificador` como alias en desuso para no romper integraciones existentes, pero el backend debe migrar al nombre canónico.

##### Comando Directo (`actuadores/{actuador}/set`)
- **Publicador:** Backend (en respuesta a orden de la App Móvil/Web).
- **QoS:** 1. Retain: `false`.
```json
{
  "modo": "MANUAL",
  "estado": true,
  "duracion_segundos": 120
}
```
*Campos:*
- `modo`: `"AUTO"` (el lazo interno PID/umbrales retoma el control) o `"MANUAL"` (forzado).
- `estado`: `true` (relé activo), `false` (relé apagado).
- `duracion_segundos`: Límite para retornar automáticamente a modo `"AUTO"` (prevención de olvido).

##### Ajuste Remoto de Setpoints y PID (`actuadores/config/pid`)
- **Publicador:** Backend.
- **QoS:** 1. Retain: `true`.
```json
{
  "setpoint_temp": 28.0,
  "setpoint_hum": 60.0,
  "kp_temp": 4.5,
  "ki_temp": 0.2,
  "kd_temp": 1.1
}
```

---

#### C. Confirmación de Estado del ESP32 (`actuadores/estado`)
Cada vez que un actuador cambia de estado (manual o automático), el ESP32 notifica el estado consolidado:
- **Publicador:** ESP32.
- **QoS:** 1. Retain: `true`.
```json
{
  "device_id": "galpon_01",
  "calefactor": { "estado": false, "modo": "AUTO", "potencia_pct": 0.0 },
  "ventilador": { "estado": true, "modo": "AUTO", "potencia_pct": 45.0 },
  "extractor": { "estado": false, "modo": "AUTO", "potencia_pct": 0.0 },
  "bomba": { "estado": false, "modo": "AUTO" },
  "alimentador": { "estado": false, "modo": "MANUAL", "bloqueado": false },
  "persiana": { "estado_fsm": "QUIETA", "modo": "AUTO" },
  "puerta": { "estado_fsm": "CERRADA", "modo": "AUTO" }
}
```
*Campos:*
- `estado`: estado real del relé ya aplicado, no la orden recibida.
- `estado_fsm`: para mecanismos temporizados, el estado actual de su máquina de estados. Valores de `persiana`: `QUIETA` | `ABRIENDO` | `PAUSA` | `CERRANDO`. Valores de `puerta`: `CERRADA` | `ABRIENDO` | `ABIERTA` | `CERRANDO`.
- `bomba` no expone `potencia_pct` porque su accionamiento es todo o nada por histéresis.

---

#### D. Mensaje de Última Voluntad (LWT - Last Will and Testament)
- **Tópico LWT:** `avisens/galpon_01/status/lwt`
- **Configuración:** Retain: `true`, QoS: 1.
- **Payload al Conectar (ESP32):**
  ```json
  {"status": "ONLINE", "ip": "192.168.1.50"}
  ```
- **Payload al Desconectar Abruptamente (Broker):**
  ```json
  {"status": "OFFLINE", "causa": "DESCONEXION_INESPERADA"}
  ```

---

## 7. Manejo de Errores, Robustez y Fail-Safe

1. **Watchdog Hardware (WDT):** Temporizador de 10 segundos configurado en el bucle de procesamiento del microcontrolador. En caso de colapso en el stack de tareas o ejecución infinita, el microcontrolador genera un volcado de registro y reinicio en caliente.
2. **Diagnóstico y Aislamiento de Sensores:** 
   - Fallo en lectura de un sensor crítico —DHT22 o HC-SR04— ($N \ge 3$ lecturas inválidas consecutivas): se desactiva la variable correspondiente del lazo de control, el sistema transita al estado `ERROR` y ejecuta el protocolo de Fail-Safe de Potencia descrito en el punto 3. El fallo se notifica en `avisens/{device_id}/telemetria/diagnostico`. La recuperación es automática en cuanto ambos sensores vuelven a entregar lecturas válidas.
   - Sensor de presencia trabado: Si el pin permanece en estado bajo continuo por más de 120 segundos, se inhabilita el actuador dependiente reportando alarma preventiva.
3. **Fail-Safe de Potencia:** Ante cualquier condición clasificada como crítica, el nodo aplica la siguiente configuración de seguridad:
   - **K1 Calefactor, K2 Ventilador y K3 Extractor:** desactivados (línea en `HIGH`), eliminando el riesgo de sobrecalentamiento.
   - **K4 Bomba:** activada (línea en `LOW`), asegurando el suministro de agua mientras dure la contingencia.
   - **Puerta:** cerrada por orden de emergencia, conteniendo a los animales dentro del galpón.
   - **Persiana y tornillo sinfín:** detenidos, dejando sin energía todo mecanismo en movimiento.

   Puesto que los módulos de relé son activos en `LOW`, el estado lógico de seguridad de las líneas de control es `HIGH`. En un reinicio de hardware los GPIO quedan en alta impedancia antes de que el firmware los inicialice, de modo que la garantía de apagado durante el arranque recae en las resistencias de pull-up externas exigidas en §4.1.

---

## 8. Estándar de Documentación Externa y Calidad de Código

Para garantizar un código fuente limpio, legible y de nivel profesional en C++ / Arduino / PlatformIO, se prohíbe el uso de comentarios extensos en el código y se establece la documentación desacoplada en manuales externos de arquitectura y módulo.

### 8.1 Filosofía: Código Autodocumentado y Sin Saturación
El firmware debe estructurarse de tal manera que su intención técnica sea transparente sin necesidad de párrafos explicativos intercalados en la lógica:
- **Nombres Explícitos:** Variables, estructuras y funciones con nombres legibles que describan acción y magnitud (ej. `calcularSalidaPidTemperatura()`, `tiempoUltimaTelemetriaMs`).
- **Constantes Tipadas:** Prohibición de valores mágicos ("magic numbers"). Todo umbral debe definirse como `constexpr` o directiva semántica en archivo de configuración.
- **Separación de Responsabilidades:** Clases dedicadas por sensor/actuador (`SensorDHT`, `SensorMQ135`, `ControladorPID`, `ClienteMQTT`).

### 8.2 Reglas Estrictas de Comentarios en Código

| Tipo de Comentario | Estado | Directriz Permitida |
|:---|:---|:---|
| **Bloques explicativos largos** | ❌ **PROHIBIDO** | Trasladar a la carpeta `docs/` del repositorio. |
| **Tutoriales y teoría de sensores** | ❌ **PROHIBIDO** | La explicación funcional va en el manual externo. |
| **Código muerto comentado** | ❌ **PROHIBIDO** | Se elimina del archivo; el historial se gestiona con Git. |
| **Doxygen en headers (`.h`)** | ⚠️ **MÍNIMO** | Máximo 2 líneas por función pública: `@brief`, `@param`, `@return`. |
| **Comentarios inline (`.cpp`)** | ⚠️ **MÍNIMO** | Exclusivamente para indicar consideraciones de hardware crítico o workaround de silicio. |

*Ejemplo de lo permitido en encabezados (`.h`):*
```cpp
/**
 * @brief Ejecuta el lazo de control PID térmico y computa la modulación PWM.
 * @param tempActual Temperatura actual de lectura en grados Celsius.
 * @return float Porcentaje de salida acotado en el intervalo [0.0, 100.0].
 */
float computarControl(float tempActual);
```

*Ejemplo de lo permitido en implementación (`.cpp`):*
```cpp
// Retardo requerido de 2ms según hoja de datos del DHT22 tras wakeup
vTaskDelay(pdMS_TO_TICKS(2));
```

### 8.3 Estructura del Repositorio y Documentación Externa
La documentación funcional completa del firmware residirá en archivos Markdown independientes dentro del directorio `docs/`:

```
avisens-firmware/
├── docs/
│   ├── 01_arquitectura_freertos.md    # Asignación de Cores, Tareas, Colas y WDT
│   ├── 02_sensores_calibracion.md     # Fórmulas de tara HX711, curvas MQ-135
│   ├── 03_control_pid_actuadores.md   # Modelo matemático de lazo cerrado y PWM
│   ├── 04_protocolo_mqtt_interfaz.md  # Contrato de datos y tópicos (Copia de ICD)
│   └── 05_maquinas_de_estado.md       # Tablas de transición FSM de relés y sistema
├── include/                           # Archivos de cabecera (.h) limpios
├── src/                               # Implementaciones (.cpp) directas y concisas
└── platformio.ini
```

### 8.4 Plantilla Estándar para Módulos Externos
Cada documento dentro de `/docs` debe estructurarse con la siguiente plantilla:

```markdown
# [Nombre del Módulo o Periférico]

## 1. Propósito y Responsabilidad
(Descripción concisa de qué resuelve el módulo).

## 2. Conexión Hardware y Pinout
| Pin Físico | Pin ESP32 | Función Eléctrica | Pull-Up / Pull-Down |
|---|---|---|---|

## 3. Modelo Matemático / Lógica
(Fórmulas LaTeX, ecuaciones de filtrado o tablas FSM de Mealy/Moore).

## 4. Parámetros de Configuración
| Constante | Valor por Defecto | Archivo de Origen | Descripción |
|---|---|---|---|

## 5. Manejo de Errores y Fail-Safe
(Comportamiento del módulo ante desconexión o fallo físico).
```

---

## 9. Plan de Pruebas y Validación

| ID Prueba | Escenario de Prueba | Criterio de Éxito |
|:---|:---|:---|
| **TC-01** | Transmisión periódica a Broker | Recepción continua de telemetría cada 5 s en el topic acordado sin desbordamiento de memoria. |
| **TC-02** | Caída y recuperación de Wi-Fi | El ESP32 reintenta reconexión de forma no bloqueante y mantiene el control térmico localmente sin congelar actuadores. |
| **TC-03** | Comando remoto manual | Publicación desde backend de comando ON para extractor; el relé conmuta en $< 200\text{ ms}$. |
| **TC-04** | Disparo de LWT (Desconexión abrupta) | Desconexión forzada de alimentación del ESP32; el Broker publica automáticamente el JSON de estado `offline` en el topic acordado. |
| **TC-05** | Validación de código limpio | Análisis estático y revisión de código: cero comentarios extensos o código huérfano dentro de los fuentes `src/`. |

---

## 10. Apéndices

### Apéndice A: Resumen de Requisitos para el Equipo Backend
1. **Persistencia de Comandos:** El backend debe almacenar el historial de quién ejecutó la orden manual (usuario ID) antes de reenviarla al tópico MQTT del ESP32.
2. **Validación de Rangos:** El backend no debe transmitir setpoints fuera de los límites biológicos seguros (Temperatura: $15^\circ\text{C}$ a $38^\circ\text{C}$; Humedad: $30\%$ a $90\%$).
3. **Escucha Permanente:** El backend debe mantener un consumidor (Worker o Listener MQTT) suscrito a `avisens/+/telemetria/#` y `avisens/+/status/lwt` para ingesta inmediata en base de datos.

### Apéndice B: Principales Dependencias de Firmware

| Librería | Versión Recomendada | Propósito |
|:---|:---|:---|
| **PubSubClient** o **AsyncMqttClient** | $\ge 2.8$ | Cliente MQTT ligero para ESP32 |
| **ArduinoJson** | $\ge 6.21$ | Serialización y deserialización eficiente de JSON |
| **DHT sensor library for ESPx** | $\ge 1.19$ | Lectura de DHT22 optimizada para ESP32 |
| **HX711** (bogde) | $\ge 0.7.5$ | Interfaz de adquisición para la celda de carga |
| **PID_v1** o implementación nativa | Reciente | Algoritmo clásico de lazo cerrado PID |