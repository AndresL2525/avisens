# Sensores y Calibración

## 1. Propósito y Responsabilidad

Cubre la capa de adquisición: los cinco sensores del galpón y el filtro compartido que acondiciona
sus señales. Todos se ejecutan en Core 0, dentro de `tareaControl`, y todos se muestrean en el
mismo bloque cada `INTERVALO_SENSORES`.

| Módulo | Archivos | Periférico | Estructura de salida |
|---|---|---|---|
| `SensorDHT` | `include/SensorDHT.h`, `src/SensorDHT.cpp` | DHT22 | `LecturaDHT` |
| `SensorMQ135` | `include/SensorMQ135.h`, `src/SensorMQ135.cpp` | MQ-135 | `LecturaMQ135` |
| `SensorKY032` | `include/SensorKY032.h`, `src/SensorKY032.cpp` | KY-032 | `LecturaKY032` |
| `SensorUltrasonico` | `include/SensorUltrasonico.h`, `src/SensorUltrasonico.cpp` | HC-SR04 | `LecturaUltrasonico` |
| `SensorPeso` | `include/SensorPeso.h`, `src/SensorPeso.cpp` | HX711 | `LecturaPeso` |
| `MovingAverage` | `include/MovingAverage.h` | — (plantilla) | valor filtrado |

Todas las estructuras `Lectura*` viven en `include/config.h`, salvo `LecturaPeso`, que se declara
en `SensorPeso.h`. Es la única excepción a la regla de tipos centralizados y conviene unificarla
cuando se toque ese módulo.

### 1.1 Contrato común de sensor

El patrón que debe seguir todo sensor nuevo:

| Método | Obligatorio | Función |
|---|:---:|---|
| `begin()` | Sí | Configura pines. Se llama desde `setup()` |
| `leer()` | Sí | Muestrea, valida, filtra y devuelve la estructura del ciclo |
| `getUltimaLectura()` | Sí | Devuelve la última lectura **válida** almacenada |
| `enError()` | Crítico | `true` tras `MAX_FALLOS_SENSOR` fallos consecutivos |
| `reset()` | Sí | Reinicia contadores, filtro y estado interno |

`SensorMQ135` y `SensorKY032` no implementan `enError()`: ninguno de los dos alimenta el fail-safe
y ninguno tiene forma de distinguir una lectura buena de una mala (§5.2 y §5.3).

Los pines nunca se pasan por constructor: cada clase los toma de las macros de `config.h`.

## 2. Conexión Hardware y Pinout

| Pin Físico | Pin ESP32 | Función Eléctrica | Pull-Up / Pull-Down |
|---|---|---|---|
| DHT22 DATA | GPIO 4 | Digital bidireccional one-wire | Pull-up externo 4.7 kΩ a 3.3 V |
| MQ-135 AOUT | GPIO 34 | Entrada analógica ADC1_CH6 | Ninguno; GPIO 34 es solo entrada |
| HC-SR04 TRIG | GPIO 13 | Salida digital, pulso de 10 µs | Ninguno |
| HC-SR04 ECHO | GPIO 35 | Entrada digital | Divisor resistivo 5 V a 3.3 V obligatorio |
| KY-032 OUT | GPIO 33 | Entrada digital | Pull-up interno (`INPUT_PULLUP`) |
| HX711 DT | GPIO 15 | Entrada digital de datos | Pin de strapping (MTDO) |
| HX711 SCK | GPIO 16 | Salida digital de reloj | Ninguno |

Advertencias eléctricas que condicionan el firmware:

- **ADC1 y Wi-Fi.** El MQ-135 usa GPIO 34, que pertenece a ADC1. ADC2 queda inutilizable mientras
  el radio está activo, de modo que la elección no es preferencia sino requisito.
- **GPIO 34 y 35 son solo entrada.** No tienen pull-up ni pull-down internos; todo
  acondicionamiento es externo.
- **ECHO entrega 5 V.** Sin divisor resistivo se degrada el pin del ESP32.
- **GPIO 15 es pin de strapping.** Un nivel bajo durante el arranque altera la configuración de
  boot, y la línea DT del HX711 queda en alta impedancia mientras el módulo no tiene alimentación
  estable.
- **Alimentación del HX711.** Su salida DT solo es compatible con 3.3 V si el módulo se alimenta a
  3.3 V. Verificar antes de conectar.

## 3. Modelo Matemático / Lógica

