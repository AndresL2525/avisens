# Arquitectura FreeRTOS — Cores, Tareas y Colas

## 1. Propósito y Responsabilidad

Reparte el firmware en dos tareas ancladas a cores distintos y define la frontera de datos entre
ellas. El objetivo es que el lazo de control ambiental siga operando con la misma cadencia aunque
la red esté caída, saturada o reconectando, conforme a la estrategia de autonomía del SDD §5.2.

| Tarea | Core | Prioridad | Stack | Periodo | Responsabilidad |
|---|:---:|:---:|:---:|:---:|---|
| `tareaControl` | 0 | 2 | 16 KB | 10 ms | Sensores, actuadores, FSM global, watchdog |
| `tareaRed` | 1 | 1 | 8 KB | 100 ms | Wi-Fi, sesión MQTT, publicación y comandos |
| `loop()` | 1 | 1 | — | 1000 ms | Inactiva; todo el trabajo vive en las dos tareas |

`tareaControl` está suscrita al watchdog. `tareaRed` no lo está, porque sus operaciones de red
bloquean durante segundos y dispararían un reinicio espurio.

### 1.1 Módulos de la capa de aplicación

`main.cpp` contiene únicamente `setup()` y `loop()`. Todo lo demás está repartido en módulos con
una responsabilidad cada uno, siguiendo el mismo patrón que los periféricos:

| Módulo | Archivos | Responsabilidad |
|---|---|---|
| `Nodo` | `include/Nodo.h`, `src/Nodo.cpp` | Declara y define las instancias globales de sensores, actuadores, red y FSM; `iniciarPerifericos()` llama a todos los `begin()` |
| `Mensajeria` | `include/Mensajeria.h`, `src/Mensajeria.cpp` | Crea las cuatro colas y ofrece la única API para cruzar la frontera entre cores |
| `SistemaFSM` | `include/SistemaFSM.h`, `src/SistemaFSM.cpp` | Estado global, contadores de arranque y de fallos, transiciones y fail-safe coordinado |
| `ConsolaSerie` | `include/ConsolaSerie.h`, `src/ConsolaSerie.cpp` | Arranque del puerto serie, banner y comandos `TARA` / `REARME` |
| `DetectorGradiente` | `include/DetectorGradiente.h`, `src/DetectorGradiente.cpp` | Aviso por salto térmico abrupto entre dos muestras |
| `TareaControl` | `include/TareaControl.h`, `src/TareaControl.cpp` | Cuerpo de la tarea de Core 1 |
| `TareaRed` | `include/TareaRed.h`, `src/TareaRed.cpp` | Cuerpo de la tarea de Core 0 |

### 1.2 Instancias globales y credenciales

`Nodo.cpp` es el único archivo que define objetos globales. Las tareas y la FSM los alcanzan por
`extern` a través de `Nodo.h`, de modo que la propiedad sigue siendo la de siempre —Core 1 los
sensores y actuadores, Core 0 la red— pero hay un solo sitio donde mirar qué existe.

Las credenciales Wi-Fi viven ahí. `ConexionWiFi` se construye con `WIFI_SSID` y `WIFI_PASS` si
`platformio.ini` los define en `build_flags`; si no, con los literales de prueba de `Nodo.cpp`,
marcados con `// Aquí colocar nombre de red` y `// Aquí colocar contraseña`. La opción de
`build_flags` es la que evita versionar la clave.

## 2. Conexión Hardware y Pinout

No aplica: este módulo no gobierna periféricos de forma directa. El mapa de pines completo está
en el SDD §4.1.

## 3. Modelo Matemático / Lógica

### 3.1 Regla de propiedad de datos

> ⚠️ **Por qué el control va en el core 1 y no en el 0.** La tarea del driver Wi-Fi del ESP32 está
> clavada en el **core 0** por el SDK (`CONFIG_ESP32_WIFI_TASK_CORE_ID`) y no se puede reubicar
> desde la aplicación; por eso Arduino-ESP32 deja el `loopTask` del usuario en el core 1. El reparto
> original de este firmware era el inverso y ponía `tareaControl` a competir con la radio: las
> interrupciones y los bloqueos de caché de flash durante el escaneo Wi-Fi corrompían los protocolos
> bit-bang del DHT22, el HC-SR04 y el HX711, que fallaban con `NaN` y `TIMEOUT` desde el arranque.
> `CORE_CONTROL` y `CORE_RED` se intercambiaron en `config.h` por ese motivo.


Core 1 posee los objetos de sensor y actuador. Core 0 posee la sesión de red. **Ningún objeto se
comparte entre cores.** Todo cruce de frontera se hace copiando estructuras por valor a través de
colas FreeRTOS, lo que elimina la necesidad de mutex y el riesgo de lectura rota en estructuras
de varios campos.

```
        Core 1 (tareaControl)                 Core 0 (tareaRed)
        ─────────────────────                 ─────────────────
        sensores, actuadores                  ConexionWiFi, ClienteMQTT
               │                                      │
               ├── buzonTelemetria  ─────────────────►│  publica telemetría
               ├── buzonActuadores  ─────────────────►│  publica estado retenido
               ├── colaEventos      ─────────────────►│  publica eventos de falla
               │◄───────────────────  colaComandos ───┤  recibe comandos MQTT
```

### 3.2 Buzones frente a colas

Se usan dos patrones distintos según la semántica del dato:

- **Buzón (longitud 1, `xQueueOverwrite` / `xQueuePeek`)** para estado continuo. El productor
  sobreescribe siempre; el consumidor lee sin extraer. Interesa el último valor, no el histórico:
  una telemetría de hace dos ciclos no aporta nada. Nunca se llena ni bloquea al productor.
