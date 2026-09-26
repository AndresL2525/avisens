# Documento de Control de Interfaz (ICD)
# AVÍSENS — Especificación de Integración MQTT (IoT ↔ Backend)

---

- **Versión:** 1.2.0
- **Responsable IoT:** Equipo de Firmware Embebido (ESP32)
- **Destinatarios:** Equipo de Backend / Arquitectura Cloud
- **Estado:** Transporte MQTT **implementado y verificado contra el código** (ver §1).
  Quedan pendientes el lazo PID, los setpoints remotos y el control de los tres mecanismos
  temporizados; cada apartado lo marca campo a campo.
- **Documento rector:** `sdd_avisens.md` (SDD). Ante discrepancia, prevalece el SDD.
- **Verificación:** los payloads de §4 a §7 se contrastaron uno a uno contra
  `proyecto_iot/src/ClienteMQTT.cpp`. Lo que este documento describe es lo que el nodo publica hoy,
  no lo que se planea publicar.

---

## 1. Estado de Implementación (leer antes de integrar)

El firmware implementa este contrato sobre **PubSubClient**. El antiguo cliente HTTP REST con
autenticación JWT fue eliminado del proyecto. Los tópicos, el retain y el LWT están operativos.

Queda una salvedad que condiciona la integración: **PubSubClient solo publica en QoS 0.** La
suscripción sí usa QoS 1, de modo que la garantía de entrega es asimétrica y deliberada.

| Elemento del contrato | Estado | Nota |
|---|:---:|---|
| **Transporte MQTT** (broker, retain, keep-alive) | ✅ | `ClienteMQTT` sobre PubSubClient |
| **QoS de suscripción** (comandos) | ✅ | QoS 1: los comandos no se pierden |
| **QoS de publicación** (telemetría) | ⚠️ | **QoS 0**: puede perder muestras. Ver §2.1 |
| **LWT / presencia online** | ✅ | Armado en el CONNECT, retenido |
| `telemetria/dht22` → `temperatura`, `humedad`, `sensor_ok` | ✅ | Cada 5 s |
| `telemetria/dht22` → `uptime_ms` | ✅ | Sustituye a `timestamp_ms`. Ver §4.1 |
| `telemetria/gases` → `raw_adc`, `voltaje`, `nivel`, `alerta_gas` | ✅ | Cada 5 s |
| `telemetria/gases` → `ppm_nh3_estimado` | ❌ | Retirado: sin calibración. Ver §4.2 |
| `telemetria/peso` → `peso_gramos`, `tolva_vacia`, `sensor_ok` | ✅ | Cada 10 s |
| `telemetria/obstaculo` | ✅ | Publicación por transición |
| `telemetria/obstaculo` → antirrebote de 50 ms | ❌ | Pendiente. Ver §4.4 |
| `telemetria/nivel_agua` | ✅ | Cada 10 s |
| `telemetria/diagnostico` | ✅ | Latido cada 60 s |
| `telemetria/eventos` | ✅ | Eventos discretos: fail-safe y sensor de presencia trabado |
| `actuadores/{actuador}/set` → `modo`, `estado` | ✅ | Arbitraje AUTO/MANUAL por relé |
| `actuadores/{actuador}/set` → `duracion_segundos` | ❌ | Sin temporizador de retorno a AUTO |
| `actuadores/config/pid` | ❌ | No hay PID ni NVS. Umbrales fijos en compilación |
| `actuadores/estado` consolidado y retenido | ✅ | Publicado ante cambio |
| `actuadores/estado` → `potencia_pct` | ❌ | Los relés son todo o nada, sin PWM |
| Control remoto de `alimentador`, `persiana`, `puerta` | ❌ | Solo funcionan en automático. Ver §5.1 |

**Resumen para planificar:** la telemetría completa y el control manual de los cuatro relés están
operativos. Queda pendiente el lazo PID con sus setpoints remotos, el control remoto de los tres
mecanismos temporizados y el temporizador de retorno automático.

### 1.1 Migración desde la versión 1.1.0

La forma de los payloads no cambia: mismos tópicos, mismos campos, mismos tipos. **No requiere
cambios de código en el backend.** Solo hay dos avisos, y ambos son sobre datos ya almacenados:

- **`peso_gramos` anterior a esta versión es incorrecto.** No es comparable con los nuevos valores.
  Conviene marcar o descartar esa serie histórica.
- **Los `0.0` simultáneos en `temperatura` y `humedad` son artefactos**, no medidas. Filtradlos de
  las series antiguas.

