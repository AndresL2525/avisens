# Protocolo MQTT — Módulos `ClienteMQTT` y `ConexionWiFi`

## 1. Propósito y Responsabilidad

Cubre la pila de comunicaciones completa, en sus dos capas:

| Módulo | Archivos | Responsabilidad |
|---|---|---|
| `ConexionWiFi` | `include/ConexionWiFi.h`, `src/ConexionWiFi.cpp` | Enlace 802.11: asociación, reconexión, IP |
| `ClienteMQTT` | `include/ClienteMQTT.h`, `src/ClienteMQTT.cpp` | Sesión MQTT, contrato de datos con el backend |

`ClienteMQTT` implementa el contrato de `icd_avisens_mqtt_backend_md.md` sobre PubSubClient.
Sustituye al antiguo `ServicioAPI`, que hablaba HTTP REST con autenticación JWT y fue eliminado
del proyecto.

La clase se encarga de establecer y mantener la sesión con el broker, armar el Last Will and
Testament, suscribirse a los comandos de actuador, serializar cada payload de telemetría y
traducir los enumerados internos a los literales que espera el backend.

Ambos se ejecutan íntegramente en Core 0. `actualizar()` bloquea mientras hay tráfico de red, de
modo que llamarla desde la tarea de control provocaría un reinicio por watchdog.

## 2. Conexión Hardware y Pinout

No aplica: los dos módulos son puramente de software y usan el periférico Wi-Fi integrado del
ESP32, `ClienteMQTT` a través de un `WiFiClient` propio.

Una nota eléctrica sí es relevante: el radio Wi-Fi demanda picos de corriente de varios cientos de
miliamperios durante la asociación. Una alimentación compartida con las bobinas de relé o con los
motores provoca caídas de tensión que se manifiestan como desconexiones aparentemente aleatorias.

## 3. Modelo Matemático / Lógica

### 3.1 Espacio de tópicos

Raíz `avisens/{device_id}/`, con `device_id` tomado de `MQTT_DEVICE_ID`.

| Tópico | Sentido | Retain | Cadencia |
|---|---|:---:|---|
| `telemetria/dht22` | Publicación | No | 5 s |
| `telemetria/gases` | Publicación | No | 5 s |
| `telemetria/peso` | Publicación | No | 10 s |
| `telemetria/nivel_agua` | Publicación | No | 10 s |
| `telemetria/obstaculo` | Publicación | No | Por transición |
| `telemetria/diagnostico` | Publicación | No | 60 s |
| `telemetria/eventos` | Publicación | No | Por evento: fail-safe o presencia trabada |
| `actuadores/estado` | Publicación | Sí | Ante cambio |
| `actuadores/+/set` | Suscripción | — | — |
| `status/lwt` | Publicación y LWT | Sí | Al conectar y al caer |

### 3.2 Calidad de servicio

PubSubClient **solo publica en QoS 0**; la suscripción sí admite QoS 1, que es el que se usa para
`actuadores/+/set`. La consecuencia operativa es asimétrica y deliberada:

- **Los comandos no se pierden.** Es la dirección que importa para la seguridad del galpón: una
  orden de apagar el calefactor debe llegar.
- **La telemetría puede perder muestras.** Un paquete perdido se corrige solo en la siguiente
  publicación, a los 5 o 10 segundos.

Si el proyecto llegara a exigir QoS 1 también en publicación, hay que sustituir PubSubClient por
AsyncMqttClient, que el SDD contempla como alternativa en su Apéndice B. Eso cambiaría el modelo
de concurrencia, porque sus callbacks se ejecutan en el contexto de la tarea TCP asíncrona y no
dentro de una llamada propia.

### 3.3 Presencia mediante LWT

El mensaje de última voluntad se registra en el propio paquete CONNECT, antes de que exista
sesión. El broker lo guarda y lo publica él mismo si el nodo desaparece sin enviar DISCONNECT,
que es justo el caso de un corte de energía. Al conectar con éxito, el cliente sobreescribe ese
tópico con el estado en línea y su IP. Ambos mensajes van retenidos, de modo que un backend que
se suscriba más tarde conoce el estado sin esperar al siguiente evento.

### 3.4 Encaminamiento de comandos

La suscripción usa un comodín de un nivel, `actuadores/+/set`, que cubre los siete actuadores del
contrato con una sola suscripción. El nombre del actuador se extrae de la posición del segmento
`actuadores/` dentro del tópico recibido.

