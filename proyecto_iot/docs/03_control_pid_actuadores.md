# Control Ambiental y Actuadores

## 1. Propósito y Responsabilidad

Describe la capa que convierte las lecturas de sensor en potencia sobre el galpón: la política de
control, el arbitraje entre automático y remoto, y los cuatro tipos de accionamiento físico
(relé, servo y los dos canales del L293D).

| Módulo | Archivos | Gobierna | Accionamiento |
|---|---|---|---|
| `Actuador` | `include/Actuador.h`, `src/Actuador.cpp` | Una línea de relé | Digital, activo en `LOW` |
| `GestorActuadores` | `include/GestorActuadores.h`, `src/GestorActuadores.cpp` | K1-K4 y la política de control | Compone cuatro `Actuador` |
| `ControlServo` | `include/ControlServo.h`, `src/ControlServo.cpp` | Puerta | PWM de servo |
| `Persiana` | `include/Persiana.h`, `src/Persiana.cpp` | Persiana | L293D canal A, 100 % |
| `Alimentador` | `include/Alimentador.h`, `src/Alimentador.cpp` | Tornillo sinfín | L293D canal B, PWM 50 % |

Los cinco se ejecutan en Core 0. **`GestorActuadores` es el único lugar donde vive la política de
control**: ni los sensores ni la tarea de red deciden nada sobre los relés.

Las máquinas de estado de puerta, persiana y alimentador se documentan en
`05_maquinas_de_estado.md`. Aquí se tratan solo la capa eléctrica y la lógica de decisión.

### 1.1 Cadencia real de actuación

`tareaControl` se ejecuta cada 10 ms, pero las llamadas a `GestorActuadores::actualizar()`,
`controlServo.actualizar()`, `alimentador.actualizar()` y `persiana.actualizar()` están dentro del
bloque condicionado por `INTERVALO_SENSORES`. **La resolución efectiva de toda actuación es por
tanto de 2 s**, no de 10 ms. Cualquier constante de tiempo menor que `INTERVALO_SENSORES` queda
redondeada al alza hasta el siguiente múltiplo de 2 s. Es un dato imprescindible para interpretar
`SERVO_DURACION_GIRO` (300 ms) y `PAUSA_PERSIANA` (500 ms).

Además, ese bloque solo se ejecuta cuando la FSM global está en `MONITORING`. En `ERROR` los
actuadores conservan los valores impuestos por el fail-safe y ninguna FSM avanza.

## 2. Conexión Hardware y Pinout

| Pin Físico | Pin ESP32 | Función Eléctrica | Pull-Up / Pull-Down |
|---|---|---|---|
| Relé K1 (calefactor) | GPIO 32 | Salida digital, activa en `LOW` | Pull-up externo 10 kΩ a 3.3 V |
| Relé K2 (ventilador) | GPIO 25 | Salida digital, activa en `LOW` | Pull-up externo 10 kΩ a 3.3 V |
| Relé K3 (extractor) | GPIO 27 | Salida digital, activa en `LOW` | Pull-up externo 10 kΩ a 3.3 V |
| Relé K4 (bomba) | GPIO 14 | Salida digital, activa en `LOW` | Pull-up externo 10 kΩ a 3.3 V |
| L293D EN1 (persiana) | GPIO 5 | Habilitación canal A | Pin de strapping; emite pulso en el boot |
| L293D IN1 / IN2 | GPIO 18 / 19 | Sentido de giro canal A | Ninguno |
| L293D EN2 (sinfín) | GPIO 21 | Habilitación canal B, con PWM | Ninguno |
| L293D IN3 / IN4 | GPIO 22 / 23 | Sentido de giro canal B | Ninguno |
| Servo puerta | GPIO 2 | PWM de posición | Pin de strapping y LED integrado |

Consideraciones que el firmware no puede resolver por sí solo:

- **Relés activos en `LOW`.** El estado seguro de las cuatro líneas es `HIGH`. `Actuador::begin()`
  escribe `HIGH` antes que nada, pero solo a partir del instante en que se ejecuta.