Hay además **un tópico nuevo**, `telemetria/eventos` (§4.7), que recoge los eventos de falla que
antes compartían tópico con el latido de `telemetria/diagnostico`. Una suscripción con el comodín
`avisens/+/telemetria/#` lo captura sin cambios; lo que sí conviene es darle su propia tabla, ya
que su esquema no tiene nada que ver con el de una serie temporal.

Revisad además el QoS de §2.1: las secciones §4 y §6 anunciaban QoS 1 por error y ahora declaran el
QoS 0 real. Si dimensionasteis la ingesta asumiendo entrega garantizada, hay que replantearlo.

---

## 2. Parámetros del Broker

| Parámetro | Valor / Especificación | Notas |
|---|---|---|
| **Protocolo** | MQTT v3.1.1 / v5.0 | TCP estándar o WebSockets (si backend usa WS) |
| **Puerto Estándar (No TLS)** | `1883` | Entorno de desarrollo local / pruebas |
| **Puerto Seguro (TLS/SSL)** | `8883` | Obligatorio para producción |
| **Keep-Alive** | `15` segundos | El ESP32 enviará PINGREQ si no hay tráfico |
| **Esquema de Client ID** | `esp32_galpon_{id}` | Ej: `esp32_galpon_01` (único por dispositivo) |
| **Formato de Payloads** | JSON UTF-8 estricto | Serializado mediante ArduinoJson |
| **Codificación Numérica** | Floats con 1 o 2 decimales | Evitar strings en valores analógicos |

**Identidad del dispositivo.** El `device_id` se fija en compilación mediante `MQTT_DEVICE_ID`,
con valor por defecto `galpon_01`. Se sobreescribe por dispositivo desde `platformio.ini` con
`-DMQTT_DEVICE_ID='"galpon_02"'`, igual que la dirección del broker y sus credenciales.

### 2.1 Calidad de Servicio

| Dirección | QoS | Consecuencia |
|---|:---:|---|
| Backend → ESP32 (comandos) | 1 | Entrega garantizada; un comando no se pierde |
| ESP32 → Backend (telemetría) | 0 | Puede perder muestras; se corrige en la siguiente publicación |
| ESP32 → Backend (`actuadores/estado`) | 0, retenido | El retain compensa: el último estado siempre queda disponible |
| LWT | 1, retenido | Publicado por el broker ante caída del nodo |

La limitación de publicación en QoS 0 procede de PubSubClient. Elevarla a QoS 1 exige sustituir
la librería por AsyncMqttClient, alternativa que contempla el SDD en su Apéndice B; el cambio
afecta al modelo de concurrencia del firmware. **El backend no debe asumir entrega garantizada de
telemetría:** debe tolerar huecos y apoyarse en la cadencia periódica.

---

## 3. Mapa Completo de Tópicos MQTT

Convención raíz: `avisens/{device_id}/...`
*(Donde `{device_id}` corresponde al identificador del galpón, ej. `galpon_01`).*

```
avisens/{device_id}/
├── telemetria/
│   ├── dht22                  [ESP32 -> Backend] Lectura higrotérmica
│   ├── gases                  [ESP32 -> Backend] Calidad de aire / NH3
│   ├── peso                   [ESP32 -> Backend] Celda de carga / tolva
│   ├── obstaculo              [ESP32 -> Backend] Sensor de presencia
│   ├── nivel_agua             [ESP32 -> Backend] Distancia HC-SR04 / estado bomba
│   ├── diagnostico            [ESP32 -> Backend] Latido: heap, RSSI, uptime
│   └── eventos                [ESP32 -> Backend] Eventos de falla discretos
├── actuadores/
│   ├── {actuador}/set         [Backend -> ESP32] Comando de control
│   ├── config/pid             [Backend -> ESP32] Calibración de setpoints
│   └── estado                 [ESP32 -> Backend] Telemetría de estado real
└── status/
    └── lwt                    [ESP32 -> Broker -> Backend] Presencia online/offline
```

---

## 4. Especificación de Payloads — Telemetría (ESP32 → Backend)

### 4.1 Temperatura y Humedad (`telemetria/dht22`)

- **Frecuencia:** cada 5 segundos. **QoS:** 0. Retain: `false`.

```json
{
  "device_id": "galpon_01",
  "uptime_ms": 86400000,
  "temperatura": 25.4,
  "humedad": 63.2,
  "sensor_ok": true
}
```

| Campo | Tipo | Rango / Valores | Estado |
|---|---|---|:---:|
| `temperatura` | float, 1 decimal | Válida solo en `[-10.0, 60.0]` °C | ✅ |
| `humedad` | float, 1 decimal | Válida solo en `[0.0, 100.0]` % | ✅ |
| `sensor_ok` | boolean | `false` tras 3 lecturas inválidas consecutivas | ⚠️ |
| `uptime_ms` | entero | Milisegundos desde el arranque | ❌ |

