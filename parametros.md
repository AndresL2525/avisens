# Especificación Técnica de Parámetros — Nodo IoT AVÍSENS

**Destinatario:** Equipo de Backend
**Origen:** auditoría del código fuente en `proyecto_iot/`
**Alcance:** todo lo que aparece aquí está extraído literalmente del código. Ningún campo, valor,
unidad ni comportamiento se ha inferido de la documentación de diseño. Lo que el código no define
está en §6, «Pendiente por definir».

**Archivos auditados:** `include/config.h`, `include/ClienteMQTT.h`, `src/ClienteMQTT.cpp`,
`src/main.cpp`, `src/Nodo.cpp`, `src/TareaControl.cpp`, `src/TareaRed.cpp`, `src/SistemaFSM.cpp`,
`src/Mensajeria.cpp`, `src/ConsolaSerie.cpp`, `src/GestorActuadores.cpp`, `include/SensorPeso.h`, `src/SensorPeso.cpp`,
`src/SensorDHT.cpp`, `src/SensorMQ135.cpp`, `src/SensorKY032.cpp`, `src/SensorUltrasonico.cpp`.

---

## 1. Direccionamiento

| Elemento | Valor | Origen en el código |
|---|---|---|
| Raíz de tópicos | `avisens/{device_id}/` | `ClienteMQTT::ClienteMQTT()`: `baseTopic_ = "avisens/" + deviceId_ + "/"` |
| `device_id` | `galpon_01` | `MQTT_DEVICE_ID` en `config.h`; fijado en compilación |
| Client ID MQTT | `esp32_galpon_01` | `clientId_ = "esp32_" + deviceId_` |
| Tópico LWT | `avisens/{device_id}/status/lwt` | `topicoLwt_ = baseTopic_ + "status/lwt"` |
| Suscripción de comandos | `avisens/{device_id}/actuadores/+/set` | `conectar()`: `baseTopic_ + SEGMENTO_ACTUADORES + "+" + SUFIJO_COMANDO` |

`SEGMENTO_ACTUADORES` es `"actuadores/"` y `SUFIJO_COMANDO` es `"/set"`, ambos `static constexpr char[]`
en `ClienteMQTT.cpp`.

### 1.1 Tópicos publicados

| Tópico | Función que lo arma | Retain | Cadencia |
|---|---|:---:|---|
| `telemetria/dht22` | `publicarDHT()` | No | `INTERVALO_TELEMETRIA_RAPIDA` (5000 ms) |
| `telemetria/gases` | `publicarGases()` | No | `INTERVALO_TELEMETRIA_RAPIDA` (5000 ms) |
| `telemetria/peso` | `publicarPeso()` | No | `INTERVALO_TELEMETRIA_LENTA` (10000 ms) |
| `telemetria/nivel_agua` | `publicarNivelAgua()` | No | `INTERVALO_TELEMETRIA_LENTA` (10000 ms) |
| `telemetria/obstaculo` | `publicarObstaculo()` | No | Al cambiar `snapshot.obstaculo` |
| `telemetria/diagnostico` | `publicarDiagnostico()` | No | `INTERVALO_DIAGNOSTICO` (60000 ms) |
| `telemetria/eventos` | `publicarEventoFalla()` | No | Al drenar `colaEventos` |
| `actuadores/estado` | `publicarEstadoActuadores()` | **Sí** | Al cambiar la estructura completa (`memcmp`) |
| `status/lwt` | `conectar()` y el broker | **Sí** | Al conectar y ante caída |

Las cadencias proceden de `tareaRed()` en `src/TareaRed.cpp`.

**Métricas y eventos van por tópicos separados.** `telemetria/diagnostico` transporta únicamente el
latido periódico —serie temporal continua: heap, RSSI, uptime—, y `telemetria/eventos` únicamente
los eventos de falla discretos. Son esquemas JSON distintos y no se mezclan nunca en el mismo
tópico, de modo que cada uno admite su propia tabla en base de datos sin campos nulos.

### 1.2 Calidad de servicio observada en el código

| Operación | Llamada | QoS |
|---|---|:---:|
| Publicación (todas) | `mqtt_.publish(topic, payload, retener)` | **0** — la sobrecarga usada de PubSubClient no admite parámetro QoS |
| Suscripción a comandos | `mqtt_.subscribe(topico, MQTT_QOS_COMANDOS)` | **1** |
| Registro del LWT | `mqtt_.connect(..., MQTT_QOS_COMANDOS, true, PAYLOAD_LWT_OFFLINE)` | **1** |

---

## 2. Variables de Salida (Telemetría)

Tipo C++ = tipo del campo en el firmware antes de serializar. Donde el valor no se almacena sino
que se calcula en el momento de publicar, la columna lo indica como *derivado*.