### 3.1 Filtro de media móvil

`MovingAverage<T, SIZE>` es una plantilla de cabecera con buffer circular y suma acumulada, de
modo que cada muestra cuesta tiempo constante en lugar de recorrer el buffer:

$$\bar{V}[k] = \frac{1}{N}\sum_{i=0}^{N-1} V[k-i]$$

La suma se actualiza de forma incremental restando la muestra que se sobreescribe y sumando la
nueva. Mientras el buffer no está lleno el divisor es el número real de muestras y no `SIZE`, lo
que evita el sesgo hacia cero de los primeros ciclos.

La clase acumula además la suma de cuadrados para obtener la desviación estándar sin recorrer el
buffer:

$$\sigma = \sqrt{\frac{1}{N}\sum V_i^2 - \bar{V}^2}$$

Sobre ella se ofrece `esPicoRuido(valor, factor)`, que aplica la regla de las tres sigmas para
descartar valores atípicos. **Ningún módulo la usa todavía**; lo mismo ocurre con `getMin()`,
`getMax()` e `isFilled()`. Son API disponible, no comportamiento activo.

`sum_` y `sum_sq_` son `double` con independencia de `T`. Con `T = int` el promedio se trunca al
devolverse, comportamiento deseado para un valor crudo de ADC.

| Instancia | Tipo | Ventana | Módulo |
|---|---|:---:|---|
| `filtroRaw_` | `int` | 10 | `SensorMQ135` |
| `filtroDistancia_` | `float` | 10 | `SensorUltrasonico` |

Con muestreo cada 2 s, una ventana de 10 muestras supone una constante de tiempo efectiva de unos
20 s: el filtro elimina el ruido de conmutación de los motores, pero también retrasa la respuesta
ante un cambio real. Es un compromiso aceptable en magnitudes ambientales lentas.

### 3.2 DHT22 — temperatura y humedad

`leer()` llama a `readHumidity()` y `readTemperature()` de la librería Adafruit y somete el
resultado a dos filtros en cascada:

1. **Descarte por `NaN`.** Es lo que devuelve la librería cuando falla el checksum o el sensor no
   responde. Corresponde al caso de cable cortado.
2. **Descarte por rango físico.** Temperatura fuera de $[-10, 60]$ °C o humedad fuera de
   $[0, 100]$ %. Cubre el caso de sensor presente pero degradado, que entrega valores con checksum
   correcto y sin sentido físico.

Solo si supera ambos se marca `valida = true`, se copia a `ultimaLectura_` y se pone el contador
de fallos a cero. El rango de validación es más estrecho que el rango nominal del DHT22
(−40 a 80 °C) porque fuera de $[-10, 60]$ °C ningún valor es plausible dentro de un galpón y se
prefiere tratarlo como avería.

El DHT22 no admite muestreo más rápido que 0.5 Hz. El periodo de 2 s de `INTERVALO_SENSORES` es el
mínimo admisible; reducirlo produce lecturas repetidas o `NaN`.

### 3.3 MQ-135 — amoníaco y CO₂

La conversión es directa sobre el ADC de 12 bits:

$$V = \text{raw}_{\text{filtrado}} \cdot \frac{3.3}{4095}$$

El valor que se filtra y se publica es el **crudo del ADC**, no una concentración en ppm. No hay
curva de calibración $R_s/R_0$ implementada: la clasificación se hace por umbrales directos sobre
el valor crudo.

| Condición | Nivel reportado |
|---|---|
| `raw < NH3_MODERADO` (800) | `NORMAL` |
| `NH3_MODERADO` ≤ `raw` < `NH3_ALTO` (800 a 1500) | `MODERADO` |
| `raw ≥ NH3_ALTO` (1500) | `ALTO` |

Solo `NH3_ALTO` tiene efecto sobre los actuadores; `NH3_MODERADO` es informativo y viaja en la
telemetría. La curva del MQ-135 no es lineal y depende de temperatura y humedad, de modo que estos
umbrales son empíricos para el galpón concreto y no trasladables sin recalibrar.

El sensor exige un precalentamiento de 24 a 48 h en la primera puesta en marcha, y de unos minutos
tras cada arranque, antes de que su resistencia interna se estabilice. El firmware no modela esa
espera: las primeras lecturas posteriores al arranque son altas y deben descartarse a mano.

### 3.4 HC-SR04 — nivel de agua

