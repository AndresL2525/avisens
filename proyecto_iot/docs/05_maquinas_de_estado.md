# Máquinas de Estado

## 1. Propósito y Responsabilidad

Reúne las tablas de transición de las cuatro máquinas de estado del firmware y del arbitraje de
modo de los relés. Todas son de tipo Mealy —la salida depende del estado y del evento— y todas
avanzan por comparación de `millis()`, nunca con `delay()`.

| FSM | Tipo de estado | Archivo | Disparo |
|---|---|---|---|
| Sistema global | `EstadoSistema` | `src/SistemaFSM.cpp` | `SistemaFSM::avanzar()`, cada 10 ms |
| Puerta | `EstadoPuerta` | `src/ControlServo.cpp` | `actualizar(presencia)` |
| Persiana | `EstadoPersiana` | `src/Persiana.cpp` | `actualizar()` |
| Alimentador | `EstadoAlimentador` | `src/Alimentador.cpp` | `actualizar()` |
| Modo de relé | `manualKn_` | `src/GestorActuadores.cpp` | Comandos MQTT |

Los cinco tipos enumerados se declaran como `enum class` en `include/config.h`, de modo que ningún
estado es intercambiable con un entero por accidente.

### 1.1 Dos relojes distintos

Conviene separar las dos cadencias antes de leer ninguna tabla:

- **La FSM global avanza cada 10 ms**, con el periodo de `tareaControl`. Sus contadores
  (`ciclosArranque`, `ciclosEnCalibracion`) cuentan ciclos de 10 ms.
- **La FSM de la puerta avanza cada 10 ms**, junto con la lectura del KY-032, para que el
  antirrebote de 50 ms sea posible. Solo cuando el estado global es `MONITORING`.
- **Las FSM de persiana y alimentador avanzan cada 2 s**, porque sus `actualizar()` están dentro
  del bloque condicionado por `INTERVALO_SENSORES`, y solo cuando el estado global es `MONITORING`.

La consecuencia es que **la temporización de persiana y alimentador se redondea al alza a
múltiplos de 2 s**. Las columnas «Temporización nominal» y «Temporización efectiva» de las tablas
siguientes recogen esa diferencia.

## 2. Conexión Hardware y Pinout

No aplica de forma directa: las FSM son lógica. El hardware que accionan está documentado en
`03_control_pid_actuadores.md` §2.

## 3. Modelo Matemático / Lógica

### 3.1 FSM del sistema global

```
                      ┌──────────────── REARME (serie) ─────────────────┐
                      ▼                                                 │
  ┌──────┐ 10 ciclos ┌─────────────┐  TARA | timeout 15 s  ┌────────────┴─┐
  │ INIT ├──────────►│ CALIBRATION ├──────────────────────►│  MONITORING  │
  └──────┘           └─────────────┘                       └──┬────────▲──┘
                                                              │        │
                                        fallosAcumulados ≥ 3  │        │ sensores
                                                              ▼        │ recuperados
                                                          ┌───────┐    │
                                                          │ ERROR ├────┘
                                                          └───────┘

     ┌───────────┐            ┌──────────┐
     │ ACTUATION │            │ SHUTDOWN │     inalcanzables: nada las asigna
     └───────────┘            └──────────┘
```

| Estado actual | Evento / Condición | Estado siguiente | Acción (salida) |
|---|---|---|---|
| `INIT` | `ciclosArranque ≥ CICLOS_ARRANQUE_MIN` | `CALIBRATION` | Pone `ciclosEnCalibracion` a cero |
| `CALIBRATION` | Comando serie `TARA` ejecutado | `MONITORING` | — |
| `CALIBRATION` | `ciclosEnCalibracion × 10 ms ≥ TIMEOUT_CALIBRACION_MS` | `MONITORING` | Sale sin tara; el HX711 queda inutilizable |
| `MONITORING` | `fallosAcumulados ≥ MAX_FALLOS_SENSOR` | `ERROR` | Transición desde el bloque de sensores, no desde la FSM |
| `ERROR` | Primera entrada (flanco) | `ERROR` | Fail-safe completo y encolado de `EventoFalla` |
| `ERROR` | `!sensorDHT.enError() && !sensorUltrasonico.enError()` | `MONITORING` | Pone `fallosAcumulados` a cero |
| Cualquiera | Comando serie `REARME` o `RESET` | `INIT` | Reinicia los tres contadores de arranque |
| `ACTUATION` | — | `MONITORING` | Retorno incondicional; estado inalcanzable |
| `SHUTDOWN` | — | `SHUTDOWN` | Fail-safe y bloqueo de sinfín y persiana; inalcanzable |