### 2.1 `telemetria/dht22`

| Clave JSON | Tipo C++ | Campo origen | Unidad | Sensor / Pin |
|---|---|---|---|---|
| `device_id` | `String` | `deviceId_` | — | — |
| `uptime_ms` | `unsigned long` | `SnapshotTelemetria.uptimeMs` | ms desde el arranque (`millis()`) | — |
| `temperatura` | `float` | `SnapshotTelemetria.temperatura` | °C | DHT22, `DHTPIN` = GPIO 4 |
| `humedad` | `float` | `SnapshotTelemetria.humedad` | % HR | DHT22, `DHTPIN` = GPIO 4 |
| `sensor_ok` | `bool` | `SnapshotTelemetria.dhtOk` | — | — |

- `dhtOk` se asigna en `publicarSnapshot()` de `TareaControl.cpp` como `!sensorDHT.enError()`, es decir es `false` solo
  tras `MAX_FALLOS_SENSOR` (3) lecturas inválidas consecutivas.
- `temperatura` y `humedad` se toman de `dht.valida ? dht : sensorDHT.getUltimaLectura()`. Ante
  lectura inválida viaja la **última lectura válida almacenada**, no un cero.
- Las unidades °C y % no están declaradas en el tipo; se deducen de `GestorActuadores::actualizar()`,
  que imprime `Serial.print("°C | Hum: ")` y `Serial.println("%")` sobre esos mismos valores, y de
  los rangos de validación `TEMP_MIN_VALIDA` / `TEMP_MAX_VALIDA` y `HUM_MIN_VALIDA` / `HUM_MAX_VALIDA`.

### 2.2 `telemetria/gases`

| Clave JSON | Tipo C++ | Campo origen | Unidad | Sensor / Pin |
|---|---|---|---|---|
| `device_id` | `String` | `deviceId_` | — | — |
| `raw_adc` | `int` | `SnapshotTelemetria.gasRaw` | cuentas de ADC | MQ-135, `MQ135_PIN` = GPIO 34 |
| `voltaje` | `float` | `SnapshotTelemetria.gasVoltaje` | V | MQ-135, `MQ135_PIN` = GPIO 34 |
| `nivel` | `const char*` | *derivado* | — | — |
| `alerta_gas` | `bool` | *derivado* | — | — |

- `raw_adc` es el valor **ya filtrado** por `MovingAverage<int, MOVING_AVG_SIZE>`, no la muestra
  cruda: `SensorMQ135::leer()` asigna `lectura.rawValue = filtroRaw_.add(rawValue)`.
- `voltaje` se calcula en `SensorMQ135::leer()` como `rawFiltrado * (ADC_VREF / ADC_MAX_CUENTAS)`,
  es decir `raw * (3.3 / 4095.0)`.
- `nivel` se deriva en `publicarGases()`: `"ALTO"` si `gasRaw >= NH3_ALTO`, `"MODERADO"` si
  `gasRaw >= NH3_MODERADO`, `"NORMAL"` en otro caso.
- `alerta_gas` se deriva como `(s.gasRaw >= NH3_ALTO)`.
- **No existe campo de validez.** `SensorMQ135::leer()` fija `lectura.valida = true` de forma
  incondicional y la clase no implementa `enError()`.

### 2.3 `telemetria/peso`

| Clave JSON | Tipo C++ | Campo origen | Unidad | Sensor / Pin |
|---|---|---|---|---|
| `device_id` | `String` | `deviceId_` | — | — |
| `peso_gramos` | `float` | `SnapshotTelemetria.peso` | gramos | HX711, `HX711_DT` = GPIO 15, `HX711_SCK` = GPIO 16 |
| `tolva_vacia` | `bool` | *derivado* | — | — |
| `sensor_ok` | `bool` | `SnapshotTelemetria.pesoOk` | — | — |

- `peso` se obtiene en `SensorPeso::leer()` como `(rawADC - offsetCero_) * factorEscala_`, con
  `factorEscala_` en gramos por cuenta de ADC (`HX711_FACTOR_ESCALA`, comentado `// Gramos/unidad`).
- **Los negativos se truncan:** `lectura.peso = (peso < 0) ? 0 : peso`.
- `tolva_vacia` se deriva en `publicarPeso()` como `(s.peso < UMBRAL_ALIMENTO_BAJO)`, con el umbral
  en 500.0 gramos.
- `pesoOk` es `!sensorPeso.enError()`.
- `LecturaPeso` contiene además un campo `float voltaje`, calculado como
  `rawADC * (ADC_VREF / HX711_ADC_FONDO_ESCALA)`, que **no se serializa en ningún tópico**.

