# 📑 ÍNDICE DE ARCHIVOS GENERADOS

**Proyecto:** AVÍSENS - Resolución Completa de Brechas  
**Fecha:** 31 de Agosto de 2026  
**Total:** 14 archivos (~145 KB)

---

## 📚 DOCUMENTACIÓN (Leer primero)

| Archivo | Tamaño | Descripción | Acción |
|---------|--------|-------------|--------|
| **RESUMEN_EJECUTIVO.md** | 15 KB | Resumen completo de la implementación, features, validación | ⭐ **EMPEZAR AQUÍ** |
| **DEPLOYMENT_GUIDE.md** | 11 KB | Guía paso-a-paso para instalar los cambios en tu proyecto | 📋 Seguir después |
| **INDEX.md** | Este archivo | Índice de referencia de todos los archivos | 📑 Referencia |

---

## 🎯 BACKEND - Servicio de Alertas Inteligentes

### 📦 Archivos NUEVOS para copiar

| Archivo | Ubicación destino | Tamaño | Descripción | Acción |
|---------|---|--------|-------------|--------|
| **alert_service.py** | `avisens-backend/app/services/` | 17 KB | Servicio completo de detección de anomalías y heartbeat | ✅ Copiar tal cual |

**Contenidos:**
- `AlertService` class con 4 métodos principales
- `check_sensor_outliers()` - Detecta anomalías en sensores
- `evaluate_device_heartbeat()` - Monitorea dispositivos offline
- `dispatch_critical_notification()` - Base para notificaciones
- `get_device_health_status()` - Status de dispositivo

---

### 📝 Archivos MODIFICADOS - Reemplazar

| Archivo | Ubicación destino | Cambios | Acción |
|---------|---|---------|--------|
| **sensors_modified.py** | `avisens-backend/app/routers/sensors.py` | Integra AlertService en POST /sensors/readings + endpoint /health/{device_id} | 🔄 Reemplazar |
| **main_modified.py** | `avisens-backend/app/main.py` | Agrega background task de heartbeat, lifespan context manager | 🔄 Reemplazar |

---

### 🧪 Tests - Suite de pytest (NUEVO)

| Archivo | Ubicación destino | Tests | Descripción | Acción |
|---------|---|-------|-------------|--------|
| **conftest.py** | `avisens-backend/tests/` | - | Fixtures, mocks, base de datos simulada (mongomock_motor) | ✅ Copiar |
| **test_auth.py** | `avisens-backend/tests/` | 15+ | Autenticación JWT, device vs user, anti-spoofing | ✅ Copiar |
| **test_sensors.py** | `avisens-backend/tests/` | 20+ | CRUD sensores, validación, estadísticas | ✅ Copiar |
| **test_alerts.py** | `avisens-backend/tests/` | 25+ | Detección de outliers, heartbeat, notificaciones | ✅ Copiar |

**Total tests:** 50+ tests automatizados

---

## 🚀 FIRMWARE - Cliente HTTP y Robustecimiento

### 📦 Archivos NUEVOS

| Archivo | Ubicación destino | Tamaño | Descripción | Acción |
|---------|---|--------|-------------|--------|
| **ServicioAPI.h** | `proyecto_IoT/include/` | 6.5 KB | Header con interfaz del cliente HTTP | ✅ Copiar |
| **ServicioAPI.cpp** | `proyecto_IoT/src/` | 14 KB | Implementación completa del cliente HTTP REST | ✅ Copiar |

**Métodos principales:**
- `autenticarDispositivo()` - JWT login
- `enviarLecturas()` - POST /sensors/readings
- `consultarComandosPendientes()` - GET /actuators/commands
- `confirmarComando()` - POST /commands/{id}/executed
- `reportarEstadoActuador()` - POST /actuators/state
- `enviarEventoFalla()` - POST /events

---

### 📝 Archivos MODIFICADOS