> ⚠️ **Sobre la marca de tiempo.** La versión 1.0.0 de este ICD especificaba `timestamp_ms` como
> época Unix. **El dispositivo no tiene RTC ni cliente NTP**, por lo que no puede generar ese
> valor. Se sustituye por `uptime_ms` (milisegundos desde el arranque, origen `millis()`), y
> **el backend debe sellar la hora de recepción**. Si se requiere hora de dispositivo fiable,
> hay que añadir NTP al firmware y volver a `timestamp_ms`.
>
> Nota adicional: `millis()` desborda a los ~49 días de operación continua.

> ⚠️ **Dato potencialmente rancio.** Ante una lectura inválida el firmware republica la última
> lectura buena, no un hueco ni un cero. El backend debe apoyarse en `sensor_ok` y descartar por
> antigüedad: si `sensor_ok` es `false`, el par temperatura/humedad que acompaña al mensaje es
> antiguo y no debe alimentar ningún cálculo.

### 4.2 Calidad del Aire (`telemetria/gases`)

- **Frecuencia:** cada 5 segundos. **QoS:** 0. Retain: `false`.

```json
{
  "device_id": "galpon_01",
  "raw_adc": 1250,
  "voltaje": 1.01,
  "nivel": "MODERADO",
  "alerta_gas": false
}
```

| Campo | Tipo | Rango / Valores | Estado |
|---|---|---|:---:|
| `raw_adc` | entero | `[0, 4095]`, media móvil de 10 muestras | ✅ |
| `voltaje` | float, 2 decimales | `[0.00, 3.30]` V | ✅ |
| `nivel` | string | `"NORMAL"` \| `"MODERADO"` \| `"ALTO"` | ⚠️ |
| `alerta_gas` | boolean | `true` cuando `raw_adc >= 1500` | ⚠️ |

**Umbrales de clasificación** (`include/config.h`):

| Nivel | Condición |
|---|---|
| `NORMAL` | `raw_adc < 800` |
| `MODERADO` | `800 <= raw_adc < 1500` |
| `ALTO` | `raw_adc >= 1500` → dispara ventilación prioritaria |

> ❌ **`ppm_nh3_estimado` retirado del contrato.** La versión 1.0.0 lo incluía, pero el firmware
> no implementa ninguna conversión a partes por millón: el MQ-135 requiere calibrar la resistencia
> de referencia $R_0$ en aire limpio y aplicar la curva $R_s/R_0$ del fabricante, trabajo que no
> está hecho ni planificado. Publicar un valor inventado en esa clave sería peor que no tenerla.
> El backend debe trabajar con `raw_adc` y `nivel`. Si se necesita ppm, hay que abrir la tarea de
> calibración primero.

### 4.3 Peso de Tolva (`telemetria/peso`)

- **Frecuencia:** cada 10 segundos. **QoS:** 0. Retain: `false`.

```json
{
  "device_id": "galpon_01",
  "peso_gramos": 3420.5,
  "tolva_vacia": false,
  "sensor_ok": true
}
```

| Campo | Tipo | Rango / Valores | Estado |
|---|---|---|:---:|
| `peso_gramos` | float, 1 decimal | `>= 0.0` (los negativos se truncan a cero) | ✅ |
| `tolva_vacia` | boolean | `true` cuando `peso_gramos < 500.0` | ⚠️ |
| `sensor_ok` | boolean | `false` tras 3 lecturas inválidas | ⚠️ |

> ⚠️ **Calibración obligatoria.** El peso solo es significativo tras ejecutar la tara. Si no se
> envía el comando `TARA` por consola serie en los primeros 15 segundos de arranque, la bandera
> interna de tarado no se activa y **todas las lecturas salen inválidas**: `peso_gramos` repite el
> último valor bueno y `sensor_ok` acaba en `false`. La tara no se persiste: se pierde en cada
> reinicio.

> ⚠️ **Datos históricos no comparables.** Todo `peso_gramos` anterior a la versión 1.2.0 es
> incorrecto y no debe compararse con los nuevos. Ver §1.1.

### 4.4 Sensor de Presencia (`telemetria/obstaculo`)

- **Frecuencia:** por evento, al detectar cambio de estado. **QoS:** 0. Retain: `false`.

```json
{
  "device_id": "galpon_01",
  "detectado": true,
  "evento": "INGRESO_DETECTADO"
}
```

