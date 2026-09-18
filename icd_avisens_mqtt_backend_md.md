# Documento de Control de Interfaz (ICD)
# AVÍSENS — Especificación de Integración MQTT (IoT ↔ Backend)

---

- **Versión:** 1.0.0
- **Responsable IoT:** Equipo de Firmware Embebido (ESP32)
- **Destinatarios:** Equipo de Backend / Arquitectura Cloud
- **Estado:** Propuesta de Integración Aprobada

---

## 1. Resumen de Integración y Parámetros del Broker

Para que el backend y el ESP32 puedan comunicarse, ambos deben conectarse al mismo Broker MQTT. El firmware del ESP32 operará como cliente bajo las siguientes especificaciones:

| Parámetro | Valor / Especificación | Notas |
|---|---|---|
| **Protocolo** | MQTT v3.1.1 / v5.0 | TCP estándar o WebSockets (si backend usa WS) |
| **Puerto Estándar (No TLS)** | `1883` | Entorno de desarrollo local / pruebas |
| **Puerto Seguro (TLS/SSL)** | `8883` | Obligatorio para producción |
| **Keep-Alive** | `15` segundos | El ESP32 enviará PINGREQ si no hay tráfico |
| **Esquema de Client ID** | `esp32_galpon_{id}` | Ej: `esp32_galpon_01` (Único por dispositivo) |
| **Formato de Payloads** | JSON UTF-8 estricto | Serializado mediante ArduinoJson |
| **Codificación Numérica** | Floats con 1 o 2 decimales | Evitar strings en valores analógicos |

---

## 2. Mapa Completo de Tópicos MQTT

Convención raíz: `avisens/{device_id}/...`  
*(Donde `{device_id}` corresponde al identificador del galpón, ej. `galpon_01`).*

```
avisens/{device_id}/
├── telemetria/
│   ├── dht22                  [ESP32 -> Backend] Lectura Higrotérmica
│   ├── gases                  [ESP32 -> Backend] Calidad de Aire / NH3
│   ├── peso                   [ESP32 -> Backend] Celda de Carga / Tolva
│   ├── obstaculo              [ESP32 -> Backend] Sensor de proximidad
│   └── diagnostico            [ESP32 -> Backend] Heap, RSSI, Fallas
├── actuadores/
│   ├── {actuador}/set         [Backend -> ESP32] Comando de Control
│   ├── config/pid             [Backend -> ESP32] Calibración de Setpoints
│   └── estado                 [ESP32 -> Backend] Telemetría de Estado Real
└── status/
    └── lwt                    [ESP32 -> Broker -> Backend] Presencia Online/Offline
```

---

## 3. Especificación de Payloads (Contratos de Datos)

### 3.1 Publicaciones del ESP32 hacia el Backend (Telemetría)

#### A. Temperatura y Humedad (`telemetria/dht22`)
- **Frecuencia:** Cada 5 segundos.
- **QoS:** 1. Retain: `false`.
```json
{
  "device_id": "galpon_01",
  "timestamp_ms": 1726618800000,
  "temperatura": 25.4,
  "humedad": 63.2,
  "sensor_ok": true
}
```

#### B. Calidad del Aire (`telemetria/gases`)
- **Frecuencia:** Cada 5 segundos.
- **QoS:** 1. Retain: `false`.
```json
{
  "device_id": "galpon_01",
  "raw_adc": 1250,
  "voltaje": 1.01,
  "ppm_nh3_estimado": 14.2,
  "alerta_gas": false
}
```

#### C. Peso de Tolva (`telemetria/peso`)
- **Frecuencia:** Cada 10 segundos.
- **QoS:** 1. Retain: `false`.
```json
{
  "device_id": "galpon_01",
  "peso_gramos": 3420.5,
  "tolva_vacia": false
}
```

#### D. Sensor de Obstáculo (`telemetria/obstaculo`)
- **Frecuencia:** Por evento (inmediato al detectar cambio de estado).
- **QoS:** 1. Retain: `false`.
```json
{
  "device_id": "galpon_01",
  "detectado": true,
  "evento": "INGRESO_DETECTADO"
}
```

#### E. Diagnóstico y Salud (`telemetria/diagnostico`)
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

### 3.2 Comandos del Backend hacia el ESP32 (Control de Actuadores)

Los actuadores disponibles `{actuador}` son:
- `calefactor`
- `humidificador`
- `extractor`
- `alimentador`

#### A. Comando Directo (`actuadores/{actuador}/set`)
- **Publicador:** Backend (en respuesta a orden de la App Móvil/Web).
- **QoS:** 1. Retain: `false`.

```json
{
  "modo": "MANUAL",
  "estado": true,
  "duracion_segundos": 120
}
```

*Definición de Campos:*
- `modo` *(string, obligatorio)*: `"AUTO"` (el lazo interno PID/umbrales retoma el control) o `"MANUAL"` (el actuador obedece el valor forzado).
- `estado` *(boolean, obligatorio si modo="MANUAL")*: `true` (encendido / relé cerrado), `false` (apagado / relé abierto).
- `duracion_segundos` *(entero, opcional)*: Tiempo límite para volver a modo `"AUTO"` de manera automática (prevención de olvido en modo manual). Si es `0` o nulo, permanece indefinido hasta nueva orden.

#### B. Ajuste Remoto de Setpoints y PID (`actuadores/config/pid`)
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

### 3.3 Confirmación de Estado del ESP32 (`actuadores/estado`)

Cada vez que un relé cambia de estado (ya sea por orden manual o por lazo automático), el ESP32 notifica el estado real consolidado:
- **Publicador:** ESP32.
- **QoS:** 1. Retain: `true`.

```json
{
  "device_id": "galpon_01",
  "calefactor": { "estado": false, "modo": "AUTO", "potencia_pct": 0.0 },
  "humidificador": { "estado": true, "modo": "AUTO", "potencia_pct": 45.0 },
  "extractor": { "estado": false, "modo": "AUTO", "potencia_pct": 0.0 },
  "alimentador": { "estado": false, "modo": "MANUAL", "bloqueado": false }
}
```

---

### 3.4 Detección de Caída / Disponibilidad (LWT - Last Will and Testament)

Para que el backend detecte si el galpón se quedó sin energía o sin conexión:

1. **Configuración de Conexión del ESP32:**
   - **Tópico LWT:** `avisens/galpon_01/status/lwt`
   - **Mensaje LWT:** `{"status": "OFFLINE", "causa": "DESCONEXION_INESPERADA"}`
   - **QoS:** 1, Retain: `true`.

2. **Mensaje al conectar con éxito:**
   - En cuanto el ESP32 negocia la sesión MQTT, publica en `avisens/galpon_01/status/lwt`:
   - **Payload:** `{"status": "ONLINE", "ip": "192.168.1.50"}`
   - **QoS:** 1, Retain: `true`.

---

## 4. Requerimientos de Backend para Cumplir el Contrato

1. **Persistencia de Comandos:** El backend debe almacenar el historial de quién ejecutó la orden manual (usuario ID) antes de reenviarla al tópico MQTT del ESP32.
2. **Validación de Rangos:** El backend no debe transmitir setpoints fuera de los límites biológicos seguros (Temperatura: $15^\circ\text{C}$ a $38^\circ\text{C}$; Humedad: $30\%$ a $90\%$).
3. **Escucha Permanente:** El backend debe mantener un consumidor (Worker o Listener MQTT) suscrito a `avisens/+/telemetria/#` y `avisens/+/status/lwt` para ingesta inmediata en base de datos.