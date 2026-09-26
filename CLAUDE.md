# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository layout

- `proyecto_iot/` — the ESP32 firmware (PlatformIO). This is the only code in the repo.
- Root `*.md` files — design documents. See "Documentation hierarchy" below; they are not incidental notes, they are the spec the firmware is measured against.

Everything (code identifiers, comments, docs, commit messages) is written in **Spanish**. Match that.

## Build and flash

```powershell
cd proyecto_iot
pio run                  # build
pio run -t upload        # flash
pio device monitor       # serial monitor, 115200 baud
pio run -t clean         # clean build
```

**The env is `esp32dev`, not `esp32`.** `README.md` and `QUICK_START.md` both document `pio run -e esp32`, which fails — there is no such env in `platformio.ini`. Since there is only one env, omit `-e` entirely.

`platformio.ini` hardcodes `upload_port = COM10` / `monitor_port = COM10`. On a different port, override with `pio run -t upload --upload-port COMx` rather than editing the file.

There is **no test suite** and no `test/` directory. Verification is done by flashing and reading the serial monitor.

### Serial commands (typed into the monitor at runtime)

- `TARA` — zero the HX711 load cell
- `REARME` / `RESET` — restart the global FSM from `INIT`

## Documentation hierarchy

This matters more than usual here, because the documents disagree with each other and with the code.

1. **`sdd_avisens.md`** — the Software Design Document. **This is the governing spec.**
2. **`icd_avisens_mqtt_backend_md.md`** — the MQTT interface contract with the backend team.
3. **`proyecto_iot/ARCHITECTURE.md`** — how the spec maps onto the firmware. Its §1.3 tracks, point by point, where the firmware does not yet satisfy the SDD.

`README.md`, `QUICK_START.md` and `CHANGELOG.md` describe **v7.0** and are stale — they predate the HX711, the backend client and the global FSM. Do not trust them for current behavior; prefer `ARCHITECTURE.md` and the code.

### Transport: MQTT (migrated)

The firmware talks MQTT via `ClienteMQTT` (PubSubClient). The old HTTP REST + JWT client (`ServicioAPI`) has been deleted. One caveat the contract depends on: **PubSubClient publishes at QoS 0 only** — subscriptions get QoS 1, so commands are delivered reliably but telemetry can drop samples. Moving to true QoS 1 publishing means swapping in AsyncMqttClient, which changes the concurrency model.

Still pending against the SDD: PID control (the loop is plain threshold on/off), setpoints in NVS, presence debounce, and the stuck-sensor alarm. `ARCHITECTURE.md` §1.3 tracks these.

Where the SDD described the *hardware* it was wrong, and those parts have been corrected against `include/config.h`: the pinout (§4.1), the fail-safe strategy (§7.2, §7.3, including that the active-low relays make `HIGH` the safe state), and the actuator literals (§6.3-B, where K2 is canonically `ventilador` — the galpón has no humidifier, and `humidificador` survives only as a deprecated alias). The pinout table in the SDD is now derived from `config.h`; treat `config.h` as the source of truth for anything electrical.

## Architecture

### Application layer

`src/main.cpp` holds only `setup()` and `loop()`. Everything else that used to live there is split into one-responsibility modules, same header/implementation pattern as the peripherals:

- `Nodo` — the **only** file defining global instances (sensors, actuators, `conexionWiFi`, `clienteMQTT`, `sistemaFSM`, `detectorGradiente`), reached elsewhere via `extern` through `Nodo.h`; `iniciarPerifericos()` runs every `begin()`. **Wi-Fi credentials live here**: `ConexionWiFi` takes `WIFI_SSID` / `WIFI_PASS` from `build_flags` if defined, otherwise the placeholder literals marked `// Aquí colocar nombre de red` / `// Aquí colocar contraseña`. Prefer `build_flags` so the password stays out of Git.
- `Mensajeria` — static class owning the four queues; the only API for crossing the core boundary. `Mensajeria::encolarComando` is the callback registered with `clienteMQTT.begin()`.
- `SistemaFSM` — global state, boot/fault counters, `avanzar()`, `evaluarSensoresCriticos()`, `enFailSafe()`.
- `ConsolaSerie` — `Serial.begin`, banner, `TARA` / `REARME`.
- `DetectorGradiente` — abrupt ΔT warning (log only, no action yet).
- `TareaControl` / `TareaRed` — the two task bodies. `aplicarComandosPendientes()` and `publicarSnapshot()` are file-static helpers in `TareaControl.cpp`.

