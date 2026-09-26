# Documentación Externa del Firmware

Los manuales de este directorio son la documentación funcional del firmware, tal como exige el
SDD §8.3. El código fuente no lleva comentarios a propósito: las cabeceras de `include/` declaran
sin Doxygen y los `.cpp` solo conservan notas de hardware. **El contrato de cada clase y cada
función vive aquí y en ningún otro sitio**, de modo que estos manuales no son material de apoyo
sino la referencia de la API.

| Manual | Contenido |
|---|---|
| [`01_arquitectura_freertos.md`](01_arquitectura_freertos.md) | Cores, tareas, colas, buzones y watchdog |
| [`02_sensores_calibracion.md`](02_sensores_calibracion.md) | Los cinco sensores, el filtro de media móvil y la calibración de la celda |
| [`03_control_pid_actuadores.md`](03_control_pid_actuadores.md) | Política de control, relés, servo y canales del L293D |
| [`04_protocolo_mqtt_interfaz.md`](04_protocolo_mqtt_interfaz.md) | Enlace Wi-Fi, sesión MQTT y contrato de datos con el backend |
| [`05_maquinas_de_estado.md`](05_maquinas_de_estado.md) | Tablas de transición de las cuatro FSM y del modo de cada relé |

Todos siguen la plantilla del SDD §8.4: propósito, pinout, lógica, parámetros y manejo de errores.

## Cobertura del código fuente

Cada archivo de `include/` y `src/` está documentado en el manual indicado.

| Archivo | Manual | Apartado principal |
|---|---|---|
| `include/config.h` | Transversal | §4 de los cinco manuales |
| `src/main.cpp` | 01 | `setup()` y `loop()` |
| `include/Nodo.h`, `src/Nodo.cpp` | 01 | §1.2 |
| `include/TareaControl.h`, `src/TareaControl.cpp` | 01 | §1 y §3.3 |
| `include/TareaRed.h`, `src/TareaRed.cpp` | 01 y 04 | §3.4 y §3 |
| `include/Mensajeria.h`, `src/Mensajeria.cpp` | 01 | §3.1 y §3.2 |
| `include/SistemaFSM.h`, `src/SistemaFSM.cpp` | 05 | §3.1 |
| `include/ConsolaSerie.h`, `src/ConsolaSerie.cpp` | 05 | §4.1 |
| `include/DetectorGradiente.h`, `src/DetectorGradiente.cpp` | 03 | §3.9 |
| `include/MovingAverage.h` | 02 | §3.1 |
| `include/SensorDHT.h`, `src/SensorDHT.cpp` | 02 | §3.2 |
| `include/SensorMQ135.h`, `src/SensorMQ135.cpp` | 02 | §3.3 |
| `include/SensorUltrasonico.h`, `src/SensorUltrasonico.cpp` | 02 | §3.4 |
| `include/SensorPeso.h`, `src/SensorPeso.cpp` | 02 | §3.5 y §4.1 |
| `include/SensorKY032.h`, `src/SensorKY032.cpp` | 02 | §3.6 |
| `include/Actuador.h`, `src/Actuador.cpp` | 03 | §3.6 |
| `include/GestorActuadores.h`, `src/GestorActuadores.cpp` | 03 y 05 | §3.2 a §3.5 y §3.5 |
| `include/ControlServo.h`, `src/ControlServo.cpp` | 03 y 05 | §3.7 y §3.2 |
| `include/Persiana.h`, `src/Persiana.cpp` | 03 y 05 | §3.8 y §3.3 |
| `include/Alimentador.h`, `src/Alimentador.cpp` | 03 y 05 | §3.8 y §3.4 |
| `include/ConexionWiFi.h`, `src/ConexionWiFi.cpp` | 04 | §3.6 |
| `include/ClienteMQTT.h`, `src/ClienteMQTT.cpp` | 04 | §3.1 a §3.5 |

## Documentos relacionados

| Documento | Papel |
|---|---|
| `../../sdd_avisens.md` | Documento de Diseño de Software. Especificación normativa |
| `../../icd_avisens_mqtt_backend_md.md` | Contrato de interfaz MQTT con el equipo de backend |
| `../ARCHITECTURE.md` | Cómo la especificación se refleja en el firmware; §1.3 lista lo pendiente |

`README.md`, `QUICK_START.md` y `CHANGELOG.md` de la raíz describen la v7.0 y están obsoletos.

## Convenciones

- Todo se escribe en español: identificadores, documentación y mensajes de consola.
- **Las cabeceras no llevan Doxygen.** El SDD §8.2 lo permitiría acotado a `@brief`, `@param` y
  `@return`, pero el proyecto optó por no usarlo: los nombres describen la intención y el
  contrato se documenta aquí. Quien añada una clase nueva no debe reintroducirlo.
- Los comentarios que sobreviven en `.cpp` son exclusivamente de hardware: lógica invertida del
  relé, pulso 25 del HX711, TRIG de 10 µs del HC-SR04, colector abierto del KY-032 y las cesiones
  de `vTaskDelay(1)` tras escribir en el L293D o en el servo.
- `include/config.h` es la fuente única de pines, umbrales, tiempos y tipos de estado. Las
  desviaciones vigentes están señaladas en el manual 02 §4 y en el 03 §4.1.
- Los defectos conocidos se documentan donde corresponde en lugar de ocultarse. Un apartado §5 que
  describe un fallo está describiendo el firmware real, no el deseado.