### 2.4 `telemetria/nivel_agua`

| Clave JSON | Tipo C++ | Campo origen | Unidad | Sensor / Pin |
|---|---|---|---|---|
| `device_id` | `String` | `deviceId_` | — | — |
| `distancia_cm` | `float` | `SnapshotTelemetria.distanciaAgua` | cm | HC-SR04, `TRIG_AGUA` = GPIO 13, `ECHO_AGUA` = GPIO 35 |
| `estado_sensor` | `const char*` | `SnapshotTelemetria.estadoAgua` | — | — |
| `bomba_activa` | `bool` | `SnapshotTelemetria.bombaActiva` | — | Relé K4, `K4_PIN` = GPIO 14 |

- La unidad cm está declarada en el struct: `float distancia; // cm` en `LecturaUltrasonico`.
- El cálculo en `SensorUltrasonico::medirDistancia()` es
  `duracion * VELOCIDAD_SONIDO_CM_US / 2.0f`, con `VELOCIDAD_SONIDO_CM_US = 0.0343f`.
- `distancia_cm` es el valor filtrado por `MovingAverage<float, MOVING_AVG_SIZE>`, aplicado **solo
  a lecturas con estado `OK`**.
- `estado_sensor` se serializa mediante `nombreEstadoUltrasonico()`. Valores posibles, tomados del
  `enum class EstadoSensorUltrasonico`: `"OK"`, `"TIMEOUT"`, `"OUT_OF_RANGE"`, `"ERROR"`.
- `bomba_activa` es `gestorActuadores.getK4()`, el estado real del relé, no la orden.

### 2.5 `telemetria/obstaculo`

| Clave JSON | Tipo C++ | Campo origen | Unidad | Sensor / Pin |
|---|---|---|---|---|
| `device_id` | `String` | `deviceId_` | — | — |
| `detectado` | `bool` | Argumento `detectado` de `publicarObstaculo()` | — | KY-032, `KY032_PIN` = GPIO 33 |
| `evento` | `const char*` | *derivado* | — | — |

- El origen es `SnapshotTelemetria.obstaculo`, asignado desde `LecturaKY032.presencia`.
- `SensorKY032::leer()` calcula `presencia = (estado == LOW)` sobre un pin configurado como
  `INPUT_PULLUP`.
- `evento` se deriva en `publicarObstaculo()`: `"INGRESO_DETECTADO"` si `detectado` es `true`,
  `"PASO_DESPEJADO"` si es `false`.
- La publicación se dispara en `tareaRed()` cuando `snapshot.obstaculo != obstaculoPrevio`, o en el
  primer ciclo tras conectar (`!obstaculoInicializado`).

### 2.6 `telemetria/diagnostico` — esquema de latido

| Clave JSON | Tipo C++ | Campo origen | Unidad |
|---|---|---|---|
| `device_id` | `String` | `deviceId_` | — |
| `free_heap` | valor de `ESP.getFreeHeap()` | *derivado en la publicación* | bytes |
| `wifi_rssi` | valor de `WiFi.RSSI()` | *derivado en la publicación* | dBm |
| `uptime_segundos` | `unsigned long` | `s.uptimeMs / 1000UL` | s |
| `modo_operativo` | `const char*` | `SnapshotTelemetria.estadoSistema` | — |
| `fallos_acumulados` | `uint32_t` | `SnapshotTelemetria.fallosAcumulados` | ciclos |
| `reconexiones_mqtt` | `uint32_t` | `ClienteMQTT::reconexiones_` | — |

- `free_heap` y `wifi_rssi` **no viajan en el snapshot**: se consultan al sistema dentro de
  `publicarDiagnostico()`, ya en Core 1.
- `modo_operativo` se serializa con `nombreEstadoSistema()`. Valores declarados en el
  `enum class EstadoSistema`: `"INIT"`, `"CALIBRATION"`, `"MONITORING"`, `"ACTUATION"`, `"ERROR"`,
  `"SHUTDOWN"`. Ver §6 sobre cuáles son alcanzables.
- `fallos_acumulados` lo incrementa `SistemaFSM::evaluarSensoresCriticos()` una vez por ciclo de muestreo mientras
  `sensorDHT.enError() || sensorUltrasonico.enError()`, y se pone a cero al volver a `MONITORING`.
- `reconexiones_mqtt` se incrementa en `conectar()` tras cada sesión establecida con éxito.

### 2.7 `telemetria/eventos`