### Dual-core split

`setup()` pins two FreeRTOS tasks and then `loop()` does nothing:

- **`tareaControl`** (Core 1, prio 2, 16 KB stack, 10 ms period) — reads every sensor, drives every actuator, runs the global FSM. Subscribed to the 10 s watchdog, so **no network I/O may happen inside it**.
- **`tareaRed`** (Core 0, prio 1, 8 KB stack, 100 ms period) — Wi-Fi upkeep, MQTT session, publishing and command intake. Deliberately *not* watchdog-subscribed, because network calls block for seconds. The 100 ms period is what keeps `mqtt_.loop()` serviced and command latency under the 200 ms the SDD requires.

**No object is shared between cores.** Core 1 owns the sensors and actuators, Core 0 owns the network session, and every crossing copies a struct through a FreeRTOS queue: `buzonTelemetria` and `buzonActuadores` are length-1 mailboxes (`xQueueOverwrite`/`xQueuePeek`) for continuous state, `colaComandos` and `colaEventos` are FIFOs for discrete events. Keep new cross-task data on that pattern rather than reaching across. See `proyecto_iot/docs/01_arquitectura_freertos.md`.

### `include/config.h` is the single source of truth

It holds all GPIO assignments, all control thresholds, all timing intervals, every `enum class` state type, every `Lectura*` struct, and the `LOG_DEBUG` / `LOG_WARN` / `LOG_ERROR` macros. Tuning behavior means editing this file, not the modules.

Pin assignments have no duplicates, but several carry ESP32 caveats (strapping pins, boot pulses, active-low relays floating at reset). `ARCHITECTURE.md` §1.4 has the annotated map.

### Peripheral module pattern

Every sensor and actuator follows the same shape, and new ones should too:

- Header in `include/`, implementation in `src/`, one class per peripheral.
- Pins come from `config.h` macros read in the constructor — **not** passed as constructor arguments.
- `begin()` for pin setup, then `leer()` (sensors) or `actualizar()` (actuators) called each cycle.
- Sensors expose `getUltimaLectura()`, a failure counter reaching `MAX_FALLOS_SENSOR`, `enError()` and `reset()`.
- Actuators with timing own a private FSM (`EstadoPuerta`, `EstadoPersiana`, `EstadoAlimentador`) advanced by `millis()` comparisons — never `delay()`.

`MovingAverage<T, SIZE>` (header-only template) is the shared filter, used by the MQ-135 and the ultrasonic sensor.

### `GestorActuadores` — relay arbitration

Owns K1-K4 and is the only place control policy lives. Two things are non-obvious:

- **AUTO/MANUAL flags.** A relay placed in MANUAL by a backend command is frozen at that value; `aplicarControl()` skips it until an `AUTO` command releases it. `failSafe()` clears all four flags before writing the relays, so an emergency cancels every manual mode in flight and recovery returns to AUTO. While the FSM is in `ERROR` or `SHUTDOWN`, `aplicarComandosPendientes()` drains and **discards** incoming commands — otherwise a MANUAL command could re-energise the heater during a fail-safe.
- **Name mapping.** `releDesdeNombre()` translates the backend's actuator literals to relay numbers. The hardware has no humidifier — K2 is a fan, so the canonical literal is `"ventilador"`; `"humidificador"` is still accepted as a deprecated alias until the backend migrates. Do not drop the alias unilaterally.

### Fail-safe chain

Sensor read fails → per-sensor counter increments → at 3 consecutive failures `enError()` goes true → global FSM enters `ERROR` → `failSafe()` cuts K1-K3 and forces K4 (pump) on, closes the door, stops the feeder and blind. Recovery is automatic once the sensors read clean again.

## Conventions