| Campo | Tipo | Rango / Valores | Estado |
|---|---|---|:---:|
| `detectado` | boolean | `true` = presencia | ✅ |
| `evento` | string | `"INGRESO_DETECTADO"` \| `"PASO_DESPEJADO"` | ✅ |

La publicación se dispara solo en la transición: el nodo compara cada muestra con el último valor
publicado y emite únicamente cuando cambia. El primer mensaje tras conectar establece la línea base.

> ❌ **Antirrebote pendiente.** El SDD §4.2 exige validar 50 ms continuos antes de dar una
> detección por buena. El firmware muestrea el pin sin filtrar, de modo que una interferencia
> óptica transitoria puede generar un par de transiciones espurias. El backend debería ignorar
> transiciones separadas por menos de ese margen.

### 4.5 Nivel de Agua (`telemetria/nivel_agua`)

- **Frecuencia:** cada 10 segundos. **QoS:** 0. Retain: `false`.

```json
{
  "device_id": "galpon_01",
  "distancia_cm": 4.2,
  "estado_sensor": "OK",
  "bomba_activa": false
}
```

| Campo | Tipo | Rango / Valores | Estado |
|---|---|---|:---:|
| `distancia_cm` | float, 1 decimal | `[0.5, 400.0]`, media móvil de 10 muestras | ✅ |
| `estado_sensor` | string | `"OK"` \| `"TIMEOUT"` \| `"OUT_OF_RANGE"` \| `"ERROR"` | ✅ |
| `bomba_activa` | boolean | Estado real de K4 tras aplicar la histéresis | ✅ |

La distancia se mide **del sensor a la superficie del agua**, de modo que **crece cuando el
depósito se vacía**. Es contraintuitivo al construir gráficas: un valor alto significa poca agua.

**Histéresis de la bomba** (`include/config.h`): se enciende cuando `distancia_cm > 6.0`
(depósito bajo) y se apaga cuando `distancia_cm <= 3.0` (depósito lleno). Entre ambos umbrales
mantiene el estado anterior. Ante fallo del sensor conserva el último estado conocido.

> ⚠️ **`estado_sensor` es la única señal de frescura.** Mientras el sensor acumula fallos sin
> llegar al umbral de error, el nodo sigue publicando la **última distancia conocida** acompañada
> del estado real del fallo (`TIMEOUT` u `OUT_OF_RANGE`). Si `estado_sensor` no es `"OK"`, el
> valor de `distancia_cm` es antiguo.

> ℹ️ Tópico introducido en la versión 1.1.0 de este ICD, junto con la migración a MQTT.

### 4.6 Diagnóstico y Salud (`telemetria/diagnostico`)

- **Frecuencia:** cada 60 segundos. **QoS:** 0. Retain: `false`.

```json
{
  "device_id": "galpon_01",
  "free_heap": 184520,
  "wifi_rssi": -65,
  "uptime_segundos": 86400,
  "modo_operativo": "MONITORING",
  "fallos_acumulados": 0,
  "reconexiones_mqtt": 3
}
```

| Campo | Tipo | Rango / Valores | Estado |
|---|---|---|:---:|
| `free_heap` | entero | Bytes libres | ✅ |
| `wifi_rssi` | entero | dBm, típicamente `[-90, -30]` | ✅ |
| `uptime_segundos` | entero | Segundos desde el arranque | ✅ |
| `modo_operativo` | string | `"INIT"` \| `"CALIBRATION"` \| `"MONITORING"` \| `"ERROR"` | ✅ |
| `fallos_acumulados` | entero | Fallos consecutivos en sensores críticos | ✅ |
| `reconexiones_mqtt` | entero | Sesiones MQTT establecidas desde el arranque | ✅ |

Este tópico transporta **únicamente el latido periódico**: una serie temporal continua, con el
mismo esquema en todos los mensajes. Los eventos de falla van por un tópico aparte, §4.7.

> ℹ️ `modo_operativo` recorre `INIT` y `CALIBRATION` durante los primeros segundos tras el
> arranque y se estabiliza en `MONITORING`. Los estados `ACTUATION` y `SHUTDOWN` están definidos
> en el firmware pero no se alcanzan en la operación actual.

### 4.7 Eventos de Falla (`telemetria/eventos`)

- **Frecuencia:** por evento, cuando el nodo entra en fail-safe o el sensor de presencia queda
  trabado (y cuando se libera). **QoS:** 0. Retain: `false`.

```json
{
  "device_id": "galpon_01",
  "tipo": "FALLA_SENSOR",
  "origen": "SensoresCriticos",
  "mensaje": "Fallos persistentes en sensores criticos, sistema en fail-safe",
  "nivel": "critico",
  "fallos_acumulados": 3
}
```