`medirDistancia()` emite el pulso de disparo de 10 µs y mide el ancho del eco con `pulseIn()`:

$$d\,[\text{cm}] = \frac{t_{\text{eco}}\,[\mu s] \times 0.0343}{2}$$

El factor 0.0343 cm/µs es la velocidad del sonido a 20 °C y la división entre dos descuenta el
camino de ida y vuelta. Esa velocidad varía en torno al 0.17 % por grado, error despreciable
frente a unos umbrales de bomba separados 3 cm.

La distancia medida es la que hay **del sensor a la superficie del agua**: crece cuando el
depósito se vacía. Por eso la bomba arranca con distancia alta y no baja, como se detalla en
`03_control_pid_actuadores.md`.

Clasificación del resultado en `EstadoSensorUltrasonico`:

| Condición | Estado | Interpretación |
|---|---|---|
| Eco válido, $0.5 \le d \le 400$ cm | `OK` | Medida utilizable |
| `pulseIn()` devuelve 0 | `TIMEOUT` | Sin eco en 30 ms: sensor desconectado o superficie absorbente |
| $d >$ `MAX_DISTANCIA_AGUA` | `OUT_OF_RANGE` | Fuera del alcance del transductor |
| $d < 0.5$ cm | `OUT_OF_RANGE` | Zona muerta del HC-SR04 |
| Fallo persistente | `ERROR` | `MAX_FALLOS_SENSOR` fallos consecutivos |

El timeout de 30 ms de `pulseIn()` corresponde a unos 5 m de recorrido, algo por encima del
alcance nominal de 4 m. Es una llamada bloqueante dentro de la tarea vigilada por el watchdog,
pero acotada a 30 ms frente a los 10 s del WDT.

El filtro de media móvil se aplica **solo a las lecturas válidas**, de modo que un timeout no
contamina la media.

### 3.5 HX711 — peso de la tolva

La conversión a masa es:

$$\text{Peso [g]} = (\text{raw} - \text{offsetCero}) \times \text{factorEscala}$$

`factorEscala` está expresado en gramos por unidad de ADC (`HX711_FACTOR_ESCALA = 0.453592`), por
lo que la operación es una multiplicación. El SDD §4.2 enuncia la misma relación como división: es
una discrepancia de notación, no de cálculo. Quien escriba el factor como *cuentas por gramo*
deberá invertir la operación.

`offsetCero` se obtiene con `tara()`, que promedia 10 lecturas con la celda descargada. El SDD
§4.2 especifica 5 muestras con descarte de extremos para rechazar dispersión; la implementación
actual usa promedio simple de 10, sin descarte. Divergencia registrada en `ARCHITECTURE.md` §1.3.

Ni la tara ni el factor se persisten: `HX711_FACTOR_ESCALA` se reaplica en cada arranque y el
offset se pierde al reiniciar. El SDD contempla guardarlos en NVS; está pendiente.

**Protocolo.** `leerADC()` implementa el bit-banging del HX711: espera a que DT baje (dato listo),
emite 24 pulsos de reloj leyendo un bit por flanco y añade un 25º pulso que selecciona la ganancia
de 128 para la conversión siguiente. La palabra resultante es complemento a dos de 24 bits.

Esta implementación arrastra tres defectos que afectan a todo valor devuelto; se describen en §5.5.

### 3.6 KY-032 — presencia

Lectura digital con pull-up interno. El módulo tiene salida en colector abierto: `LOW`
significa obstáculo detectado, `HIGH` campo despejado.

**Antirrebote (SDD §4.2).** `leer()` guarda el nivel crudo y el instante en que cambió por última
vez; solo cuando el nivel lleva `KY032_DEBOUNCE_MS` (50 ms) sin cambiar se traslada a la presencia
estable que devuelve la lectura. Para que esa ventana tenga sentido el sensor se muestrea en el
bucle de 10 ms de `tareaControl`, no cada 2 s con el resto de sensores. Un destello infrarrojo o un
insecto que cruce el haz en menos de 50 ms ya no abre la puerta.

**Sensor trabado (SDD §7.2).** Si la presencia estable se mantiene más de `KY032_TRABADO_MS`
(120 s), `enTrabado()` devuelve `true`. El sensor no toma ninguna acción por sí mismo:
`TareaControl` presenta «sin presencia» a la puerta para que cierre y quede inhabilitada, y encola
un evento `PresenciaTrabada` en `telemetria/eventos`. La condición se levanta sola cuando el pin
vuelve a `HIGH`, y se encola un segundo evento de nivel `info`. Ambos casos pasan por `LOG_WARN`.