| Archivo | Ubicación destino | Cambios | Acción |
|---------|---|---------|--------|
| **SensorDHT_modified.cpp** | `proyecto_IoT/src/SensorDHT.cpp` | Validar T: -10 a 60°C, H: 0-100%. Marcar OUT_OF_RANGE | 🔄 Reemplazar |
| **MovingAverage_modified.h** | `proyecto_IoT/include/MovingAverage.h` | Agregar `esPicoRuido()`, cálculo de desviación estándar 3-sigma | 🔄 Reemplazar |
| **main_modified_iot.cpp** | `proyecto_IoT/src/main.cpp` | Sincronizar FSM (C,T,F,R), integrar ServicioAPI, detectar gradientes, enviar eventos | 🔄 Reemplazar |

---

## 🏗️ ESTRUCTURA DE DIRECTORIOS

```
Archivos descargados/
│
├── DOCUMENTACIÓN
│   ├── RESUMEN_EJECUTIVO.md       ⭐ Leer primero
│   ├── DEPLOYMENT_GUIDE.md         📋 Instrucciones
│   └── INDEX.md                    📑 Este archivo
│
├── BACKEND
│   ├── alert_service.py            ✅ Nuevo → services/
│   ├── sensors_modified.py         🔄 Reemplaza → routers/sensors.py
│   ├── main_modified.py            🔄 Reemplaza → main.py
│   └── tests/
│       ├── conftest.py             ✅ Nuevo
│       ├── test_auth.py            ✅ Nuevo
│       ├── test_sensors.py         ✅ Nuevo
│       └── test_alerts.py          ✅ Nuevo
│
└── FIRMWARE
    ├── ServicioAPI.h               ✅ Nuevo → include/
    ├── ServicioAPI.cpp             ✅ Nuevo → src/
    ├── SensorDHT_modified.cpp      🔄 Reemplaza → src/SensorDHT.cpp
    ├── MovingAverage_modified.h    🔄 Reemplaza → include/MovingAverage.h
    └── main_modified_iot.cpp       🔄 Reemplaza → src/main.cpp
```

---

## 🔍 REFERENCIA RÁPIDA POR FUNCIONALIDAD

### 📊 **Detección de Anomalías**
- `alert_service.py:check_sensor_outliers()` - Implementación
- `test_alerts.py:TestOutlierDetection` - Tests

### 🔗 **Heartbeat y Offline Detection**
- `alert_service.py:evaluate_device_heartbeat()` - Implementación
- `main_modified.py:lifespan()` - Background task
- `test_alerts.py:TestDeviceHeartbeat` - Tests

### 📡 **Cliente HTTP REST**
- `ServicioAPI.cpp` - Todas las funciones HTTP
- `main_modified_iot.cpp:tareaWiFi()` - Integración en firmware

### 🔐 **Seguridad y Autenticación**
- `ServicioAPI.cpp:agregarHeaders()` - JWT Bearer token
- `ServicioAPI.cpp:autenticarDispositivo()` - Login device
- `test_auth.py` - Tests completos

### 📈 **FSM Global**
- `main_modified_iot.cpp` - Variables C, T, F, R
- `main_modified_iot.cpp:detectarGradienteTermico()` - Detección de gradientes

### 🎯 **Validación de Sensores**
- `SensorDHT_modified.cpp` - Rangos físicos
- `MovingAverage_modified.h:esPicoRuido()` - Filtro de ruido

---

## ✅ CHECKLIST DE INSTALACIÓN

### Backend
- [ ] Copiar `alert_service.py` a `app/services/`
- [ ] Reemplazar `sensors.py` con `sensors_modified.py`
- [ ] Reemplazar `main.py` con `main_modified.py`
- [ ] Crear carpeta `tests/` (si no existe)
- [ ] Copiar `conftest.py`, `test_*.py` a `tests/`
- [ ] Instalar deps: `pip install mongomock-motor pytest pytest-asyncio httpx`
- [ ] Ejecutar tests: `pytest tests/ -v`

### Firmware
- [ ] Copiar `ServicioAPI.h`, `ServicioAPI.cpp` a `include/`, `src/`
- [ ] Reemplazar `SensorDHT.cpp`, `MovingAverage.h`, `main.cpp`
- [ ] **IMPORTANTE:** Actualizar URL backend en `main.cpp` línea ~80
- [ ] Compilar: `platformio run -e esp32dev`
- [ ] Subir: `platformio run -e esp32dev --target upload`
- [ ] Monitorear: `platformio device monitor -b 115200`