| Clave JSON | Tipo C++ | Campo origen | Valor emitido por el código |
|---|---|---|---|
| `device_id` | `String` | `deviceId_` | — |
| `tipo` | `const char*` | literal | Siempre `"FALLA_SENSOR"` |
| `origen` | `char[32]` | `EventoFalla.origen` | Único valor emitido: `"SensoresCriticos"` |
| `mensaje` | `char[96]` | `EventoFalla.mensaje` | Único valor emitido: `"Fallos persistentes en sensores criticos, sistema en fail-safe"` |
| `nivel` | `char[16]` | `EventoFalla.nivel` | Único valor emitido: `"critico"` |
| `fallos_acumulados` | `uint32_t` | `EventoFalla.fallosAcumulados` | — |

Existe **una sola llamada a `Mensajeria::encolarEvento()`** en todo el firmware, en la rama `ERROR` de
`SistemaFSM::avanzar()`. Los tres literales son por tanto los únicos valores que el backend puede
recibir hoy, aunque los buffers admitan otros.

### 2.8 `actuadores/estado`

Estructura anidada: un objeto por actuador. Se publica **con retain**.

| Objeto | Claves | Tipo C++ del origen | Pin |
|---|---|---|---|
| `calefactor` | `estado`, `modo` | `bool` / `bool` | `K1_PIN` = GPIO 32 |
| `ventilador` | `estado`, `modo` | `bool` / `bool` | `K2_PIN` = GPIO 25 |
| `extractor` | `estado`, `modo` | `bool` / `bool` | `K3_PIN` = GPIO 27 |
| `bomba` | `estado`, `modo` | `bool` / `bool` | `K4_PIN` = GPIO 14 |
| `alimentador` | `estado`, `modo`, `bloqueado` | `bool` / literal / `bool` | `EN2_PIN`/`IN3_PIN`/`IN4_PIN` = GPIO 21/22/23 |
| `persiana` | `estado_fsm`, `modo` | `EstadoPersiana` / literal | `EN1_PIN`/`IN1_PIN`/`IN2_PIN` = GPIO 5/18/19 |
| `puerta` | `estado_fsm`, `modo` | `EstadoPuerta` / literal | `SERVO_PIN` = GPIO 2 |

- `modo` de los cuatro relés se deriva de las banderas `manualKn_`:
  `e.manualCalefactor ? "MANUAL" : "AUTO"`, y equivalentes.
- `modo` de `alimentador`, `persiana` y `puerta` es el **literal `"AUTO"` escrito en el código**, no
  una variable. No puede tomar otro valor.
- `alimentador.estado` es `(alimentador.getEstado() == EstadoAlimentador::ENCENDIDO)`.
- `alimentador.bloqueado` es `!alimentador.isHabilitado()`.
- `persiana.estado_fsm`: `"QUIETA"`, `"ABRIENDO"`, `"PAUSA"`, `"CERRANDO"`.
- `puerta.estado_fsm`: `"CERRADA"`, `"ABRIENDO"`, `"ABIERTA"`, `"CERRANDO"`.

### 2.9 `status/lwt`

Dos payloads. **Ninguno se construye con ArduinoJson**; son cadenas literales o concatenadas.

| Caso | Claves | Construcción |
|---|---|---|
| Desconexión | `status`, `causa` | `PAYLOAD_LWT_OFFLINE`, `static constexpr char[]` |
| Conexión | `status`, `ip` | Concatenación de `String` con `WiFi.localIP().toString()` |

---

## 3. Payloads Reales

Formatos exactos según el orden en que las funciones insertan las claves. ArduinoJson conserva el
orden de inserción, de modo que este es el orden real sobre el cable. Los valores son de ejemplo;
los **nombres de campo son literales del código**.

### 3.1 `avisens/{device_id}/telemetria/dht22`

```json
{"device_id":"galpon_01","uptime_ms":86400000,"temperatura":25.4,"humedad":63.2,"sensor_ok":true}
```

### 3.2 `avisens/{device_id}/telemetria/gases`

```json
{"device_id":"galpon_01","raw_adc":742,"voltaje":0.5977,"nivel":"NORMAL","alerta_gas":false}
```

### 3.3 `avisens/{device_id}/telemetria/peso`

```json
{"device_id":"galpon_01","peso_gramos":3420.5,"tolva_vacia":false,"sensor_ok":true}
```

### 3.4 `avisens/{device_id}/telemetria/obstaculo`

```json
{"device_id":"galpon_01","detectado":true,"evento":"INGRESO_DETECTADO"}
```

### 3.5 `avisens/{device_id}/telemetria/nivel_agua`

```json
{"device_id":"galpon_01","distancia_cm":4.2,"estado_sensor":"OK","bomba_activa":false}
```

### 3.6 `avisens/{device_id}/telemetria/diagnostico` — latido

```json
{"device_id":"galpon_01","free_heap":184520,"wifi_rssi":-65,"uptime_segundos":86400,"modo_operativo":"MONITORING","fallos_acumulados":0,"reconexiones_mqtt":3}
```