`GestorActuadores::releDesdeNombre()` traduce ese nombre a número de relé y devuelve cero cuando
el actuador no admite control remoto. Hoy devuelven cero `alimentador`, `persiana` y `puerta`:
los tres funcionan solo en automático, gobernados por sus propias máquinas de estado
temporizadas, y sus comandos se descartan con una advertencia por consola.

El manejador registrado en `begin()` no toca el hardware. Encola el comando para que Core 1 lo
aplique, según la regla de propiedad descrita en `01_arquitectura_freertos.md`.

### 3.5 Capacidad de los buffers

El buffer de PubSubClient se eleva a `MQTT_BUFFER_SIZE` porque su valor por defecto de 256 bytes
no admite el payload consolidado de `actuadores/estado`, que describe siete actuadores con sus
modos y estados de FSM.

Cada payload se serializa con `StaticJsonDocument`, dimensionado por tópico mediante las
constantes `JSON_CAPACIDAD_*`. Se prefiere la variante estática sobre `DynamicJsonDocument`
porque reserva en pila y evita fragmentar el heap en un nodo que publica de forma continua
durante semanas.

### 3.6 Capa de enlace: `ConexionWiFi`

`ClienteMQTT` da por hecho que hay red. Quien la establece y la mantiene es `ConexionWiFi`, con
una máquina implícita de dos estados y ninguna espera bloqueante.

```
comenzar()  ─► WIFI_STA + hostname + WiFi.begin()   (no espera asociación)
                        │
actualizar() ─► ¿WL_CONNECTED?
                 ├─ sí  ─► flanco de subida: traza la IP obtenida
                 └─ no  ─► flanco de bajada: traza la caída
                           cada INTERVALO_RECONEXION_MS: WiFi.reconnect()
```

Tres decisiones de diseño merecen explicación:

- **`comenzar()` no espera.** El patrón habitual de Arduino, un `while (WiFi.status() != WL_CONNECTED) delay(500)`,
  bloquearía `setup()` durante decenas de segundos con un punto de acceso ausente y el nodo nunca
  llegaría a arrancar sus tareas. Aquí la asociación se resuelve en segundo plano y el lazo de
  control empieza a funcionar de inmediato, con o sin red.
- **Modo estación explícito.** `WIFI_STA` desactiva el punto de acceso que el ESP32 levanta por
  defecto, que consume memoria y expone una red abierta.
- **Reintento espaciado 30 s.** `INTERVALO_RECONEXION_MS` es una constante privada de la clase, no
  una macro de `config.h`. El valor es deliberadamente alto: `WiFi.reconnect()` consume corriente
  y reintentar cada segundo contra un router apagado no acelera nada.

`estaConectado()` contrasta su bandera interna con `WiFi.status()` en cada llamada, de modo que
una desconexión detectada por la pila se refleja aunque `actualizar()` no haya corrido todavía.
`ClienteMQTT::actualizar()` consulta este método antes de intentar nada, y por eso una caída de
Wi-Fi no produce intentos de conexión TCP contra una interfaz muerta.

Las credenciales llegan por constructor desde `src/Nodo.cpp`, tomadas de `WIFI_SSID` y `WIFI_PASS` si
están definidas en `build_flags`. **Si no lo están, se compila con las credenciales de prueba
incrustadas en el código**, que es lo que ocurre hoy con la configuración por defecto.

`setHostname()` y `desconectar()` están implementados y ningún camino del firmware los invoca. El
hostname se fija en el constructor con el valor `galponsmart`.

## 4. Parámetros de Configuración