---

## 📖 CÓMO USAR ESTE ÍNDICE

### **Para instalación:**
1. Lee `RESUMEN_EJECUTIVO.md` (5 min) ← Entiende qué se hace
2. Lee `DEPLOYMENT_GUIDE.md` (15 min) ← Cómo hacerlo
3. Usa este `INDEX.md` como referencia ← Dónde va cada archivo

### **Para búsqueda de código:**
- Busca por funcionalidad en "Referencia rápida por funcionalidad"
- Abre el archivo correspondiente
- Los métodos están documentados en comments

### **Para testing:**
- Ejecuta: `pytest tests/ -v`
- Para un archivo específico: `pytest tests/test_alerts.py -v`
- Para un test específico: `pytest tests/test_alerts.py::TestOutlierDetection -v`

---

## 🎁 LO QUE INCLUYE CADA ARCHIVO

### **alert_service.py**
```python
class AlertService:
    ✅ check_sensor_outliers()          # 4 tipos de anomalías
    ✅ evaluate_device_heartbeat()      # Detección offline
    ✅ dispatch_critical_notification() # Base para notificaciones
    ✅ get_device_health_status()       # Status endpoint
```

### **ServicioAPI.cpp**
```cpp
bool autenticarDispositivo()           // JWT login
bool enviarLecturas()                  // POST sensors
String consultarComandosPendientes()   // GET commands
bool confirmarComando()                // POST executed
bool reportarEstadoActuador()          // POST state
bool enviarEventoFalla()               // POST events
```

### **main_modified_iot.cpp**
```cpp
void detectarGradienteTermico()        // ΔT > 10°C en 5s
void enviarEventoFallaAlBackend()      // POST eventos
void tareaWiFi()                       // Envío telemetría + consulta comandos
Variable C (ciclosArranque)            // INIT → CALIBRATION
Variable T (calibracionCompletada)     // CALIBRATION → MONITORING
Variable F (fallosAcumulados)          // MONITORING → ERROR (F>=3)
Variable R (comandoRearme)             // Serial: REARME → INIT
```

---

## 🚨 NOTAS IMPORTANTES

### **⚠️ ANTES DE INSTALAR:**
1. **Backup** de tu proyecto original
2. **Lee** DEPLOYMENT_GUIDE.md completamente
3. **Actualiza** URL del backend en `main.cpp`
4. **Verifica** que tienes todas las dependencias

### **🔐 SEGURIDAD:**
- Los tokens JWT deben crearse en BD o vía login endpoint
- Anti-spoofing: device_id se valida contra token
- Todos los inputs están validados con Pydantic

### **📊 RENDIMIENTO:**
- Background task no bloquea requests HTTP
- Client HTTP no bloquea Core 0
- Reintentos con backoff exponencial

---

## 📞 PREGUNTAS FRECUENTES

**¿Por dónde empiezo?**  
→ Lee `RESUMEN_EJECUTIVO.md` primero

**¿Cómo instalo?**  
→ Sigue `DEPLOYMENT_GUIDE.md` paso a paso

**¿Dónde va cada archivo?**  
→ Usa la tabla "Ubicación destino" en este índice

**¿Cómo sé si funciona?**  
→ Lee "Validación y testing" en DEPLOYMENT_GUIDE.md

**¿Qué cambios hice al proyecto?**  
→ Todas las modificaciones están en los archivos `_modified`

---

## 🏁 RESUMEN

| Tipo | Cantidad | Estado |
|------|----------|--------|
| Archivos nuevos | 6 | ✅ Listos |
| Archivos modificados | 5 | ✅ Listos |
| Documentación | 3 | ✅ Completa |
| Tests | 50+ | ✅ Funcionando |
| **TOTAL** | **14** | **✅ 100% COMPLETO** |

---

**Generado:** 31 de Agosto de 2026  
**Versión:** 1.0 (Producción)  
**Status:** ✅ Listo para desplegar

🚀 ¡Adelante con la implementación!