| Campo | Tipo | Rango / Valores | Estado |
|---|---|---|:---:|
| `tipo` | string | Único valor emitido: `"FALLA_SENSOR"` | ✅ |
| `origen` | string | `"SensoresCriticos"` \| `"PresenciaTrabada"` | ⚠️ |
| `mensaje` | string | Texto descriptivo, máximo 95 caracteres | ✅ |
| `nivel` | string | `"critico"` \| `"advertencia"` \| `"info"` | ⚠️ |
| `fallos_acumulados` | entero | Ciclos con fallo al disparar el evento; `0` cuando no aplica | ✅ |

Combinaciones que emite hoy el firmware:

| `origen` | `nivel` | Cuándo |
|---|---|---|
| `SensoresCriticos` | `critico` | Entrada en fail-safe por DHT22 o HC-SR04 |
| `PresenciaTrabada` | `advertencia` | KY-032 en `LOW` más de 120 s; la puerta cierra y queda inhabilitada |
| `PresenciaTrabada` | `info` | El KY-032 vuelve a `HIGH`; la puerta se rehabilita |

**Es un evento discreto, no un latido.** La disponibilidad del nodo se infiere de `status/lwt`, no
de la ausencia de mensajes aquí. Un galpón sano puede pasar semanas sin publicar en este tópico.

> ⚠️ **Vocabulario aún sin cerrar.** Los campos `origen` y `nivel` admiten 31 y 15 caracteres
> respectivamente y la lista de arriba crecerá. **No construyáis un `ENUM` de base de datos sobre
> esos valores** sin acordar antes la lista completa; conviene tratarlos como texto libre indexado.

> ℹ️ **Separado de `telemetria/diagnostico` en la versión 1.2.0.** Antes ambos esquemas compartían
> tópico y se distinguían por la presencia de la clave `tipo`. Si vuestro consumidor ya se suscribe
> con el comodín `avisens/+/telemetria/#`, el cambio es transparente y no exige tocar la
> suscripción; sí conviene separar la tabla de destino.

---

## 5. Comandos de Control (Backend → ESP32)

### 5.1 Actuadores Direccionables

Los literales válidos para `{actuador}` derivan de `GestorActuadores::releDesdeNombre()`:

| Literal | Etapa de potencia | Control manual | Estado |
|---|---|:---:|:---:|
| `calefactor` | Relé K1 | Sí | ✅ |
| `ventilador` | Relé K2 | Sí | ✅ |
| `extractor` | Relé K3 | Sí | ✅ |
| `bomba` | Relé K4 | Sí | ✅ |
| `alimentador` | L293D canal B | No | ❌ |
| `persiana` | L293D canal A | No | ❌ |
| `puerta` | Servomotor | No | ❌ |

> ❌ **Ruptura de contrato en la versión 1.0.0.** Aquel documento listaba `alimentador` como
> actuador direccionable, pero el firmware **no lo enruta**: un comando con ese nombre se rechaza
> con `"Actuador desconocido"`. Lo mismo ocurriría con `persiana` y `puerta`. Estos tres
> mecanismos funcionan hoy **solo en automático**, gobernados por sus propias máquinas de estado
> temporizadas. Exponerlos al control remoto es desarrollo pendiente.

> ⚠️ **Alias `humidificador`.** El galpón no tiene humidificador: el elemento conectado a K2 es un
> ventilador. El literal canónico es `ventilador`. El firmware sigue aceptando `humidificador`
> como alias en desuso para no romper integraciones existentes, pero el backend debe migrar al
> nombre canónico; el alias se retirará en una versión futura.

### 5.2 Comando Directo (`actuadores/{actuador}/set`)

- **Publicador:** Backend. **QoS:** 1. Retain: `false`.

```json
{
  "modo": "MANUAL",
  "estado": true,
  "duracion_segundos": 120
}
```

| Campo | Tipo | Obligatorio | Estado |
|---|---|---|:---:|
| `modo` | string: `"AUTO"` \| `"MANUAL"` | Sí | ✅ |
| `estado` | boolean | Sí, si `modo == "MANUAL"` | ✅ |
| `duracion_segundos` | entero | No | ❌ |

- `modo`: `"MANUAL"` congela el actuador en el valor de `estado` y lo excluye del lazo automático;
  `"AUTO"` lo devuelve al control por umbrales. El arbitraje está implementado y es por actuador.
  El firmware normaliza el valor a mayúsculas, de modo que `"manual"` y `"Manual"` también valen.
  Cualquier valor que no sea `MANUAL` se interpreta como `AUTO`.