### 3.7 `avisens/{device_id}/telemetria/eventos`

```json
{"device_id":"galpon_01","tipo":"FALLA_SENSOR","origen":"SensoresCriticos","mensaje":"Fallos persistentes en sensores criticos, sistema en fail-safe","nivel":"critico","fallos_acumulados":3}
```

### 3.8 `avisens/{device_id}/actuadores/estado` (retenido)

```json
{"device_id":"galpon_01","calefactor":{"estado":false,"modo":"AUTO"},"ventilador":{"estado":true,"modo":"AUTO"},"extractor":{"estado":false,"modo":"AUTO"},"bomba":{"estado":false,"modo":"AUTO"},"alimentador":{"estado":false,"modo":"AUTO","bloqueado":false},"persiana":{"estado_fsm":"QUIETA","modo":"AUTO"},"puerta":{"estado_fsm":"CERRADA","modo":"AUTO"}}
```

### 3.9 `avisens/{device_id}/status/lwt` (retenido)

```json
{"status":"OFFLINE","causa":"DESCONEXION_INESPERADA"}
```

```json
{"status":"ONLINE","ip":"192.168.1.50"}
```

---

## 4. Variables de Entrada / Comandos

### 4.1 Comandos MQTT

**Único punto de entrada del firmware:** la suscripción `avisens/{device_id}/actuadores/+/set`,
procesada por `ClienteMQTT::procesarComando()`.

El nombre del actuador **no viaja en el payload**: se extrae del tópico, del segmento situado entre
`actuadores/` y la siguiente barra.

Claves leídas del JSON, literalmente las dos que aparecen en el código:

| Propiedad | Tipo esperado | Valor por defecto | Acción que detona |
|---|---|---|---|
| `modo` | string | `"AUTO"` | `doc["modo"] \| "AUTO"`. Si el valor es exactamente `"MANUAL"`, `comando.modoManual = true`. **Cualquier otro valor da `false`**, es decir AUTO |
| `estado` | bool | `false` | `doc["estado"] \| false`. Solo se usa si `modoManual` es `true` |

Ninguna otra clave del payload se lee. Las claves adicionales se ignoran en silencio.

**Acciones resultantes**, en `aplicarComandosPendientes()` de `TareaControl.cpp`:

| Condición | Llamada |
|---|---|
| `comando.modoManual == true` | `gestorActuadores.establecerManual(comando.rele, comando.estado)` |
| `comando.modoManual == false` | `gestorActuadores.establecerAutomatico(comando.rele)` |

`establecerManual()` marca el relé como manual y le escribe el estado; `establecerAutomatico()`
solo limpia la marca, sin escribir el relé.

**Bloqueo en fail-safe.** `aplicarComandosPendientes()` consulta `sistemaFSM.enFailSafe()` antes de
aplicar: si el estado es `EstadoSistema::ERROR` o `EstadoSistema::SHUTDOWN`, el comando se extrae de la cola y **se
descarta** con `LOG_WARN("Comando remoto descartado: sistema en fail-safe")`. No se aplica, no se
guarda para después y no se emite acuse. El backend puede detectarlo porque `actuadores/estado` no
cambia y `modo_operativo` en `telemetria/diagnostico` vale `"ERROR"`.

El comando **no se aplica en el hilo de red**: `procesarComando()` encola un `ComandoActuador` en
`colaComandos` (longitud `LONGITUD_COLA_COMANDOS` = 8) y lo consume Core 0.

### 4.2 Literales de actuador aceptados

Extraídos de `GestorActuadores::releDesdeNombre()`. El nombre se pasa por `toLowerCase()` antes de
comparar, de modo que **el nombre del actuador no distingue mayúsculas**.

| Literal aceptado | Relé devuelto | Pin |
|---|:---:|---|
| `calefactor`, `k1` | 1 | GPIO 32 |
| `ventilador`, `humidificador`, `k2` | 2 | GPIO 25 |
| `extractor`, `k3` | 3 | GPIO 27 |
| `bomba`, `k4` | 4 | GPIO 14 |
| Cualquier otro | 0 | — |

Un `0` provoca `LOG_WARN("Actuador sin control remoto: ...")` y **el comando se descarta antes de
encolarse**. Esto incluye `alimentador`, `persiana` y `puerta`, que no aparecen en la función.

### 4.3 Comandos por puerto serie

Procesados por `ConsolaSerie::procesar()` en `src/ConsolaSerie.cpp`, dentro de `tareaControl`. Se leen hasta
`'\n'`, se aplica `trim()` y `toUpperCase()`.