**Tiempos de arranque.** `INIT` dura 10 ciclos de 10 ms, es decir 100 ms: es una barrera de
consistencia, no una espera de estabilización de sensores. `CALIBRATION` dura hasta 1500 ciclos,
15 s, que es la ventana para enviar `TARA` por consola.

**Detección de flanco en `ERROR`.** La variable estática `ultimoEstadoAtendido` hace que el bloque
de emergencia se ejecute una sola vez al entrar, y no cada 10 ms. Se actualiza al final de la
función solo cuando el estado no es `ERROR`, de modo que una nueva entrada en `ERROR` vuelve a
disparar el fail-safe y una nueva notificación al backend.

**Latencia real hasta `ERROR`.** Hay dos contadores encadenados, ambos avanzando a 2 s:

```
fallo del sensor ─► 3 lecturas inválidas ─► enError()  ≈ 6 s
                 ─► 3 ciclos más          ─► ERROR      ≈ 6 s
                                    total  ≈ 12 s desde el primer fallo
```

Los relés, sin embargo, quedan seguros mucho antes: `GestorActuadores::actualizar()` llama a
`failSafe()` en cuanto `enError()` se pone a `true`, a los 6 s. Ver
`03_control_pid_actuadores.md` §5.2.

**Estados inalcanzables.** Ninguna línea del firmware asigna `ACTUATION` ni `SHUTDOWN`. El SDD los
contempla —actuación en curso y apagado controlado— y el código que los atendería ya existe, pero
falta el disparador: para `ACTUATION`, marcar el estado mientras se accionan los relés; para
`SHUTDOWN`, un comando remoto o de consola que ordene la parada. Registrado en `ARCHITECTURE.md`.

**Congelación de las FSM de actuador en `ERROR`.** Fuera de `MONITORING` no se llama a ningún
`actualizar()`. Los mecanismos conservan el estado que les dejó el fail-safe; en particular la
puerta queda en `CERRANDO` —no en `CERRADA`— hasta que el sistema vuelve a `MONITORING` y su FSM
puede completar la transición. El servo ya está físicamente en 0°: es el estado lógico el que va
por detrás.

### 3.2 FSM de la puerta (`ControlServo`)

```
  ┌─────────┐ presencia  ┌──────────┐ 300 ms  ┌─────────┐
  │ CERRADA ├───────────►│ ABRIENDO ├────────►│ ABIERTA │
  └────▲────┘            └──────────┘         └────┬────┘
       │                                           │ 2000 ms sin presencia
       │              300 ms       ┌──────────┐    │
       └──────────────────────────-┤ CERRANDO │◄───┘
                                   └──────────┘
```

| Estado actual | Evento / Condición | Estado siguiente | Acción (salida) |
|---|---|---|---|
| `CERRADA` | `hayPresencia == true` | `ABRIENDO` | Escribe `ANGULO_ABIERTA` (90°) |
| `ABRIENDO` | Transcurrido `SERVO_DURACION_GIRO` | `ABIERTA` | — |
| `ABIERTA` | `hayPresencia == true` | `ABIERTA` | **Reinicia el temporizador** |
| `ABIERTA` | `SERVO_TIEMPO_ABIERTA` sin presencia | `CERRANDO` | Escribe `ANGULO_CERRADA` (0°) |
| `CERRANDO` | Transcurrido `SERVO_DURACION_GIRO` | `CERRADA` | — |
| Cualquiera | `cerrarEmergencia()` | `CERRANDO` | Escribe 0° y emite `LOG_WARN` |

| Temporización | Nominal | Efectiva (tick de 10 ms) |
|---|---|---|
| Recorrido del servo | 300 ms | 300 ms |
| Permanencia abierta | 2000 ms | 2000 ms |
| Ciclo completo sin presencia sostenida | 2.6 s | 2.6 s |

La entrada `hayPresencia` llega ya filtrada: `SensorKY032` exige `KY032_DEBOUNCE_MS` (50 ms) de
nivel estable antes de cambiarla, así que un cruce fugaz por el haz no dispara `ABRIENDO`.