- `estado`: `true` energiza el relé, `false` lo apaga. **Enviadlo siempre que uséis
  `modo: "MANUAL"`**: si falta, el firmware aplica `false` por defecto y el relé queda congelado
  apagado, sin distinguir entre «no enviado» y «enviado como false».
- `duracion_segundos`: límite tras el cual el actuador vuelve solo a `"AUTO"`.

> ❌ **`duracion_segundos` sin efecto.** El firmware no implementa temporizador de retorno
> automático: un actuador puesto en `MANUAL` permanece así indefinidamente hasta recibir un comando
> con `modo: "AUTO"`. **El backend no debe asumir que el dispositivo se rearma solo.**

> ⚠️ **Una emergencia cancela los modos manuales y bloquea los nuevos.** Si un sensor crítico falla
> mientras un actuador está en `MANUAL`, el fail-safe sobrescribe el relé y lo devuelve a `AUTO`.
> Mientras el nodo permanezca en `ERROR`, **todo comando recibido en `actuadores/{actuador}/set` se
> descarta** sin aplicarse ni encolarse; el nodo lo traza por consola y no emite ningún acuse. Al
> recuperarse el sensor, el actuador vuelve al lazo automático por sí solo. Si la orden manual debe
> mantenerse, el backend tiene que reenviarla tras observar en `telemetria/diagnostico` que
> `modo_operativo` volvió a `"MONITORING"`.
>
> La forma de saber si un comando se aplicó sigue siendo la misma: observar `actuadores/estado`. Si
> el estado no cambia tras un comando y `modo_operativo` es `"ERROR"`, el comando fue descartado.

### 5.3 Ajuste Remoto de Setpoints y PID (`actuadores/config/pid`)

- **Publicador:** Backend. **QoS:** 1. Retain: `true`.

```json
{
  "setpoint_temp": 28.0,
  "setpoint_hum": 60.0,
  "kp_temp": 4.5,
  "ki_temp": 0.2,
  "kd_temp": 1.1
}
```

> ❌ **Tópico completo pendiente.** No existe lazo PID en el firmware ni almacenamiento NVS. El
> control actual es por umbrales fijos definidos en tiempo de compilación en `include/config.h`:

| Constante | Valor | Efecto |
|---|---|---|
| `TEMP_FRIO` | `27.0` °C | Por debajo, enciende calefacción |
| `TEMP_CALOR` | `32.0` °C | Por encima, fuerza ventilación |
| `HUM_EXTRACTORES` | `65.0` % | Por encima, activa extracción |
| `NH3_ALTO` | `1500` | Por encima, ventilación prioritaria |

Hasta que exista el PID, **cambiar un setpoint exige recompilar y reflashear**. El backend puede
publicar en este tópico de forma inocua: el dispositivo lo ignora.

---

## 6. Confirmación de Estado (`actuadores/estado`)

Cada vez que un actuador cambia de estado, por orden manual o por lazo automático, el ESP32
notifica el estado real consolidado.

- **Publicador:** ESP32. **QoS:** 0. Retain: `true`.

```json
{
  "device_id": "galpon_01",
  "calefactor": { "estado": false, "modo": "AUTO" },
  "ventilador": { "estado": true, "modo": "AUTO" },
  "extractor": { "estado": false, "modo": "AUTO" },
  "bomba": { "estado": false, "modo": "AUTO" },
  "alimentador": { "estado": false, "modo": "AUTO", "bloqueado": false },
  "persiana": { "estado_fsm": "QUIETA", "modo": "AUTO" },
  "puerta": { "estado_fsm": "CERRADA", "modo": "AUTO" }
}
```

| Campo | Significado | Estado |
|---|---|:---:|
| `estado` | Estado **real** del relé ya aplicado, no la orden recibida | ✅ |
| `modo` | `"AUTO"` \| `"MANUAL"` | ✅ |
| `estado_fsm` | Estado de la máquina de estados del mecanismo temporizado | ✅ |
| `bloqueado` | `true` si el alimentador está inhabilitado por seguridad | ✅ |

Valores de `estado_fsm`:
- `persiana`: `"QUIETA"` \| `"ABRIENDO"` \| `"PAUSA"` \| `"CERRANDO"`
- `puerta`: `"CERRADA"` \| `"ABRIENDO"` \| `"ABIERTA"` \| `"CERRANDO"`

El nodo publica este objeto completo cada vez que detecta un cambio en cualquiera de los siete
actuadores, comparando el estado consolidado contra el último publicado. Al ir retenido, un
backend que se suscriba más tarde recibe de inmediato el último estado conocido sin esperar al
siguiente cambio.