- **Alta impedancia en el reset.** Entre el reset y la ejecución de `begin()`, los GPIO quedan
  flotantes y los relés en estado indefinido. Las pull-up externas de 10 kΩ exigidas por el SDD
  §4.1 son la única garantía de que nada se energice durante el arranque.
- **GPIO 5 emite un pulso en el boot.** Puede provocar un movimiento breve de la persiana en cada
  reinicio. Es una característica del silicio, no un fallo del firmware.
- **GPIO 2 tiene LED integrado.** El parpadeo en el arranque es normal; conviene comprobar que no
  altera la posición de reposo del servo.
- **Alimentación separada.** Motores, servo y bobinas de relé no deben compartir el regulador de
  3.3 V del ESP32. Masa común, alimentación independiente.

## 3. Modelo Matemático / Lógica

### 3.1 Objetivo del SDD: PID con ventana PWM

El SDD §5.1 especifica un controlador PID discreto para el lazo térmico:

$$e[k] = \text{SP} - \text{PV}[k]$$

$$u[k] = K_p\,e[k] + K_i\,T_s\sum_{j=0}^{k} e[j] + K_d\,\frac{e[k]-e[k-1]}{T_s}$$

con anti-windup por saturación de la integral en $[0, 100]\,\%$ y traducción de $u[k]$ a una
ventana temporal fija de 10 s (*time-proportioning*), de modo que un 60 % se convierte en 6 s
encendido y 4 s apagado. El propósito de esa ventana es limitar el desgaste de los contactos del
relé electromecánico, que no admite conmutación rápida.

**Nada de esto está implementado.** No hay controlador PID, ni setpoints remotos, ni persistencia
en NVS. Queda registrado como pendiente en `ARCHITECTURE.md` §1.3.

### 3.2 Implementación actual: umbrales todo-nada

`GestorActuadores::actualizar()` evalúa tres condiciones y de ahí deriva el estado de los cuatro
relés:

```
gasesAltos   = rawNH3 >= NH3_ALTO
calefaccion  = !gasesAltos && (temperatura < TEMP_FRIO)
ventilacion  = !calefaccion && (temperatura >= TEMP_CALOR ||
                                humedad > HUM_EXTRACTORES ||
                                gasesAltos)
```

El orden de precedencia es deliberado y tiene lectura zootécnica:

1. **El amoníaco manda sobre el frío.** Si el gas está alto, la calefacción se inhibe aunque haga
   frío, porque un galpón cerrado con NH₃ elevado es más peligroso que unos grados de menos.
2. **Calefacción y ventilación son excluyentes.** `ventilacion` exige `!calefaccion`, de modo que
   nunca se calienta y se extrae al mismo tiempo.

Asignación a relés:

| Relé | Carga | Condición de activación |
|:---:|---|---|
| K1 | Calefactor | `calefaccion` |
| K2 | Ventilador | `calefaccion \|\| ventilacion` |
| K3 | Extractor | `ventilacion` |
| K4 | Bomba | histéresis sobre la distancia de agua (§3.3) |

K2 se activa en ambos modos a propósito: durante la calefacción el ventilador reparte el aire
caliente y evita el gradiente entre la zona del foco y el resto del galpón; durante la ventilación
acompaña al extractor.

Tabla de verdad del lazo térmico, con los umbrales por defecto:

| Temperatura | Humedad | NH₃ | K1 | K2 | K3 | Situación |
|---|---|---|:---:|:---:|:---:|---|
| < 27 °C | cualquiera | < 1500 | ON | ON | off | Calentando y repartiendo |
| < 27 °C | cualquiera | ≥ 1500 | off | ON | ON | El gas inhibe la calefacción |
| 27 a 32 °C | ≤ 65 % | < 1500 | off | off | off | Banda muerta: confort |
| 27 a 32 °C | > 65 % | < 1500 | off | ON | ON | Deshumidificando |
| ≥ 32 °C | cualquiera | cualquiera | off | ON | ON | Refrigerando |

La banda muerta de 5 °C entre `TEMP_FRIO` y `TEMP_CALOR` es lo que impide el ciclado rápido de los
relés: sin ella, un único setpoint haría conmutar los contactos en cada oscilación de décimas de
grado. Cumple la misma función que la ventana PWM del SDD, con menor precisión.