| Comando | Acción exacta |
|---|---|
| `REARME` o `RESET` | `estadoSistema = EstadoSistema::INIT`; `ciclosArranque = 0`; `ciclosEnCalibracion = 0`; `calibracionCompletada = false` |
| `TARA` | `sensorPeso.setFactor(HX711_FACTOR_ESCALA)`; `sensorPeso.tara()`; `calibracionCompletada = true` |

Velocidad del puerto: `BAUD_RATE` = 115200.

---

## 5. Constantes y Umbrales

Todas son macros `#define` de `include/config.h`, salvo donde se indique.

### 5.1 Pines GPIO

| Constante | Valor | Periférico |
|---|:---:|---|
| `DHTPIN` | 4 | DHT22, datos |
| `MQ135_PIN` | 34 | MQ-135, salida analógica |
| `TRIG_AGUA` | 13 | HC-SR04, disparo |
| `ECHO_AGUA` | 35 | HC-SR04, eco |
| `KY032_PIN` | 33 | KY-032, salida digital |
| `HX711_DT` | 15 | HX711, datos |
| `HX711_SCK` | 16 | HX711, reloj |
| `K1_PIN` | 32 | Relé calefacción |
| `K2_PIN` | 25 | Relé ventilador |
| `K3_PIN` | 27 | Relé extractor |
| `K4_PIN` | 14 | Relé bomba |
| `EN1_PIN` / `IN1_PIN` / `IN2_PIN` | 5 / 18 / 19 | L293D canal A, persiana |
| `EN2_PIN` / `IN3_PIN` / `IN4_PIN` | 21 / 22 / 23 | L293D canal B, sinfín |
| `SERVO_PIN` | 2 | Servo de la puerta |

`DHTTYPE` vale `DHT22` (macro de la librería Adafruit, no numérica).

### 5.2 Umbrales de control

| Constante | Valor | Unidad | Uso en el código |
|---|:---:|---|---|
| `TEMP_FRIO` | 27.0 | °C | `activarCalefaccion = !gasesAltos && (temperatura < TEMP_FRIO)` |
| `TEMP_CALOR` | 32.0 | °C | Término de `activarVentilacion` |
| `HUM_EXTRACTORES` | 65.0 | % | Término de `activarVentilacion` |
| `NH3_ALTO` | 1500 | cuentas ADC | `gasesAltos`, `alerta_gas` y `nivel = "ALTO"` |
| `NH3_MODERADO` | 800 | cuentas ADC | Solo `nivel = "MODERADO"`; no afecta a actuadores |
| `NIVEL_BOMBA_ON` | 6.0 | cm | `distancia > NIVEL_BOMBA_ON` enciende K4 |
| `NIVEL_BOMBA_OFF` | 3.0 | cm | `distancia <= NIVEL_BOMBA_OFF` apaga K4 |
| `UMBRAL_ALIMENTO_BAJO` | 500.0 | gramos | Deriva `tolva_vacia` |
| `MAX_FALLOS_SENSOR` | 3 | lecturas | Umbral de `enError()` y de paso a `ERROR` |
| `MAX_DISTANCIA_AGUA` | 400.0 | cm | Por encima, estado `OUT_OF_RANGE` |
| `MIN_DISTANCIA_AGUA` | 0.5 | cm | Por debajo, estado `OUT_OF_RANGE` |
| `TEMP_MIN_VALIDA` / `TEMP_MAX_VALIDA` | -10.0 / 60.0 | °C | Rango de validación del DHT22 |
| `HUM_MIN_VALIDA` / `HUM_MAX_VALIDA` | 0.0 / 100.0 | % | Rango de validación del DHT22 |

### 5.3 Temporizadores

| Constante | Valor | Unidad | Efecto |
|---|:---:|---|---|
| `INTERVALO_SENSORES` | 2000 | ms | Periodo de muestreo y de actuación |
| `INTERVALO_TELEMETRIA_RAPIDA` | 5000 | ms | Cadencia de `dht22` y `gases` |
| `INTERVALO_TELEMETRIA_LENTA` | 10000 | ms | Cadencia de `peso` y `nivel_agua` |
| `INTERVALO_DIAGNOSTICO` | 60000 | ms | Cadencia del latido |
| `INTERVALO_PERSIANA` | 300000 | ms | Espera entre ciclos de persiana |
| `DURACION_PERSIANA` | 3000 | ms | Recorrido de la lona |
| `PAUSA_PERSIANA` | 500 | ms | Permanencia abierta |
| `INTERVALO_ALIMENTO` | 300000 | ms | Espera entre dispensados |
| `DURACION_ALIMENTO` | 300000 | ms | Giro del sinfín |
| `SERVO_DURACION_GIRO` | 300 | ms | Recorrido del servo |
| `SERVO_TIEMPO_ABIERTA` | 2000 | ms | Permanencia abierta sin presencia |
| `WDT_TIMEOUT_S` | 10 | s | Timeout del watchdog |
| `TIMEOUT_CALIBRACION_MS` | 15000 | ms | Ventana para enviar `TARA` |
| `CICLOS_ARRANQUE_MIN` | 10 | ciclos de 10 ms | Permanencia en `INIT` |
| `MQTT_REINTENTO_MS` | 5000 | ms | Espera entre intentos de conexión |
| `MQTT_KEEPALIVE_S` | 15 | s | Intervalo de PINGREQ |
| `HX711_TIMEOUT_MS` | 150 | ms | Espera a que DT señale dato listo |
| `HX711_ESPERA_MUESTRA_MS` | 100 | ms | Separación entre conversiones |
| `SERIAL_TIMEOUT_MS` | 50 | ms | Timeout de `Serial.readStringUntil()` en `tareaControl` |