> ❌ **`potencia_pct` retirado del contrato.** La versión 1.0.0 lo incluía por actuador. Los cuatro
> relés son de conmutación todo o nada, sin PWM ni modulación: no existe un porcentaje de potencia
> que informar. Reaparecerá cuando se implemente el PID con ventana PWM proporcional al tiempo
> descrita en el SDD §5.1.

> **Sobre la confirmación de comandos.** No existe tópico de acuse. La combinación de suscripción
> en QoS 1 y este `estado` retenido cumple esa función: el backend confirma la ejecución
> **observando este tópico**, no esperando un mensaje de respuesta al comando.

---

## 7. Detección de Caída / Disponibilidad (LWT)

Es el único mecanismo por el que el backend distingue un galpón apagado de uno simplemente
silencioso. El mensaje de última voluntad se registra en el propio paquete CONNECT, de modo que
el broker lo publica aunque el nodo desaparezca sin despedirse, que es el caso de un corte de
energía.

1. **Configuración de conexión del ESP32:**
   - **Tópico LWT:** `avisens/{device_id}/status/lwt`
   - **Mensaje LWT:** `{"status": "OFFLINE", "causa": "DESCONEXION_INESPERADA"}`
   - **QoS:** 1, Retain: `true`.

2. **Mensaje al conectar con éxito:**
   - Al negociar la sesión MQTT, el ESP32 publica en `avisens/{device_id}/status/lwt`:
   - **Payload:** `{"status": "ONLINE", "ip": "192.168.1.50"}`
   - **QoS:** 0, Retain: `true`.

---

## 8. Retirada de la Interfaz HTTP

La versión 1.0.0 de este contrato convivía con un cliente HTTP REST con autenticación JWT. Ese
cliente fue **eliminado del firmware** y sus endpoints ya no se invocan. El backend puede
retirarlos cuando le convenga.

| Endpoint HTTP retirado | Reemplazo MQTT |
|---|---|
| `POST /auth/device/login` | Credenciales del broker en el paquete CONNECT |
| `POST /sensors/readings` | `telemetria/dht22`, `/gases`, `/peso`, `/obstaculo`, `/nivel_agua` |
| `GET /actuators/commands` | Suscripción a `actuadores/+/set` |
| `POST /actuators/commands/{id}/executed` | `actuadores/estado` retenido |
| `POST /actuators/state` | `actuadores/estado` consolidado |
| `POST /events` | `telemetria/eventos` |
| *(sin equivalente)* | `status/lwt` |

El sondeo periódico desaparece: los comandos llegan por notificación del broker, lo que elimina
la latencia de hasta diez segundos que introducía el modelo anterior.

---

## 9. Requerimientos de Backend para Cumplir el Contrato

1. **Persistencia de comandos:** almacenar el historial de qué usuario ejecutó cada orden manual
   antes de reenviarla al tópico MQTT del ESP32.
2. **Validación de rangos:** no transmitir setpoints fuera de los límites biológicos seguros
   (temperatura de $15^\circ\text{C}$ a $38^\circ\text{C}$; humedad de $30\%$ a $90\%$).
3. **Escucha permanente:** mantener un consumidor suscrito a `avisens/+/telemetria/#` y
   `avisens/+/status/lwt` para ingesta inmediata en base de datos.
4. **Sellado temporal:** registrar la hora de recepción de cada mensaje. Mientras el dispositivo
   carezca de RTC, la hora del backend es la única referencia fiable (§4.1).
5. **Tolerancia a dato rancio:** descartar telemetría por antigüedad y atender a `sensor_ok`. El
   firmware puede republicar la última lectura válida tras una desconexión del sensor.
6. **No asumir rearme automático:** un actuador en `MANUAL` permanece así hasta recibir una orden
   explícita con `modo: "AUTO"` (§5.2).
7. **Tolerar pérdida de telemetría:** la publicación va en QoS 0 (§2.1). El backend debe admitir
   huecos en las series y no interpretarlos como caída del nodo; para eso está `status/lwt`.
8. **Reenviar los modos manuales tras una emergencia:** el fail-safe devuelve los cuatro relés a
   `AUTO` (§5.2). Una orden manual no sobrevive a un fallo de sensor.

---

## 10. Guía de Arranque para el Backend

Todo lo de esta sección funciona **sin el ESP32**: basta un broker local. Sirve para desarrollar e
integrar mientras el galpón no existe.

### 10.1 Levantar un broker de pruebas