- **Cola FIFO (`xQueueSend` / `xQueueReceive`)** para eventos discretos. Cada comando y cada
  evento de falla debe procesarse exactamente una vez, así que se encolan y se consumen.

| Estructura | Tipo | Longitud | Productor | Consumidor |
|---|---|:---:|---|---|
| `buzonTelemetria` | `SnapshotTelemetria` | 1 | Core 1 | Core 0 |
| `buzonActuadores` | `EstadoActuadores` | 1 | Core 1 | Core 0 |
| `colaComandos` | `ComandoActuador` | 8 | Core 0 | Core 1 |
| `colaEventos` | `EventoFalla` | 4 | Core 1 | Core 0 |

Todos los envíos usan timeout cero: si una cola está llena se descarta el mensaje antes que
bloquear una tarea. En `colaComandos` eso significaría perder una orden, pero ocho comandos
pendientes sin drenar ya implican que Core 1 está detenido, situación que resuelve el watchdog.

### 3.3 Camino de un comando remoto

El callback de PubSubClient se ejecuta dentro de `mqtt_.loop()`, es decir en Core 0. No toca los
actuadores: traduce el nombre del actuador a número de relé, valida el JSON y encola. Core 1
drena la cola al principio de cada ciclo y aplica los cambios sobre `GestorActuadores`. Así el
único hilo que escribe en los relés sigue siendo Core 1.

```
Broker ─► mqtt_.loop() ─► callbackEstatico ─► procesarComando ─► Mensajeria::encolarComando
                                                                            │
                                            TareaControl: aplicarComandosPendientes ◄┘
                                                        │
                                              GestorActuadores (Core 1)
```

`Mensajeria::encolarComando` es el manejador que `main.cpp` registra en `clienteMQTT.begin()`.
`ClienteMQTT` no conoce la cola: solo invoca el puntero a función que le dieron.

### 3.4 Detección de cambios en Core 0

La telemetría periódica se publica por cadencia. Dos casos se publican por cambio:

- **Presencia:** se compara el valor del snapshot con el último publicado y solo se emite en la
  transición, que es lo que exige el contrato para `telemetria/obstaculo`.
- **Estado de actuadores:** se compara la estructura completa con `memcmp` contra la última
  publicada. Por eso ambas copias se inicializan con `= {}`: sin ello, los bytes de relleno
  quedarían indefinidos y provocarían publicaciones redundantes.

## 4. Parámetros de Configuración

| Constante | Valor por Defecto | Archivo de Origen | Descripción |
|---|---|---|---|
| `CORE_CONTROL` | `1` | `include/config.h` | Core del lazo de control |
| `CORE_RED` | `0` | `include/config.h` | Core de la pila de red |
| `STACK_TAREA_CONTROL` | `16384` | `include/config.h` | Stack de `tareaControl` en bytes |
| `STACK_TAREA_RED` | `8192` | `include/config.h` | Stack de `tareaRed` en bytes |
| `PRIORIDAD_TAREA_CONTROL` | `2` | `include/config.h` | Prioridad FreeRTOS del control |
| `PRIORIDAD_TAREA_RED` | `1` | `include/config.h` | Prioridad FreeRTOS de la red |
| `PERIODO_TAREA_CONTROL_MS` | `10` | `include/config.h` | Cesión del scheduler en Core 1 |
| `PERIODO_TAREA_RED_MS` | `100` | `include/config.h` | Cesión del scheduler en Core 0 |
| `LONGITUD_COLA_COMANDOS` | `8` | `include/config.h` | Comandos en vuelo admitidos |
| `LONGITUD_COLA_EVENTOS` | `4` | `include/config.h` | Eventos de falla en vuelo |
| `LONGITUD_BUZON` | `1` | `include/config.h` | Obligatorio para `xQueueOverwrite` |
| `WDT_TIMEOUT_S` | `10` | `include/config.h` | Timeout del watchdog |
| `INTERVALO_SENSORES` | `2000` | `include/config.h` | Periodo de muestreo |

El periodo de `tareaRed` es de 100 ms y no de un segundo porque `mqtt_.loop()` debe ejecutarse con
frecuencia suficiente para atender el keep-alive de 15 s y entregar los comandos entrantes con
latencia baja. El criterio de éxito TC-03 del SDD exige conmutar un relé en menos de 200 ms.

## 5. Manejo de Errores y Fail-Safe

- **Watchdog.** `tareaControl` se suscribe con `esp_task_wdt_add(NULL)` y hace `esp_task_wdt_reset()`
  al inicio de cada ciclo. Si el lazo se bloquea más de 10 segundos el nodo entra en pánico,
  vuelca el stack y reinicia.
- **Prohibición de E/S de red en Core 1.** Ninguna publicación se realiza desde `tareaControl`.
  Los eventos de falla se encolan y los publica Core 0. Esto evita que un broker lento provoque un
  reinicio por watchdog y, sobre todo, que retrase la actuación del fail-safe: los relés se
  accionan antes de encolar nada.
- **Degradación sin red.** Si Wi-Fi o el broker caen, `tareaRed` no publica y reintenta cada
  `MQTT_REINTENTO_MS`. Core 1 no se entera y mantiene el lazo de control con los últimos umbrales
  conocidos. Los buzones se sobreescriben sin acumular memoria.
- **Pérdida de telemetría.** Los buzones descartan los valores intermedios por diseño. Si la red
  estuvo caída dos minutos, al reconectar se publica el estado actual, no la historia perdida.