### 3.3 Histéresis de la bomba

`calcularActivacionBomba()` es la única parte del control con memoria propia:

```
distancia > NIVEL_BOMBA_ON  (6 cm) ─► bomba ON   (depósito vacío)
distancia ≤ NIVEL_BOMBA_OFF (3 cm) ─► bomba OFF  (depósito lleno)
3 cm < distancia ≤ 6 cm            ─► mantener el estado anterior
```

La distancia crece cuando baja el nivel, porque el HC-SR04 mide del sensor a la superficie. La
banda de 3 cm evita que el oleaje del llenado apague y encienda la bomba de forma continua.

Ante un estado de sensor distinto de `OK` la función devuelve `ultimoEstadoBomba_` sin modificarlo:
no se cambia nada con información dudosa. Esa salvaguarda depende de que el ultrasónico sea honesto
sobre la frescura de su dato, cosa que hace desde que `leer()` dejó de reetiquetar como `OK` la
distancia arrastrada (`02_sensores_calibracion.md` §5.4).

### 3.4 Arbitraje AUTO / MANUAL

Cada relé lleva una bandera de modo. Un comando MQTT con `modo: "MANUAL"` la activa y congela el
relé en el valor recibido; `aplicarControl()` salta ese relé en cada ciclo hasta que un comando
`AUTO` libera la bandera.

| Situación | Efecto sobre `aplicarControl()` |
|---|---|
| Relé en AUTO | Recalcula y escribe el valor del lazo |
| Relé en MANUAL | Se omite por completo; conserva el valor remoto |
| Vuelta a AUTO | El siguiente ciclo (≤ 2 s) recupera el valor calculado |

El arbitraje tiene una excepción que está fuera de esta clase: **con la FSM global en `ERROR` o
`SHUTDOWN`, `aplicarComandosPendientes()` descarta todo comando antes de que llegue aquí.** Sin
ese filtro, un `MANUAL` recibido durante una emergencia encendía el relé y lo dejaba encendido:
el fail-safe ya se había aplicado en el flanco de entrada y no se repite, y `actualizar()` no se
ejecuta fuera de `MONITORING`. Un comando remoto no puede vencer al fail-safe.

`forzarRele()` escribe el valor sin tocar las banderas. Está pensado para depuración y **ningún
camino del firmware lo invoca hoy**. No sirve como sustituto del mando remoto: un relé en AUTO
forzado con `forzarRele()` vuelve a su valor calculado en el siguiente ciclo, a los 2 s.

### 3.5 Traducción de nombres del backend

`releDesdeNombre()` convierte el literal del contrato MQTT en número de relé, sin distinguir
mayúsculas. Devuelve `0` cuando el actuador no admite control por relé, y el comando se descarta
con advertencia.

| Literal aceptado | Relé | Nota |
|---|:---:|---|
| `calefactor`, `k1` | 1 | — |
| `ventilador`, `k2` | 2 | Literal canónico |
| `humidificador` | 2 | **Alias en desuso.** El galpón no tiene humidificador |
| `extractor`, `k3` | 3 | — |
| `bomba`, `k4` | 4 | — |
| `alimentador`, `persiana`, `puerta` | 0 | Solo automáticos; sin mando remoto |

El alias `humidificador` existe porque el contrato original del backend nombraba así al K2. **No
debe retirarse de forma unilateral**: hacerlo rompe al backend mientras no haya migrado. La
función inversa `nombreDesdeRele()` devuelve siempre el literal canónico, de modo que el firmware
nunca publica `humidificador`.

### 3.6 Capa eléctrica del relé

`Actuador` encapsula la inversión lógica, de modo que ningún otro módulo necesita saber que el
relé es activo en bajo:

| Llamada | Nivel en el GPIO | `getEstado()` | Bobina |
|---|:---:|:---:|---|
| `activar()` | `LOW` | `true` | Energizada |
| `desactivar()` | `HIGH` | `false` | Liberada |
| `begin()` | `HIGH` | `false` | Liberada |