El rearme del temporizador en `ABIERTA` es la salvaguarda contra cerrar la puerta sobre un animal:
mientras el KY-032 detecte algo, la cuenta atrás no empieza. Su contrapartida —un sensor trabado en
`LOW` manteniendo la puerta abierta indefinidamente— la cubre `KY032_TRABADO_MS` (120 s): pasado
ese tiempo `TareaControl` presenta `hayPresencia == false` a la FSM, que cierra por el camino normal
(`ABIERTA` → `CERRANDO` → `CERRADA`) y no vuelve a abrir hasta que el pin se libere. El aviso al
backend va por `telemetria/eventos` (`PresenciaTrabada`).

`ABRIENDO` y `CERRANDO` no comandan el servo: el ángulo se escribe en la transición y estos
estados solo modelan el tiempo de recorrido mecánico. La FSM no tiene realimentación de posición;
si el servo se atasca, el firmware seguirá creyendo que la puerta está donde le corresponde.

### 3.3 FSM de la persiana (`Persiana`)

```
  ┌────────┐ 5 min   ┌──────────┐ 3 s   ┌───────┐ 500 ms  ┌──────────┐
  │ QUIETA ├────────►│ ABRIENDO ├──────►│ PAUSA ├────────►│ CERRANDO │
  └────▲───┘         └──────────┘       └───────┘         └─────┬────┘
       │                                                        │ 3 s
       └────────────────────────────────────────────────────────┘
```

| Estado actual | Evento / Condición | Estado siguiente | Acción (salida) |
|---|---|---|---|
| `QUIETA` | Transcurrido `INTERVALO_PERSIANA` | `ABRIENDO` | `EN1=HIGH`, `IN1=HIGH`, `IN2=LOW` |
| `ABRIENDO` | Transcurrido `DURACION_PERSIANA` | `PAUSA` | Detiene el motor; la lona queda arriba |
| `PAUSA` | Transcurrido `PAUSA_PERSIANA` | `CERRANDO` | `EN1=HIGH`, `IN1=LOW`, `IN2=HIGH` |
| `CERRANDO` | Transcurrido `DURACION_PERSIANA` | `QUIETA` | Detiene el motor |
| Cualquiera | `setHabilitado(false)` | `QUIETA` | Detiene el motor y bloquea el ciclo |
| Cualquiera | `detener()` | `QUIETA` | Detiene el motor sin bloquear |

| Temporización | Nominal | Efectiva (tick de 2 s) |
|---|---|---|
| Espera entre ciclos | 5 min | 5 min |
| Apertura | 3 s | 4 s |
| Permanencia abierta | 500 ms | 2 s |
| Cierre | 3 s | 4 s |

`PAUSA` es el único estado en que la lona está arriba con el motor parado, y dura 500 ms nominales.
Con la cadencia real, la renovación de aire efectiva son unos 2 s cada 5 minutos. Si el propósito
es ventilar, `PAUSA_PERSIANA` es el parámetro a subir; ver `03_control_pid_actuadores.md` §4.2.

La FSM no tiene finales de carrera: `DURACION_PERSIANA` es una estimación del recorrido. Un valor
excesivo deja el motor forzando contra el tope mecánico durante el resto del tiempo.

`abrirManual()` y `cerrarManual()` están implementados pero **ningún camino de comandos los
invoca**, porque `releDesdeNombre("persiana")` devuelve `0`. Son API disponible para cuando el
contrato MQTT incorpore el mando de la persiana.

### 3.4 FSM del alimentador (`Alimentador`)

```
  ┌──────────┐ 5 min  ┌────────────┐
  │ APAGADO  ├───────►│ ENCENDIDO  │
  │          │◄───────┤  (PWM 50%) │
  └──────────┘  5 min └────────────┘
```

| Estado actual | Evento / Condición | Estado siguiente | Acción (salida) |
|---|---|---|---|
| `APAGADO` | Transcurrido `INTERVALO_ALIMENTO` | `ENCENDIDO` | `IN3=HIGH`, `IN4=LOW`, `analogWrite(EN2, 128)` |
| `ENCENDIDO` | Transcurrido `DURACION_ALIMENTO` | `APAGADO` | `analogWrite(EN2, 0)` y ambas `IN` a `LOW` |
| Cualquiera | `setHabilitado(false)` | `APAGADO` | Detiene el motor y bloquea el ciclo |
| Cualquiera | `detener()` | `APAGADO` | Detiene el motor sin bloquear |

Es la más simple de las cuatro: dos estados y ningún sensor de realimentación. Como
`INTERVALO_ALIMENTO` y `DURACION_ALIMENTO` valen lo mismo, el sinfín gira la mitad del tiempo de
forma permanente. La bandera `habilitado_` viaja a la telemetría como `alimentadorBloqueado`, de
modo que el backend puede ver que el ciclo está inhibido.

