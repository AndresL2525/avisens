

---

## 🔴 BACKEND — Funcionalidades Completamente Faltantes

### 1. `AlertService` (Sección 4.5.4 del documento)
**Estado:** ❌ **NO EXISTE** — No hay archivo `app/services/alert_service.py`

El documento describe un servicio con 3 métodos críticos que **no están implementados**:

| Método documentado | Qué debería hacer | Estado real |
|---|---|---|
| `check_sensor_outliers(reading)` | Detectar gradientes térmicos abruptos (ΔT > 10°C en 5s) e incoherencias físicas | ❌ No existe |
| `evaluate_device_heartbeat(device_id)` | Tarea en background que verifica si un dispositivo no envía datos en >30s | ❌ No existe |
| `dispatch_critical_notification(event)` | Enviar notificación push/webhook ante eventos críticos | ❌ No existe |

**Impacto:** Las funcionalidades **F-09** (Alertas de Fallo y Datos Anómalos) y **F-11** (Detección de Dispositivo Offline) del documento **dependen 100% de este servicio** y actualmente son solo texto en el SSD.

---

### 2. Detección de Dispositivo Offline (F-11)
**Estado:** ❌ **NO IMPLEMENTADO**

El documento dice:
> *"Monitoreo pasivo en el backend que genera una alerta de desconexión si un galpón no emite lecturas de telemetría en más de 30 segundos consecutivos."*

**Lo que falta:**
- Cron job o background task (ej. con `APScheduler` o `asyncio` task) que consulte `sensor_readings` cada 30-60s.
- Lógica que compare `now - last_timestamp > 30s` por cada `device_id`.
- Inserción automática de evento `SISTEMA`/`ALERTA` en la colección `events` cuando se detecta offline.
- Endpoint o mecanismo para que la app móvil consulte estado de conectividad de los galpones.

---

### 3. Validación de Outliers en Lecturas (F-09 actualizado)
**Estado:** ❌ **NO IMPLEMENTADO**

El documento describe detección de:
- **Gradientes térmicos abruptos:** ΔT > 10°C en 5 segundos
- **Valores fuera de rango físico plausible:** temperatura < -10°C o > 60°C, humedad = 0% o 100% sostenida
- **Picos de ruido:** descarte de lecturas con delta > 3σ

**Lo que falta:**
- En `sensor_service.py`: lógica post-inserción que compare la nueva lectura con la anterior del mismo `device_id`.
- Regla de negocio que genere evento `ALERTA` cuando se detecte un outlier.
- El `SensorValidationException` existe en `exceptions.py`, pero **solo valida rangos estáticos del body** (Pydantic), no detecta anomalías entre lecturas consecutivas.

---

### 4. Carpeta de Tests
**Estado:** ❌ **NO EXISTE** (`avisens-backend/tests/` → 404 en GitHub)

El documento en el roadmap (v1.3) dice:
> *"Agregar tests con pytest (tests/ ya creado)"*

Pero la carpeta **no existe** en el repositorio. El `README.md` del backend sí menciona que hay tests planeados, pero no hay código.

---

## 🟡 BACKEND — Inconsistencias Menores

| Inconsistencia | Detalle |
|---|---|
| **Notificaciones push (FCM)** | Mencionadas en roadmap v1.5 pero no hay código ni servicio de notificaciones |
| **WebSockets (v2.0)** | Mencionado en roadmap pero no implementado |
| **Dashboard web** | Mencionado en roadmap v1.6 pero no existe |

---

## 🔴 FIRMWARE IoT — Funcionalidades Completamente Faltantes

### 5. `ServicioAPI.cpp` — Cliente HTTP hacia el Backend
**Estado:** ❌ **NO EXISTE**

El documento en el roadmap (v8.0) dice:
> *"Implementar `ServicioAPI.cpp` — HTTP REST hacia backend"*

**Lo que falta:**
- Clase `ServicioAPI` que use `HTTPClient` de Arduino para hacer `POST /sensors/readings`, `GET /actuators/commands`, `POST /actuators/state`.
- Manejo de JWT: almacenar el `access_token` recibido de `/auth/device/login` y enviarlo en header `Authorization: Bearer <token>`.
- Reintentos con backoff exponencial ante fallos de red.
- Cola/buffer en SPIFFS para no perder datos si la API cae.

**Impacto:** El ESP32 **nunca se comunica con el backend**. Solo está en modo standalone local.

---

### 6. Envío de Eventos de Falla desde ESP32 (Sección 5.9)
**Estado:** ❌ **NO IMPLEMENTADO**

El documento describe un flujo donde el ESP32, al detectar 3 fallos consecutivos en un sensor, envía un `POST /events` con JSON detallado:

```json
{
  "device_id": "galpon_01",
  "tipo": "FALLA_SENSOR",
  "origen": "SensorDHT",
  "mensaje": "DHT22 desconectado o sin respuesta (3 timeouts seguidos)",
  "nivel": "critico",
  "metadata": { "fallos_consecutivos": 3, "ultimo_valor_valido": {...} }
}
```

**Lo que falta:**
- En `main.cpp`, dentro del bloque `errorCritico`, no hay código que construya ni envíe este JSON.
- La `ConexionWiFi` solo gestiona WiFi, no tiene cliente HTTP.
- El `tareaWiFi` en Core 1 solo hace `conexionWiFi.actualizar()`, no envía datos.

**Lo que hay ahora:** Solo un `LOG_ERROR()` al puerto serial y activación local del fail-safe.

---