## 4. Parámetros de Configuración

| Constante | Valor por Defecto | Archivo de Origen | Descripción |
|---|---|---|---|
| `DHTPIN` | `4` | `include/config.h` | Línea de datos del DHT22 |
| `DHTTYPE` | `DHT22` | `include/config.h` | Variante del sensor |
| `MQ135_PIN` | `34` | `include/config.h` | Entrada analógica del MQ-135 |
| `TRIG_AGUA` | `13` | `include/config.h` | Disparo del HC-SR04 |
| `ECHO_AGUA` | `35` | `include/config.h` | Eco del HC-SR04 |
| `KY032_PIN` | `33` | `include/config.h` | Salida del detector de presencia |
| `KY032_DEBOUNCE_MS` | `50` | `include/config.h` | Nivel estable exigido antes de aceptar un cambio |
| `KY032_TRABADO_MS` | `120000` | `include/config.h` | Presencia continua que dispara la alarma de sensor trabado |
| `HX711_DT` | `15` | `include/config.h` | Datos de la celda de carga |
| `HX711_SCK` | `16` | `include/config.h` | Reloj de la celda de carga |
| `HX711_FACTOR_ESCALA` | `0.453592` | `include/config.h` | Gramos por unidad de ADC |
| `UMBRAL_ALIMENTO_BAJO` | `500.0` | `include/config.h` | Gramos por debajo de los cuales se avisa |
| `NH3_MODERADO` | `800` | `include/config.h` | Frontera `NORMAL` / `MODERADO` |
| `NH3_ALTO` | `1500` | `include/config.h` | Frontera `MODERADO` / `ALTO`; dispara ventilación |
| `MAX_DISTANCIA_AGUA` | `400.0` | `include/config.h` | Alcance máximo aceptado, en cm |
| `MAX_FALLOS_SENSOR` | `3` | `include/config.h` | Fallos consecutivos antes de `enError()` |
| `MOVING_AVG_SIZE` | `10` | `include/config.h` | Ventana del filtro de media móvil |
| `INTERVALO_SENSORES` | `2000` | `include/config.h` | Periodo de muestreo de todos los sensores |

| `HX711_TIMEOUT_MS` | `150` | `include/config.h` | Espera máxima a que DT señale dato listo |
| `HX711_MUESTRAS_TARA` | `10` | `include/config.h` | Lecturas promediadas en la tara |
| `HX711_ESPERA_MUESTRA_MS` | `100` | `include/config.h` | Separación entre conversiones, a 10 SPS |
| `HX711_SATURACION_POS` | `8388607` | `include/config.h` | Tope positivo del ADC de 24 bits |
| `HX711_SATURACION_NEG` | `-8388608` | `include/config.h` | Tope negativo del ADC de 24 bits |

### 4.1 Procedimiento de calibración de la celda de carga

1. Arrancar el nodo con la tolva **vacía** y esperar a que la FSM global entre en `CALIBRATION`.
2. Enviar `TARA` por el puerto serie a 115200 baudios. El comando reaplica `HX711_FACTOR_ESCALA`,
   promedia 10 lecturas y fija el offset.
3. Colocar una masa patrón conocida y leer el peso reportado.
4. Ajustar `HX711_FACTOR_ESCALA` por regla de tres:
   $\text{factor}_{\text{nuevo}} = \text{factor}_{\text{actual}} \times \frac{\text{masa patrón}}{\text{peso leído}}$
5. Recompilar, volver a flashear y repetir desde el paso 1.

Si no se envía `TARA`, la FSM sale de `CALIBRATION` por timeout a los 15 s y `leer()` devuelve
lecturas inválidas de forma permanente, porque la bandera `tarado_` nunca se activa.

## 5. Manejo de Errores y Fail-Safe

### 5.1 Cadena de fallo común

Los dos sensores clasificados como críticos por el SDD §7 son el DHT22 y el HC-SR04:

```
lectura inválida ─► contadorFallos_++ ─► ≥ MAX_FALLOS_SENSOR ─► enError() == true
                                                                      │
                             GestorActuadores::failSafe()  ◄──────────┤
                             FSM global ─► ERROR ─► fail-safe completo ┘
```