| Constante | Valor por Defecto | Archivo de Origen | Descripción |
|---|---|---|---|
| `MQTT_BROKER_HOST` | `"192.168.1.100"` | `include/config.h` | Dirección del broker |
| `MQTT_BROKER_PORT` | `1883` | `include/config.h` | Puerto TCP sin TLS |
| `MQTT_DEVICE_ID` | `"galpon_01"` | `include/config.h` | Identificador del galpón |
| `MQTT_USUARIO` | `""` | `include/config.h` | Usuario del broker; vacío lo omite |
| `MQTT_CLAVE` | `""` | `include/config.h` | Clave del broker; vacía la omite |
| `MQTT_KEEPALIVE_S` | `15` | `include/config.h` | Intervalo de PINGREQ |
| `MQTT_BUFFER_SIZE` | `768` | `include/config.h` | Buffer interno de PubSubClient |
| `MQTT_REINTENTO_MS` | `5000` | `include/config.h` | Espera entre intentos de conexión |
| `MQTT_QOS_COMANDOS` | `1` | `include/config.h` | QoS de suscripción y del LWT |
| `INTERVALO_TELEMETRIA_RAPIDA` | `5000` | `include/config.h` | Cadencia de DHT22 y gases |
| `INTERVALO_TELEMETRIA_LENTA` | `10000` | `include/config.h` | Cadencia de peso y nivel de agua |
| `INTERVALO_DIAGNOSTICO` | `60000` | `include/config.h` | Cadencia del latido de salud |
| `JSON_CAPACIDAD_TELEMETRIA` | `192` | `include/config.h` | Capacidad de payloads de sensor |
| `JSON_CAPACIDAD_COMANDO` | `256` | `include/config.h` | Capacidad de comandos entrantes |
| `JSON_CAPACIDAD_DIAGNOSTICO` | `256` | `include/config.h` | Capacidad del diagnóstico |
| `JSON_CAPACIDAD_EVENTO` | `384` | `include/config.h` | Capacidad de eventos de falla |
| `JSON_CAPACIDAD_ESTADO` | `640` | `include/config.h` | Capacidad del estado consolidado |
| `WIFI_SSID` | — | `platformio.ini` | SSID de la red; sin él se usa el valor incrustado |
| `WIFI_PASS` | — | `platformio.ini` | Clave de la red; sin ella se usa el valor incrustado |
| `INTERVALO_RECONEXION_MS` | `30000` | `include/ConexionWiFi.h` | Espera entre reintentos de asociación |
| `PERIODO_TAREA_RED_MS` | `100` | `include/config.h` | Periodo con que se sirven ambas capas |

Las credenciales y la dirección del broker admiten sobreescritura desde `platformio.ini` con
`-D` en `build_flags`, para no versionarlas en el repositorio:

```ini
build_flags =
    -DMQTT_BROKER_HOST='"10.0.0.5"'
    -DMQTT_DEVICE_ID='"galpon_02"'
    -DMQTT_USUARIO='"avisens"'
    -DMQTT_CLAVE='"secreto"'
    -DWIFI_SSID='"RedGalpon"'
    -DWIFI_PASS='"clave-wifi"'
```

`INTERVALO_RECONEXION_MS` es la única constante de temporización del firmware que no vive en
`config.h`: es una `static constexpr` privada de `ConexionWiFi`. Conviene trasladarla cuando se
revise el módulo.

## 5. Manejo de Errores y Fail-Safe

- **Sin Wi-Fi.** `ClienteMQTT::actualizar()` retorna de inmediato. No se intenta conexión TCP ni se
  consume CPU.
- **Caída del enlace.** `ConexionWiFi::actualizar()` detecta el flanco de bajada, lo traza y lanza
  `WiFi.reconnect()` cada 30 s. No hay límite de reintentos ni reinicio del nodo: un galpón sin red
  sigue siendo un galpón controlado, y reiniciar solo perdería el estado de los actuadores.
- **Credenciales por defecto.** Si `WIFI_SSID` y `WIFI_PASS` no están definidos en `build_flags`,
  `Nodo.cpp` compila con credenciales de prueba incrustadas. El nodo no asociará y el firmware no
  lo distingue de un router apagado: revisar la traza de consola en la primera puesta en marcha.
- **Sin broker.** Se reintenta cada `MQTT_REINTENTO_MS`. El código de error de PubSubClient se
  registra por consola. El contador de reconexiones viaja en `telemetria/diagnostico` como
  indicador de inestabilidad del enlace.
- **Publicación fallida.** `publicar()` devuelve `false` y deja traza. No se reintenta ni se
  almacena: la siguiente publicación periódica sustituye al dato perdido. El buffer de
  persistencia en RAM que contempla el SDD §5.2 está pendiente de implementar.
- **JSON de comando inválido.** Se descarta con traza de error y no se altera ningún actuador.
- **Actuador desconocido o sin control remoto.** Se descarta con advertencia. El contrato del
  backend se mantiene estable aceptando `humidificador` como alias en desuso de `ventilador`.
- **Caída del nodo.** El broker publica el LWT retenido en `status/lwt`. Es el único mecanismo por
  el que el backend distingue un galpón apagado de uno simplemente silencioso.
- **Independencia del control.** Ningún fallo de este módulo afecta al lazo ambiental: la tarea de
  control corre en el otro core y no consulta el estado de la sesión MQTT.