### 5.4 Conexión MQTT

| Constante | Valor | Sobreescribible en compilación |
|---|---|:---:|
| `MQTT_BROKER_HOST` | `"192.168.1.100"` | Sí (`#ifndef`) |
| `MQTT_BROKER_PORT` | `1883` | Sí (`#ifndef`) |
| `MQTT_DEVICE_ID` | `"galpon_01"` | Sí (`#ifndef`) |
| `MQTT_USUARIO` | `""` | Sí (`#ifndef`) |
| `MQTT_CLAVE` | `""` | Sí (`#ifndef`) |
| `MQTT_BUFFER_SIZE` | `768` | No |
| `MQTT_QOS_COMANDOS` | `1` | No |

En `conectar()`, un `MQTT_USUARIO` o `MQTT_CLAVE` de longitud cero se convierte en `nullptr` y se
omite del paquete CONNECT.

### 5.5 Capacidad de los documentos JSON

Determinan el tamaño máximo de cada payload. Si un payload excede su capacidad, ArduinoJson lo
trunca en silencio.

| Constante | Valor (bytes) | Tópicos que la usan |
|---|:---:|---|
| `JSON_CAPACIDAD_TELEMETRIA` | 192 | `dht22`, `gases`, `peso`, `obstaculo`, `nivel_agua` |
| `JSON_CAPACIDAD_COMANDO` | 256 | Deserialización de `actuadores/+/set` |
| `JSON_CAPACIDAD_DIAGNOSTICO` | 256 | `diagnostico`, latido |
| `JSON_CAPACIDAD_EVENTO` | 384 | `diagnostico`, evento de falla |
| `JSON_CAPACIDAD_ESTADO` | 640 | `actuadores/estado` |

### 5.6 Conversión y adquisición

| Constante | Valor | Uso |
|---|---|---|
| `ADC_VREF` | 3.3f | Numerador de la conversión a voltios |
| `ADC_MAX_CUENTAS` | 4095.0f | Fondo de escala del ADC de 12 bits |
| `MOVING_AVG_SIZE` | 10 | Ventana del filtro, MQ-135 y HC-SR04 |
| `VELOCIDAD_SONIDO_CM_US` | 0.0343f | cm/µs en el cálculo de distancia |
| `TRIG_PULSO_US` | 10 | Ancho del pulso de disparo |
| `ECHO_TIMEOUT_US` | 30000 | Timeout de `pulseIn()` |
| `HX711_FACTOR_ESCALA` | 0.453592 | Gramos por cuenta de ADC |
| `HX711_BITS` | 24 | Bits leídos por conversión |
| `HX711_MUESTRAS_TARA` | 10 | Lecturas promediadas en la tara |
| `HX711_SATURACION_POS` | 8388607L | Tope positivo; por encima, lectura inválida |
| `HX711_SATURACION_NEG` | -8388608L | Tope negativo; por debajo, lectura inválida |
| `HX711_ADC_FONDO_ESCALA` | 16777216.0f | Fondo de escala de 24 bits |
| `PWM_ALIMENTADOR` | 128 | Ciclo útil pasado a `analogWrite(EN2_PIN, ...)` |
| `ANGULO_CERRADA` / `ANGULO_ABIERTA` | 0 / 90 | Grados escritos al servo |

### 5.7 Concurrencia

| Constante | Valor | Uso |
|---|:---:|---|
| `CORE_CONTROL` / `CORE_RED` | 0 / 1 | Anclaje de las dos tareas |
| `PERIODO_TAREA_CONTROL_MS` | 10 | Periodo de `tareaControl` |
| `PERIODO_TAREA_RED_MS` | 100 | Periodo de `tareaRed` |
| `LONGITUD_COLA_COMANDOS` | 8 | Comandos en vuelo admitidos |
| `LONGITUD_COLA_EVENTOS` | 4 | Eventos de falla en vuelo |
| `LONGITUD_BUZON` | 1 | Buzones de estado continuo |