Los métodos son `virtual` para permitir derivar actuadores con temporización propia, aunque hoy
ninguna clase hereda de ella: `GestorActuadores` compone cuatro instancias en lugar de extenderlas.

### 3.7 Servo de la puerta

El servo se posiciona en dos ángulos discretos, sin rampa: `ANGULO_CERRADA` (0°) y `ANGULO_ABIERTA`
(90°). `escribirServoCuidado()` escribe el ángulo y cede el procesador con
`vTaskDelay(pdMS_TO_TICKS(1))`, para no retener Core 0 durante la conmutación del PWM.

`SERVO_DURACION_GIRO` no comanda el servo: modela el tiempo que este tarda en recorrer
mecánicamente los 90°, y se usa para saber cuándo dar el movimiento por terminado. Si el servo es
más lento que ese valor, la FSM lo dará por abierto antes de que lo esté.

### 3.8 Canales del L293D

Ambos canales comparten el mismo esquema de tres líneas: una habilitación y dos de sentido.

| EN | IN_a | IN_b | Efecto |
|:---:|:---:|:---:|---|
| `LOW` | × | × | Salidas en alta impedancia; motor libre |
| `HIGH`/PWM | `HIGH` | `LOW` | Giro directo (abrir / dispensar) |
| `HIGH`/PWM | `LOW` | `HIGH` | Giro inverso (cerrar) |
| `HIGH`/PWM | igual | igual | Frenado; **no se usa** |

Diferencias entre los dos canales:

- **Persiana (canal A).** `digitalWrite(EN1_PIN, HIGH)`: el motor trabaja al 100 % porque debe
  vencer el peso de la lona. Usa los dos sentidos.
- **Alimentador (canal B).** `analogWrite(EN2_PIN, 128)`: ciclo útil del 50 %, suficiente para el
  par del tornillo sinfín y con menor consumo y menor riesgo de atasco. Usa un único sentido; el
  sinfín no necesita invertir.

Toda función que cambia el estado del motor cede el procesador con `vTaskDelay(1 ms)` tras
escribir. Se busca que el cambio de las tres líneas se consolide antes de que la tarea siga
ejecutando, y nunca se usa `delay()`.

### 3.9 Diagnóstico por consola

`GestorActuadores::actualizar()` imprime en cada ciclo un bloque con temperatura, humedad, nivel
de NH₃ clasificado, distancia de agua y estado de los cuatro relés. Con el muestreo por defecto
sale un bloque cada 2 s a 115200 baudios. Es la herramienta principal de verificación en banco,
dado que el proyecto no tiene suite de pruebas.

## 4. Parámetros de Configuración

| Constante | Valor por Defecto | Archivo de Origen | Descripción |
|---|---|---|---|
| `K1_PIN` | `32` | `include/config.h` | Relé de calefacción |
| `K2_PIN` | `25` | `include/config.h` | Relé de ventilador |
| `K3_PIN` | `27` | `include/config.h` | Relé de extractor |
| `K4_PIN` | `14` | `include/config.h` | Relé de bomba |
| `TEMP_FRIO` | `27.0` | `include/config.h` | Por debajo, se activa calefacción |
| `TEMP_CALOR` | `32.0` | `include/config.h` | A partir de aquí, se ventila |
| `HUM_EXTRACTORES` | `65.0` | `include/config.h` | Humedad que dispara extracción |
| `NH3_ALTO` | `1500` | `include/config.h` | Gas que inhibe calefacción y fuerza ventilación |
| `NIVEL_BOMBA_ON` | `6.0` | `include/config.h` | Distancia en cm que enciende la bomba |
| `NIVEL_BOMBA_OFF` | `3.0` | `include/config.h` | Distancia en cm que la apaga |
| `EN1_PIN` / `IN1_PIN` / `IN2_PIN` | `5` / `18` / `19` | `include/config.h` | L293D canal A, persiana |
| `EN2_PIN` / `IN3_PIN` / `IN4_PIN` | `21` / `22` / `23` | `include/config.h` | L293D canal B, sinfín |
| `INTERVALO_PERSIANA` | `300000` | `include/config.h` | Espera entre ciclos de persiana (5 min) |
| `DURACION_PERSIANA` | `3000` | `include/config.h` | Tiempo de recorrido de la lona |
| `PAUSA_PERSIANA` | `500` | `include/config.h` | Permanencia abierta antes de cerrar |
| `INTERVALO_ALIMENTO` | `300000` | `include/config.h` | Espera entre dispensados (5 min) |
| `DURACION_ALIMENTO` | `300000` | `include/config.h` | Tiempo de giro del sinfín (5 min) |
| `SERVO_PIN` | `2` | `include/config.h` | PWM de la puerta |
| `SERVO_DURACION_GIRO` | `300` | `include/config.h` | Recorrido mecánico estimado del servo |
| `SERVO_TIEMPO_ABIERTA` | `2000` | `include/config.h` | Permanencia abierta sin presencia |
| `ANGULO_CERRADA` | `0` | `include/ControlServo.h` | Posición de puerta cerrada |
| `ANGULO_ABIERTA` | `90` | `include/ControlServo.h` | Posición de puerta abierta |