### 3.5 Modo de cada relé (AUTO / MANUAL)

Cada uno de los cuatro relés lleva su propia máquina de dos estados, independiente de las
anteriores y gobernada solo por comandos remotos:

```
   comando MANUAL ──►┌────────┐
  ┌────────┐         │ MANUAL │
  │  AUTO  │◄────────┤        │
  └────────┘ comando └────────┘
              AUTO
```

| Estado actual | Evento / Condición | Estado siguiente | Acción (salida) |
|---|---|---|---|
| `AUTO` | Comando con `modo: "MANUAL"` | `MANUAL` | Escribe el estado recibido en el relé |
| `MANUAL` | Comando con `modo: "MANUAL"` | `MANUAL` | Actualiza el estado forzado |
| `MANUAL` | Comando con `modo: "AUTO"` | `AUTO` | No escribe; el lazo recupera el relé en ≤ 2 s |
| `AUTO` | Ciclo de control | `AUTO` | `aplicarControl()` escribe el valor calculado |
| `MANUAL` | Ciclo de control | `MANUAL` | `aplicarControl()` **omite** el relé |
| Cualquiera | `failSafe()` | `AUTO` | Limpia la bandera y escribe el valor de emergencia |
| Cualquiera | Comando con FSM global en `ERROR` / `SHUTDOWN` | Sin cambio | El comando se **descarta** antes de aplicarse |

El estado arranca en `AUTO` y no se persiste: un reinicio devuelve los cuatro relés al control
automático. El backend debe reenviar sus comandos manuales tras detectar un `status/lwt` de vuelta
a `ONLINE`.

**Defecto conocido.** `failSafe()` escribe los relés sin consultar ni borrar estas banderas, de
modo que un relé en `MANUAL` queda atrapado en el valor de emergencia tras la recuperación. Se
detalla en `03_control_pid_actuadores.md` §5.3.

### 3.6 Composición de estados en la telemetría

`EstadoActuadores`, el buzón que Core 0 entrega a Core 1, transporta el estado de todas estas
máquinas en una sola estructura, que se publica en `actuadores/estado` cuando cambia:

| Campo | Origen |
|---|---|
| `calefactor`, `ventilador`, `extractor`, `bomba` | Salida de cada `Actuador` |
| `manualCalefactor` … `manualBomba` | FSM de modo de §3.5 |
| `puerta` | `EstadoPuerta` de §3.2 |
| `persiana` | `EstadoPersiana` de §3.3 |
| `alimentadorActivo` | `EstadoAlimentador == ENCENDIDO` |
| `alimentadorBloqueado` | Negación de `habilitado_` |

La comparación con `memcmp` que decide si republicar exige que ambas copias se inicialicen con
`= {}`; de lo contrario los bytes de relleno indefinidos provocarían publicaciones redundantes. Ver
`01_arquitectura_freertos.md` §3.4.

`SnapshotTelemetria` transporta además `estadoSistema`, de modo que el backend conoce en qué punto
de la FSM global está el nodo en cada envío de diagnóstico.

## 4. Parámetros de Configuración

| Constante | Valor por Defecto | Archivo de Origen | Descripción |
|---|---|---|---|
| `CICLOS_ARRANQUE_MIN` | `10` | `include/config.h` | Ciclos de 10 ms en `INIT` |
| `TIMEOUT_CALIBRACION_MS` | `15000` | `include/config.h` | Ventana para enviar `TARA` |
| `MAX_FALLOS_SENSOR` | `3` | `include/config.h` | Ciclos con error antes de pasar a `ERROR` |
| `PERIODO_TAREA_CONTROL_MS` | `10` | `include/config.h` | Base de tiempo de la FSM global |
| `INTERVALO_SENSORES` | `2000` | `include/config.h` | Base de tiempo real de persiana y alimentador |
| `KY032_DEBOUNCE_MS` | `50` | `include/config.h` | Antirrebote de la entrada de presencia |
| `KY032_TRABADO_MS` | `120000` | `include/config.h` | Presencia continua que inhabilita la puerta |
| `SERVO_DURACION_GIRO` | `300` | `include/config.h` | Recorrido mecánico del servo |
| `SERVO_TIEMPO_ABIERTA` | `2000` | `include/config.h` | Permanencia abierta sin presencia |
| `ANGULO_CERRADA` / `ANGULO_ABIERTA` | `0` / `90` | `include/ControlServo.h` | Posiciones de la puerta |
| `INTERVALO_PERSIANA` | `300000` | `include/config.h` | Espera entre ciclos de persiana |
| `DURACION_PERSIANA` | `3000` | `include/config.h` | Recorrido de la lona |
| `PAUSA_PERSIANA` | `500` | `include/config.h` | Permanencia abierta |
| `INTERVALO_ALIMENTO` | `300000` | `include/config.h` | Espera entre dispensados |
| `DURACION_ALIMENTO` | `300000` | `include/config.h` | Giro del sinfín |