---

## 6. Pendiente por Definir

Elementos ambiguos, a medio implementar o presentes en el código sin efecto observable. **El
backend no debe construir nada sobre ellos sin acordarlo antes con el equipo de firmware.**

### 6.1 Propiedades que el firmware no lee

| Elemento | Situación en el código |
|---|---|
| `duracion_segundos` | `procesarComando()` lee **solo** `modo` y `estado`. No existe temporizador de retorno a AUTO en ninguna parte del firmware. Un actuador en MANUAL permanece así indefinidamente |
| Tópico `actuadores/config/pid` | No hay suscripción a ese tópico. El comodín `actuadores/+/set` no lo captura, porque el último segmento debe ser `set`. Cualquier mensaje publicado ahí se descarta en el broker sin llegar al nodo |
| Setpoints remotos | No hay lectura de NVS ni variable de setpoint. Los umbrales son macros resueltas en compilación |

### 6.2 `estado` ausente con `modo: "MANUAL"`

`doc["estado"] | false` aplica `false` por defecto. Un `{"modo":"MANUAL"}` sin `estado` **apaga** el
relé y lo congela apagado. No hay distinción entre «no enviado» y «enviado como false», de modo que
el backend debe enviar `estado` de forma explícita siempre que use `modo: "MANUAL"`.

### 6.3 Estados declarados pero inalcanzables

`nombreEstadoSistema()` puede devolver `"ACTUATION"` y `"SHUTDOWN"`, pero **ninguna línea del
firmware asigna esos valores a `estadoSistema`**. `modo_operativo` no puede tomarlos en la
operación actual. Queda por definir si se implementan o se retiran del contrato.

### 6.4 Sensores sin señal de validez

| Tópico | Situación |
|---|---|
| `telemetria/gases` | `SensorMQ135` fija `valida = true` siempre y no implementa `enError()`. No hay `sensor_ok` en el payload. Un sensor desconectado publica valores indistinguibles de los reales |
| `telemetria/obstaculo` | `LecturaKY032` no tiene campo de validez ni contador de fallos. `detectado` no puede acompañarse de una señal de salud |

Queda por definir si se añade `sensor_ok` a ambos tópicos, lo que cambiaría el contrato.

### 6.5 Vocabulario del campo `nivel`

La clave `nivel` significa dos cosas distintas según el tópico:

- En `telemetria/gases`: nivel de gas, valores `"NORMAL"` / `"MODERADO"` / `"ALTO"`.
- En `telemetria/eventos`: severidad del evento, único valor emitido `"critico"`.

Son tópicos y esquemas separados, así que no hay colisión técnica. Queda apuntado porque un
deserializador genérico o un mapeo común de columnas sí las confundiría.

Queda además por definir el vocabulario completo de severidad: el campo admite 15 caracteres y hoy
solo se emite `"critico"`, en minúsculas y sin tilde.

### 6.6 Campos calculados y no publicados

| Campo | Situación |
|---|---|
| `LecturaPeso.voltaje` | Se calcula en cada lectura y no se serializa en ningún tópico |
| `Lectura*.timestamp` | Los cuatro structs de lectura tienen `unsigned long timestamp` con `millis()`. **Ninguno se serializa.** El único tiempo que llega al backend es `uptime_ms` / `uptime_segundos` del snapshot |
| `LecturaMQ135.valida`, `LecturaDHT.valida` | Se usan internamente; no viajan como tal. `sensor_ok` se deriva de `enError()`, que es otra condición |

### 6.7 Ausencia de reloj absoluto

No hay RTC ni cliente NTP en el firmware. El único tiempo disponible es `millis()`, expuesto como
`uptime_ms` y `uptime_segundos`. **El sellado temporal es responsabilidad del backend.**

`millis()` es `unsigned long` de 32 bits y desborda a los ~49.7 días de operación continua, momento
en el que `uptime_ms` vuelve a cero sin que haya reinicio del nodo.

### 6.8 Constante declarada y sin uso

`SERVO_NEUTRO` (93) está definida en `config.h` y no se referencia en ningún archivo. No tiene
efecto sobre el comportamiento ni sobre el contrato.

### 6.9 Identidad del dispositivo fijada en compilación

`MQTT_DEVICE_ID` se resuelve en tiempo de compilación. Un nodo no puede cambiar su `device_id` en
caliente ni recibirlo por configuración remota. Cada galpón exige su propio binario, o el uso de
`-DMQTT_DEVICE_ID` en `platformio.ini`.