- **The code carries no comments** beyond hardware notes (SDD §8.2): no long explanatory blocks, no tutorials, no commented-out code. Headers in `include/` declare with **no Doxygen at all** — the SDD would allow `@brief` / `@param` / `@return`, but the project chose to drop it, so do not reintroduce it. The only inline comments left in `.cpp` are hardware quirks and silicon workarounds (active-low relays, the HX711's 25th clock pulse, the HC-SR04 trigger pulse, the KY-032 open collector, the `vTaskDelay(1)` yields after writing to the L293D and the servo), two safety rationales in `TareaControl.cpp` (why an invalid DHT read must not become `0.0`, why commands are discarded in fail-safe), and the two credential placeholders in `Nodo.cpp` that the user asked for. The API contract lives in `proyecto_iot/docs/`, not in the sources.
- **No magic numbers.** Every threshold becomes a named constant in `config.h`.
- `vTaskDelay()` inside tasks, never `delay()`.
- `enum class` for all state types.

## Known defects

These are real and tracked in `ARCHITECTURE.md`; treat them as existing bugs rather than intended behavior:

- Telemetry ships the last good reading when the current one is invalid. That is deliberate — publishing `0.0` was worse — but it means `sensor_ok` / `estado_sensor` is the only freshness signal, and the stale value still goes out.
- `ACTUATION` and `SHUTDOWN` remain unreachable in the global FSM.
- The MQ-135 and the KY-032 have no failure detection at all: `valida` is hardcoded `true` and a disconnected sensor reads as a plausible value. Neither feeds the fail-safe.
- The KY-032 has no 50 ms debounce (SDD §4.2) and no stuck-sensor alarm (SDD §7.2, 120 s in `LOW`).
- Actuator FSMs advance at `INTERVALO_SENSORES` (2 s), not the 10 ms task period, because their `actualizar()` calls sit inside the sensor block. Any constant below 2 s rounds up — `PAUSA_PERSIANA` (500 ms) and `SERVO_DURACION_GIRO` (300 ms) do.
- `DURACION_ALIMENTO == INTERVALO_ALIMENTO == 5 min`, so the auger runs half the time, permanently. Suspected mis-set constant; needs bench validation before changing.
- No PID, no NVS, no remote setpoints (SDD §5.1). The loop is plain threshold on/off.

Fixed during the MQTT migration: blocking network I/O inside the watchdog task, the unsynchronised cross-core access, the two Serial readers competing for `TARA` at boot, and the unreachable `INIT`/`CALIBRATION` states.

Fixed in the v1.2.0 defect pass (all verified by build only — **no hardware validation**):

- An invalid DHT22 read used to enter the control loop as `0.0 °C`, which is below `TEMP_FRIO` (27) and **energised the heater** for up to two cycles. `TareaControl.cpp` now carries the last valid reading and declares the sensor in error if there has never been one.
- `SensorPeso::leerADC()` read the 24 bits inverted, never sign-extended the two's-complement word, used `0` as an error sentinel, and blocked up to 1 s per read (11.5 s in `tara()`, past the 10 s watchdog). It now returns `bool` with the value by reference, extends the sign, and uses `HX711_TIMEOUT_MS` (150 ms). **Weights change: the load cell must be recalibrated.**
- `failSafe()` now clears the four MANUAL flags, so relays return to AUTO after recovery instead of staying stuck at their emergency value.
- `SensorUltrasonico::leer()` still carries the last known distance, but no longer relabels it `OK` — the real failure state survives, so the pump holds rather than acting on old data.
- `ControlServo.h` lost its `#ifndef` fallback block, whose `SERVO_PIN 18` collided with the blind's `IN1_PIN`. `ANGULO_CERRADA` / `ANGULO_ABIERTA` moved to `config.h`.
- `LecturaDHT` and `LecturaPeso` are fully initialised on the failure paths; they used to return uninitialised `peso` / `voltaje` / `temperatura` fields.
- A MANUAL command received while in `ERROR` used to re-energise the relay and stay there: `aplicarComandosPendientes()` ran before the (edge-guarded) fail-safe and `actualizar()` never ran outside `MONITORING`. Commands are now discarded in `ERROR` / `SHUTDOWN`.
- `SHUTDOWN` re-ran `failSafe()` every 10 ms (100 `LOG_ERROR`/s). Now edge-guarded like `ERROR`.
- `SensorPeso::leer()` warned every 2 s forever when `TARA` had never been sent. It now warns once.
- `Serial.readStringUntil()` could block `tareaControl` for the 1 s default timeout; `setup()` now sets `SERIAL_TIMEOUT_MS` (50 ms).
- `ClienteMQTT::procesarComando()` compared `modo` case-sensitively (`"manual"` silently meant AUTO). It now upper-cases the value first.
- Fault events moved from `telemetria/diagnostico` to their own topic `telemetria/eventos`, so the heartbeat and the event schemas no longer share a topic.