### 7. Clasificación de Errores en 3 Categorías (Sección 5.8)
**Estado:** 🟡 **PARCIALMENTE IMPLEMENTADO**

El documento describe 3 categorías de diagnóstico:

| Categoría | Descripción | Implementación actual |
|---|---|---|
| **Fallo de Bus / Sin Respuesta** | Sensor no responde a pulsos de inicio | ✅ `SensorUltrasonico` tiene `TIMEOUT`. `SensorDHT` detecta `isnan()`. |
| **Lectura Fuera de Rango Físico** | Valores imposibles (temp < -10°C, distancia > 400cm) | 🟡 `SensorUltrasonico` tiene `OUT_OF_RANGE`. `SensorDHT` **NO** valida rangos físicos (solo `isnan`). |
| **Picos de Ruido / Gradiente Excesivo** | Delta > 3 desviaciones estándar de la ventana | ❌ **NO IMPLEMENTADO** en ningún sensor. |

**Lo que falta:**
- En `SensorDHT.cpp`: validar que temperatura esté en [-10, 60] y humedad en [0, 100]. Descartar y marcar como `OUT_OF_RANGE` si no.
- En `main.cpp` o `SensorDHT`: mantener historial de últimas N lecturas y calcular desviación estándar para detectar picos de ruido.
- En `main.cpp`: detectar gradiente térmico (ΔT > 10°C en 5s) comparando con lectura anterior.

---

### 8. FSM Global del Sistema (Sección 5.3.1) — Desincronización Documento/Código
**Estado:** 🟡 **PARCIALMENTE IMPLEMENTADO — Desincronizado con el documento**

El documento describe transiciones basadas en variables formales:

| Variable documento | Significado | Implementación real en `main.cpp` |
|---|---|---|
| **C** | Contador de ciclos de arranque (≥ 10) | ✅ `ciclosTarea > 10` (similar) |
| **T** | Calibración HX711 completada | ❌ **NO existe**. La calibración ocurre en `setup()`, fuera de la FSM. |
| **F** | Fallos acumulados ≥ 3 | ✅ `sensorDHT.enError() \|\| sensorUltrasonico.enError()` |
| **R** | Comando de rearme manual | ❌ **NO existe**. No hay forma de forzar rearme desde serial ni app. |

**Problemas:**
- La transición `INIT → CALIBRATION` y `CALIBRATION → MONITORING` usa el mismo contador `ciclosTarea` (que es un contador infinito de ciclos del loop), no un contador específico de arranque.
- No hay estado `SHUTDOWN` accesible desde ningún comando.
- No hay comando serial para forzar transición `ERROR → INIT` (variable R).

---

## 🟢 Lo que SÍ está implementado correctamente

Para que no todo sea negativo, estas partes **sí coinciden** documento-código:

### Backend ✅
- Todos los routers (`auth`, `sensors`, `actuators`, `events`) con endpoints documentados
- Todos los services (`auth_service`, `sensor_service`, `actuator_service`)
- Modelos Pydantic con validaciones de rango
- JWT dual (device + user) con anti-spoofing
- Middleware CORS, Security Headers, Rate Limiting
- Health check en `/health`
- Docker multi-stage con usuario no-root
- Manejo global de excepciones
- Logging estructurado con structlog
- `docker-compose.yml` y `.env.example`

### IoT / Firmware ✅
- FSMs locales: `ControlServo`, `Persiana`, `Alimentador`
- `GestorActuadores` con lógica de umbrales y histéresis de bomba
- `MovingAverage` template con buffer circular
- `SensorUltrasonico` con estados `OK`, `TIMEOUT`, `OUT_OF_RANGE`, `ERROR`
- `SensorDHT` con contador de fallos y `enError()`
- Fail-safe local (`gestorActuadores.failSafe()`)
- Watchdog Timer (10s, panic=true)
- FreeRTOS dual core (`tareaGalpon` en Core 0, `tareaWiFi` en Core 1)
- `SensorPeso` (HX711) con calibración por serial

---

## 📋 Resumen Ejecutivo: Lista de Tareas Pendientes

| # | Tarea | Prioridad | Archivo(s) a crear/modificar |
|---|-------|-----------|------------------------------|
| 1 | Crear `AlertService` con detección de outliers | 🔴 Alta | `app/services/alert_service.py` |
| 2 | Implementar heartbeat de dispositivos (F-11) | 🔴 Alta | `app/services/alert_service.py` + cron |
| 3 | Crear `ServicioAPI.cpp/h` para comunicación HTTP | 🔴 Alta | `proyecto_iot/src/ServicioAPI.cpp` |
| 4 | Enviar eventos de falla desde ESP32 al backend | 🔴 Alta | `main.cpp` + `ServicioAPI.cpp` |
| 5 | Validar rangos físicos en `SensorDHT` | 🟡 Media | `proyecto_iot/src/SensorDHT.cpp` |
| 6 | Detectar gradientes térmicos (ΔT > 10°C/5s) | 🟡 Media | `main.cpp` o `SensorDHT.cpp` |
| 7 | Implementar descarte de picos de ruido (3σ) | 🟡 Media | `MovingAverage.h` o nuevo filtro |
| 8 | Sincronizar FSM Global con documento (variables T, R) | 🟡 Media | `main.cpp` |
| 9 | Crear carpeta `tests/` con pytest | 🟢 Baja | `avisens-backend/tests/` |
| 10 | Corregir documento: eliminar "tests/ ya creado" | 🟢 Baja | `README.md` + SSD |

---