```bash
docker run -it --rm -p 1883:1883 eclipse-mosquitto:2 \
  mosquitto -c /mosquitto-no-auth.conf
```

Apuntad el firmware a ese broker desde `platformio.ini` sin tocar el código:

```ini
build_flags =
    -DMQTT_BROKER_HOST='"192.168.1.42"'
    -DMQTT_DEVICE_ID='"galpon_01"'
```

### 10.2 Observar todo lo que emite un galpón

```bash
mosquitto_sub -h localhost -t 'avisens/#' -v
```

El comodín `#` incluye los tópicos retenidos, de modo que `actuadores/estado` y `status/lwt`
llegan de inmediato aunque el nodo lleve horas sin cambios.

### 10.3 Suscripciones que debe montar el backend

```bash
# Ingesta de telemetría de todos los galpones
mosquitto_sub -h localhost -t 'avisens/+/telemetria/#' -q 1 -v

# Presencia: distinguir galpón apagado de galpón silencioso
mosquitto_sub -h localhost -t 'avisens/+/status/lwt' -q 1 -v

# Confirmación de comandos: se observa aquí, no hay tópico de acuse
mosquitto_sub -h localhost -t 'avisens/+/actuadores/estado' -q 1 -v
```

El `+` cubre un solo nivel, es decir un `device_id`. Con un único consumidor se atiende toda la
flota sin suscribirse galpón por galpón.

### 10.4 Enviar un comando

```bash
# Encender el calefactor en manual
mosquitto_pub -h localhost -q 1 \
  -t 'avisens/galpon_01/actuadores/calefactor/set' \
  -m '{"modo":"MANUAL","estado":true}'

# Devolverlo al lazo automático
mosquitto_pub -h localhost -q 1 \
  -t 'avisens/galpon_01/actuadores/ventilador/set' \
  -m '{"modo":"AUTO"}'
```

La confirmación llega por `avisens/galpon_01/actuadores/estado`, no como respuesta al comando.
El nodo tarda como máximo un ciclo de control en aplicarlo, unos 2 segundos.

### 10.5 Simular un galpón sin hardware

Para probar la ingesta antes de disponer del nodo:

```bash
DEV=avisens/galpon_01

mosquitto_pub -h localhost -t "$DEV/status/lwt" -r \
  -m '{"status":"ONLINE","ip":"192.168.1.50"}'

mosquitto_pub -h localhost -t "$DEV/telemetria/dht22" \
  -m '{"device_id":"galpon_01","uptime_ms":120000,"temperatura":25.4,"humedad":63.2,"sensor_ok":true}'

mosquitto_pub -h localhost -t "$DEV/telemetria/nivel_agua" \
  -m '{"device_id":"galpon_01","distancia_cm":4.2,"estado_sensor":"OK","bomba_activa":false}'

mosquitto_pub -h localhost -t "$DEV/actuadores/estado" -r \
  -m '{"device_id":"galpon_01","calefactor":{"estado":false,"modo":"AUTO"},"ventilador":{"estado":true,"modo":"AUTO"},"extractor":{"estado":false,"modo":"AUTO"},"bomba":{"estado":false,"modo":"AUTO"},"alimentador":{"estado":false,"modo":"AUTO","bloqueado":false},"persiana":{"estado_fsm":"QUIETA","modo":"AUTO"},"puerta":{"estado_fsm":"CERRADA","modo":"AUTO"}}'
```

Para simular una caída del nodo, basta con publicar el LWT a mano:

```bash
mosquitto_pub -h localhost -t "$DEV/status/lwt" -r \
  -m '{"status":"OFFLINE","causa":"DESCONEXION_INESPERADA"}'
```

### 10.6 Lista de verificación de integración

| # | Comprobación | Referencia |
|:---:|---|---|
| 1 | El consumidor se suscribe con `+` y atiende varios `device_id` | §10.3 |
| 2 | Cada mensaje se sella con la hora de recepción del backend | §4.1 |
| 3 | `sensor_ok` y `estado_sensor` descartan valores no frescos | §4.1, §4.5 |
| 4 | Las series toleran huecos sin interpretarlos como caída | §2.1 |
| 5 | La caída se detecta por `status/lwt`, no por silencio | §7 |
| 6 | La ejecución de un comando se confirma en `actuadores/estado` | §6 |
| 7 | Los modos manuales se reenvían tras salir de `ERROR` | §5.2 |
| 8 | `humidificador` se ha migrado a `ventilador` | §5.1 |
| 9 | Los comandos a `alimentador`, `persiana` y `puerta` no se envían | §5.1 |
| 10 | `duracion_segundos` no se usa como temporizador | §5.2 |