Una sola lectura válida pone el contador a cero y la recuperación es automática. El HX711 lleva el
mismo contador, pero **no participa en el fail-safe**: su fallo solo se refleja en el campo
`sensor_ok` de la telemetría de peso.

### 5.2 MQ-135

No tiene detección de fallo. `analogRead()` sobre un pin desconectado devuelve un valor flotante
plausible, así que `valida` se fija siempre en `true`. Un sensor arrancado del zócalo produce
lecturas erráticas que el sistema tratará como ciertas, con el riesgo de activar la ventilación
por un valor falso `ALTO`. Mitigación disponible pero no aplicada: usar `esPicoRuido()` del filtro
y contrastar contra el rango físico del ADC.

### 5.3 KY-032

Tampoco distingue fallo de lectura legítima: un sensor desconectado con pull-up interno lee `HIGH`,
es decir «sin presencia», que es el estado seguro. El modo de fallo peligroso es el contrario
—salida forzada a `LOW`— y lo cubre la alarma de sensor trabado (§3.6): tras `KY032_TRABADO_MS` la
puerta cierra y se avisa al backend, sin llevar el sistema a `ERROR`.

### 5.4 Defecto: la telemetría envejece en silencio

`ultimaLectura_` solo se actualiza en lectura válida, en los cinco sensores. Cuando uno falla, el
firmware **publica ese último valor bueno en lugar de un cero**, y `sensor_ok` (o `estado_sensor`
en el caso del agua) es la única señal de que el dato no es fresco. Es una decisión consciente: la
alternativa, publicar `0.0`, entregaba al backend un valor indistinguible de una medida real.

El caso del ultrasónico es el mismo: mientras el contador no llega a `MAX_FALLOS_SENSOR`, arrastra
la última distancia conocida, pero **conserva el estado de fallo real** (`TIMEOUT` u
`OUT_OF_RANGE`). `GestorActuadores` ve un estado distinto de `OK` y mantiene la bomba en su valor
anterior en lugar de decidir sobre un dato viejo.

> Hasta la revisión de defectos, ese dato arrastrado viajaba con `estado == OK` y el consumidor no
> podía distinguirlo de una medida fresca.

### 5.5 El HX711 y el watchdog

`leerADC()` devuelve `bool` y entrega el valor por referencia, porque el ADC de 24 bits puede
producir legítimamente un cero y no existe ningún valor que sirva de centinela de error. La
palabra llega en complemento a dos y se le extiende el signo a 32 bits antes de convertirla.

La espera a que DT baje está acotada por `HX711_TIMEOUT_MS` (150 ms), no por segundos, porque toda
esta ruta se ejecuta dentro de `tareaControl`, que está suscrita al watchdog de 10 s:

| Operación | Peor caso | Contexto |
|---|---|---|
| `verificarConexion()`, 3 intentos | ≈ 0.75 s | `setup()`, antes de existir la tarea vigilada |
| `tara()`, 10 muestras | ≈ 3 s | `ConsolaSerie::procesar()`, dentro de `tareaControl` |

`tara()` aborta y deja `tarado_` en `false` si no obtiene ninguna muestra válida, de modo que un
`TARA` enviado sin la celda conectada falla de forma explícita en vez de fijar un cero arbitrario.

> Con el timeout anterior de 1 s, `tara()` alcanzaba los **11.5 s** en el peor caso y reiniciaba el
> nodo por watchdog. Los tres defectos de `leerADC()` —bits invertidos, sin extensión de signo y
> con `0` como centinela— hacían además que **todo peso publicado fuera incorrecto**. Corregido el
> protocolo, **la celda debe recalibrarse**: el factor de escala vigente se ajustó contra lecturas
> erróneas.

### 5.6 Resumen por sensor

| Sensor | Detecta fallo | Alimenta fail-safe | Deja rastro |
|---|:---:|:---:|---|
| DHT22 | Sí (`NaN` y rango) | Sí | Contador y `LOG_ERROR` al llegar al límite |
| HC-SR04 | Sí (timeout y rango) | Sí | Contador y `LOG_ERROR` al llegar al límite |
| HX711 | Sí (timeout y saturación) | No | `sensor_ok` en `telemetria/peso` |
| MQ-135 | No | No | — |
| KY-032 | Solo trabado en `LOW` (> 120 s) | No | Evento `PresenciaTrabada` en `telemetria/eventos` |