### 4.1 Desviaciones de la regla de fuente única

Todos los parámetros de esta capa viven ya en `config.h`. Queda un residuo:

- **`SERVO_NEUTRO` (93) está definido en `config.h` y no lo usa nadie.** Corresponde al punto
  muerto de un servo de rotación continua; el firmware actual usa un servo de posición. Es
  configuración huérfana y puede retirarse.

> `ControlServo.h` definía valores de respaldo con `#ifndef` para `SERVO_PIN`,
> `SERVO_DURACION_GIRO`, `SERVO_TIEMPO_ABIERTA` y los tres macros `LOG_*`. Nunca llegaban a
> aplicarse porque la cabecera incluye `config.h` antes, pero el respaldo de `SERVO_PIN` era
> **GPIO 18**, que en el hardware real es `IN1_PIN` de la persiana: bastaba reordenar los includes
> para que el servo escribiera sobre el control del motor. El bloque se eliminó y
> `ANGULO_CERRADA` / `ANGULO_ABIERTA` pasaron a `config.h`.

### 4.2 Constantes de temporización sospechosas

`DURACION_ALIMENTO` e `INTERVALO_ALIMENTO` valen ambas 300000 ms. El tornillo sinfín gira durante
5 minutos seguidos y descansa otros 5: un ciclo útil del 50 % permanente. Para un dispensador de
pienso lo esperable es un pulso de segundos. La cabecera de `Alimentador.h` describe además un
«100 % PWM» que el código contradice, porque llama a `iniciarMotor(128)`.

En la persiana, `PAUSA_PERSIANA` de 500 ms es menor que la cadencia real de actuación de 2 s
(§1.1), de modo que la lona empieza a cerrarse en el mismo tick en que termina de abrirse. La
renovación de aire efectiva es de unos 3 s cada 5 minutos.

Ninguna de las dos se ha modificado: son valores de configuración y su ajuste corresponde a quien
conozca la mecánica instalada. Quedan señaladas para revisión en banco.

## 5. Manejo de Errores y Fail-Safe

### 5.1 Configuración de seguridad

`GestorActuadores::failSafe()` aplica lo exigido por el SDD §7.3:

| Actuador | Estado en fail-safe | Motivo |
|---|---|---|
| K1 calefactor | OFF (`HIGH`) | Elimina el riesgo de sobrecalentamiento |
| K2 ventilador | OFF (`HIGH`) | Sin lectura fiable no hay criterio para ventilar |
| K3 extractor | OFF (`HIGH`) | Igual que K2 |
| K4 bomba | **ON** (`LOW`) | El suministro de agua se mantiene durante la contingencia |
| Puerta | Cerrada de emergencia | Contiene a los animales dentro del galpón |
| Persiana | Detenida | Ningún mecanismo queda en movimiento |
| Alimentador | Detenido | Ídem |

K4 es la única salida que se **fuerza activa**: quedarse sin agua es peor que un exceso de riego
temporal.

### 5.2 Dos caminos hacia el fail-safe

El sistema puede entrar en configuración segura por dos vías distintas, y en la práctica la
primera se adelanta siempre a la segunda:

1. **Vía local.** `GestorActuadores::actualizar()` recibe `enErrorDHT` y `enErrorUltrasonico` como
   argumentos y, si alguno es cierto, llama a `failSafe()` y retorna antes de calcular nada. Actúa
   en el mismo ciclo en que un sensor acumula 3 fallos consecutivos.
2. **Vía global.** `SistemaFSM` acumula `fallosAcumulados` y transita a `ERROR`, donde
   ejecuta el fail-safe completo —relés, puerta, persiana y sinfín— y encola el evento para que
   Core 1 lo publique. Necesita 3 ciclos más, unos 6 s.

Los relés quedan seguros por la vía local; el cierre de puerta, la parada de mecanismos y la
notificación al backend llegan por la vía global.

**La vía local se repite; su traza no.** Mientras el sensor siga en error, `actualizar()` vuelve a
llamar a `failSafe()` cada 2 s y los cuatro relés se reescriben en cada pasada: reafirmar la
configuración segura cuesta cuatro `digitalWrite` y cierra la puerta a cualquier deriva. Lo que sí
está guardado por flanco es el log. `enFailSafe_` se levanta en la primera aplicación y no se baja
hasta que `aplicarControl()` vuelve a ejecutarse, de modo que `FAIL-SAFE ACTIVADO` y `Sensor en
error` se imprimen una vez por episodio y no 30 veces por minuto. Cuando el log muestra el mensaje
repetido es porque el sistema está oscilando entre `ERROR` y `MONITORING`, que es información real
y no ruido.

### 5.3 El fail-safe libera el modo MANUAL

`failSafe()` limpia las cuatro banderas de MANUAL antes de escribir los relés. El razonamiento:

- Durante la emergencia la configuración de seguridad debe aplicarse siempre, también sobre relés
  bajo control remoto. Ninguna orden del backend puede impedir que el calefactor se apague.
- Al recuperarse los sensores, los cuatro relés vuelven al lazo automático por sí solos. Sin esa
  limpieza, `aplicarControl()` seguiría saltándose los relés marcados como MANUAL y **quedarían
  congelados en el valor de emergencia** —calefactor apagado, bomba encendida— de forma indefinida.

La contrapartida, documentada en el ICD §5.2, es que **una emergencia cancela los modos manuales en
curso**. El backend que quiera mantener una orden manual debe reenviarla tras observar que el nodo
salió de `ERROR`.

### 5.4 Defecto corregido: la lectura inválida encendía el calefactor

Hasta la revisión de defectos, la tarea de control degradaba una lectura inválida del DHT22 a `0.0 °C`
antes de pasarla al lazo. Como `TEMP_FRIO` vale 27, la comparación `0.0 < 27` daba verdadero y
**el calefactor se encendía** durante los dos ciclos que tarda `enError()` en activarse, unos 4 s.
Un `NaN` transitorio del sensor bastaba para energizar la calefacción.

Ahora se arrastra la última lectura válida y, si nunca hubo ninguna, se declara el sensor en error
para que el gestor entre directamente en configuración de seguridad.

### 5.5 Comportamiento ante comandos inválidos

- **Número de relé fuera de 1-4.** `establecerManual()` y `establecerAutomatico()` emiten
  `LOG_WARN` y retornan sin tocar nada.
- **Nombre de actuador desconocido.** `releDesdeNombre()` devuelve `0` y `ClienteMQTT` descarta el
  comando antes de encolarlo.
- **Actuador sin mando remoto** (`alimentador`, `persiana`, `puerta`). Mismo tratamiento: se
  descarta con advertencia. Estos tres mecanismos solo obedecen a sus FSM temporizadas.

### 5.6 Degradación sin red

Ningún módulo de esta capa consulta el estado de Wi-Fi ni de MQTT. Con el enlace caído el lazo
sigue ejecutándose con los umbrales compilados en `config.h`, que es lo que el SDD §5.2 llama
*Local Fallback*. La diferencia con el objetivo del SDD es que no hay setpoints remotos que
recordar: al no existir NVS ni configuración dinámica, el modo degradado y el normal usan
exactamente los mismos valores.