### 4.1 Comandos de consola que alteran las FSM

Se escriben en el monitor serie a 115200 baudios y los procesa `ConsolaSerie::procesar()`, dentro
de `tareaControl`. No distinguen mayúsculas.

| Comando | Efecto sobre las FSM |
|---|---|
| `TARA` | Aplica el factor de escala, ejecuta la tara y marca `calibracionCompletada`, lo que saca a la FSM global de `CALIBRATION` |
| `REARME` / `RESET` | Devuelve la FSM global a `INIT` y pone a cero los contadores de arranque y calibración |

`REARME` **no reinicia el hardware** ni las FSM de actuador: los relés conservan su estado y la
puerta, la persiana y el sinfín siguen en el estado en que estaban. Reinicia únicamente la
secuencia de arranque lógica.

Enviar `TARA` sin HX711 conectado puede provocar un reinicio por watchdog; ver
`02_sensores_calibracion.md` §5.5.

## 5. Manejo de Errores y Fail-Safe

### 5.1 Acciones al entrar en `ERROR`

Se ejecutan una sola vez, en el flanco de entrada:

1. `gestorActuadores.failSafe()` — K1, K2 y K3 a `HIGH`; K4 a `LOW`.
2. `controlServo.cerrarEmergencia()` — servo a 0° y estado lógico a `CERRANDO`.
3. `alimentador.detener()` — motor sin energía, estado `APAGADO`.
4. `persiana.detener()` — motor sin energía, estado `QUIETA`.
5. `Mensajeria::encolarEvento("SensoresCriticos", …, "critico", fallos)` — se encola para que Core 1 lo publique.

El orden importa: **primero se asegura la potencia y después se notifica**. Encolar es una
operación con timeout cero y no puede bloquear, pero aun así el hardware queda seguro antes de que
intervenga nada relacionado con la red.

### 5.2 Salida de `ERROR`

La recuperación es automática y exige que **los dos** sensores críticos vuelvan a leer bien. Basta
una lectura válida de cada uno para que su contador se reinicie y `enError()` deje de ser cierto.
Al transitar a `MONITORING` se pone `fallosAcumulados` a cero, de modo que la siguiente caída
vuelve a disponer del margen completo de tres ciclos.

En el ciclo siguiente a la recuperación, `GestorActuadores::actualizar()` recalcula los cuatro
relés y las tres FSM de actuador reanudan su avance desde donde quedaron congeladas.

### 5.3 Robustez de las comparaciones de tiempo

Todas las transiciones temporizadas usan la forma `ahora - tiempoEstado_ >= CONSTANTE` sobre
`unsigned long`. Es la única forma correcta en Arduino: la aritmética sin signo hace que la resta
siga siendo válida cuando `millis()` desborda a los 49.7 días. La forma equivocada
—`ahora >= tiempoEstado_ + CONSTANTE`— congelaría la máquina durante el desbordamiento. Todo
estado nuevo debe seguir el mismo patrón.

`transicionar()` es el único punto que escribe `estado_` y `tiempoEstado_`, en las tres FSM de
actuador. Mantener esa disciplina es lo que garantiza que ningún estado quede con un sello de
tiempo viejo.

### 5.4 Resumen de estados seguros

| FSM | Estado seguro | Cómo se alcanza |
|---|---|---|
| Sistema global | `ERROR` | Automático por fallo de sensor crítico |
| Puerta | `CERRADA` (servo a 0°) | `cerrarEmergencia()` |
| Persiana | `QUIETA` (motor sin energía) | `detener()` o `setHabilitado(false)` |
| Alimentador | `APAGADO` (motor sin energía) | `detener()` o `setHabilitado(false)` |
| Relés K1-K3 | OFF (`HIGH`) | `failSafe()` |
| Relé K4 | **ON** (`LOW`) | `failSafe()`; el agua se mantiene |
