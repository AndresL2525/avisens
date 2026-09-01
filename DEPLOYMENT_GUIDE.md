# 📋 GUÍA DE DEPLOYMENT - RESOLUCIÓN DE BRECHAS AVÍSENS

**Fecha:** 31 de Agosto de 2026  
**Versión:** 1.0 (Implementación Completa - Sin Placeholders)  
**Estado:** 🔴 **CRÍTICO** - Cambios de arquitectura requieren testing completo antes de producción

---

## 📌 RESUMEN DE CAMBIOS

### **ETAPA 1: BACKEND (FastAPI + MongoDB Motor)**

#### **Archivos NUEVOS:**

| Archivo | Descripción | Ubicación |
|---------|-------------|-----------|
| `alert_service.py` | Servicio de detección de anomalías y heartbeat | `avisens-backend/app/services/` |
| `conftest.py` | Fixtures y configuración para pytest | `avisens-backend/tests/` |
| `test_auth.py` | Tests de autenticación | `avisens-backend/tests/` |
| `test_sensors.py` | Tests de lecturas de sensores | `avisens-backend/tests/` |
| `test_alerts.py` | Tests de AlertService | `avisens-backend/tests/` |

#### **Archivos MODIFICADOS:**

| Archivo | Cambios |
|---------|---------|
| `app/routers/sensors.py` | Agregar AlertService; endpoint `/sensors/health/{device_id}` |
| `app/main.py` | Background task para heartbeat cada 30s |

---

### **ETAPA 2: FIRMWARE IoT (ESP32 + FreeRTOS + C++)**

#### **Archivos NUEVOS:**

| Archivo | Descripción | Ubicación |
|---------|-------------|-----------|
| `ServicioAPI.h` | Header del cliente HTTP REST | `proyecto_IoT/include/` |
| `ServicioAPI.cpp` | Implementación del cliente HTTP | `proyecto_IoT/src/` |

#### **Archivos MODIFICADOS:**

| Archivo | Cambios |
|---------|---------|
| `SensorDHT.cpp` | Validar rangos físicos (-10 a 60°C, 0-100% humedad) |
| `MovingAverage.h` | Agregar método `esPicoRuido()` con 3-sigma rule |
| `main.cpp` | Sincronizar FSM, integrar ServicioAPI, enviar eventos |

---

## 🚀 INSTRUCCIONES DE INSTALACIÓN

### **PASO 1: BACKEND - Instalar AlertService**

```bash
# Navegar al directorio del backend
cd avisens-backend

# 1. Copiar alert_service.py
cp /path/to/alert_service.py app/services/

# 2. Reemplazar sensors.py
cp /path/to/sensors_modified.py app/routers/sensors.py

# 3. Reemplazar main.py
cp /path/to/main_modified.py app/main.py

# 4. Crear carpeta de tests si no existe
mkdir -p tests

# 5. Copiar fixtures y tests
cp /path/to/conftest.py tests/
cp /path/to/test_auth.py tests/
cp /path/to/test_sensors.py tests/
cp /path/to/test_alerts.py tests/

# 6. Instalar dependencias adicionales (si es necesario)
pip install mongomock-motor pytest pytest-asyncio httpx
```

### **PASO 2: BACKEND - Validar instalación**

```bash
# Ejecutar tests
pytest tests/ -v

# Resultado esperado: ✅ Todos los tests pasan (>50 tests)

# Iniciar servidor
python -m uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
```

**Verificación en navegador/Postman:**
- GET `http://localhost:8000/health` → debe devolver estado con background tasks
- GET `http://localhost:8000/docs` → documentación OpenAPI

---

### **PASO 3: FIRMWARE - Instalar cliente HTTP**

```bash
# Navegar al proyecto IoT
cd proyecto_IoT

# 1. Copiar archivos en include/
cp /path/to/ServicioAPI.h include/

# 2. Copiar archivos en src/
cp /path/to/ServicioAPI.cpp src/

# 3. Reemplazar archivos modificados
cp /path/to/SensorDHT_modified.cpp src/SensorDHT.cpp
cp /path/to/MovingAverage_modified.h include/MovingAverage.h
cp /path/to/main_modified_iot.cpp src/main.cpp
```

### **PASO 4: FIRMWARE - Ajustar configuración**

Editar `proyecto_IoT/src/main.cpp` y ajustar:

```cpp
// Línea ~80: URL del backend (reemplazar IP/puerto según tu ambiente)
ServicioAPI servicioAPI(
    "http://192.168.1.100:8000",  // ← CAMBIAR A TU BACKEND
    "galpon_01",                   // ← ID del dispositivo
    "device_secret_key_123"        // ← Secret (debe existir en BD)
);
```

También en `platformio.ini`, asegurar que HTTPClient esté disponible:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    https://github.com/bblanchon/ArduinoJson.git
    adafruit/DHT sensor library
    # ... otras librerías
```

### **PASO 5: FIRMWARE - Compilar y subir**

```bash
# Compilar
platformio run -e esp32dev

# Subir al dispositivo
platformio run -e esp32dev --target upload

# Monitorear puerto serial
platformio device monitor -b 115200
```

---

## ✅ VALIDACIÓN POST-DEPLOYMENT

### **Backend:**

1. **Health Check:**
   ```bash
   curl http://localhost:8000/health
   # Respuesta esperada:
   # {
   #   "status": "healthy",
   #   "background_tasks": {"heartbeat": "running"}
   # }
   ```

2. **Crear lectura de sensor:**
   ```bash
   curl -X POST http://localhost:8000/sensors/readings \
     -H "Authorization: Bearer {DEVICE_TOKEN}" \
     -H "Content-Type: application/json" \
     -d '{
       "device_id": "galpon_01",
       "temperatura": 28.5,
       "humedad": 65.0,
       "calidad_aire": 450
     }'
   ```

3. **Verificar estado de dispositivo:**
   ```bash
   curl http://localhost:8000/sensors/health/galpon_01 \
     -H "Authorization: Bearer {USER_TOKEN}"
   ```

4. **Ejecutar tests:**
   ```bash
   pytest tests/ -v --tb=short
   ```

### **Firmware:**

1. **Serial Output esperado (Monitor):**
   ```
   [WiFi Task] Iniciada en Core 1
   Intentando autenticar dispositivo...
   ✓ Dispositivo autenticado
   ✓ Token JWT: eyJ0eXAiOiJKV1QiLCJhbGc...
   ✓ Telemetría enviada
   ✓ Comandos pendientes consultados
   ```

2. **Verificar eventos en MongoDB:**
   ```bash
   # En mongosh o MongoDB Compass
   db.events.find({"device_id": "galpon_01", "tipo": "ALERTA"}).pretty()
   ```

3. **Verificar heartbeat (offline detection):**
   - Desconectar dispositivo de WiFi
   - Esperar 35 segundos
   - Verificar evento en `events` collection con `tipo: "SISTEMA"`

---

## 🔧 CONFIGURACIÓN DEL BACKEND EN PRODUCCIÓN

### **1. Base de datos MongoDB:**

Crear índices para optimizar queries:

```javascript
// En MongoDB Atlas o local mongosh
use avisens
db.sensor_readings.createIndex({ "device_id": 1, "timestamp": -1 })
db.events.createIndex({ "device_id": 1, "timestamp": -1 })
db.events.createIndex({ "tipo": 1 })
db.devices.createIndex({ "device_id": 1 }, { unique: true })
db.users.createIndex({ "email": 1 }, { unique: true })
```

### **2. Variables de entorno (.env):**

```bash
# MongoDB
MONGODB_URL=mongodb+srv://user:pass@cluster.mongodb.net/?retryWrites=true&w=majority
MONGODB_NAME=avisens

# JWT
SECRET_KEY=tu-secret-key-super-seguro-min-32-chars

# API
API_HOST=0.0.0.0
API_PORT=8000
API_WORKERS=4

# CORS
CORS_ORIGINS=["http://localhost:3000", "https://tuapp.com"]

# Environment
ENVIRONMENT=production
```

### **3. Docker Compose para producción:**

```yaml
version: '3.8'

services:
  backend:
    image: avisens-backend:latest
    ports:
      - "8000:8000"
    environment:
      - MONGODB_URL=${MONGODB_URL}
      - SECRET_KEY=${SECRET_KEY}
      - ENVIRONMENT=production
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8000/health"]
      interval: 30s
      timeout: 10s
      retries: 3
    restart: unless-stopped

  mongodb:
    image: mongo:5.0
    volumes:
      - mongodb_data:/data/db
    environment:
      - MONGO_INITDB_DATABASE=avisens
    restart: unless-stopped

volumes:
  mongodb_data:
```

---

## 📊 MATRIZ DE COBERTURA

| Requisito (SSD) | Estado | Implementación |
|---|---|---|
| **F-09: Alertas de Fallo** | ✅ Completo | `AlertService.check_sensor_outliers()` |
| **F-11: Detección Offline** | ✅ Completo | `AlertService.evaluate_device_heartbeat()` |
| Gradiente térmico (ΔT>10°C/5s) | ✅ Completo | `main.cpp:detectarGradienteTermico()` |
| Humedad estancada (0%/100%) | ✅ Completo | `alert_service.py:check_sensor_outliers()` |
| Valores fuera de rango | ✅ Completo | `SensorDHT.cpp` + `alert_service.py` |
| Picos de ruido (3-sigma) | ✅ Completo | `MovingAverage.esPicoRuido()` |
| Cliente HTTP ESP32 | ✅ Completo | `ServicioAPI.cpp` |
| Envío de eventos de falla | ✅ Completo | `main.cpp:enviarEventoFallaAlBackend()` |
| FSM Global sincronizada | ✅ Completo | `main.cpp` (Variables C, T, F, R) |
| Background task heartbeat | ✅ Completo | `main.py:lifespan()` |
| Tests (pytest) | ✅ Completo | 50+ tests en `tests/` |

---

## ⚠️ NOTAS CRÍTICAS

### **1. Dependencias adicionales:**

```bash
# Backend
pip install mongomock-motor pytest pytest-asyncio httpx

# Firmware
# En platformio.ini: bblanchon/ArduinoJson v6.18.0+
```

### **2. Tokens JWT:**

Los tokens deben crearse en la base de datos o a través del endpoint `/auth/device/login`:

```python
# Para crear un dispositivo en MongoDB:
db.devices.insert_one({
    "device_id": "galpon_01",
    "secret": "device_secret_key_123",
    "active": True,
    "created_at": datetime.now(timezone.utc)
})
```

### **3. Timeout de heartbeat:**

Por defecto, un dispositivo se marca como offline después de **30 segundos** sin enviar datos.
Ajustable en `alert_service.py:evaluate_device_heartbeat(timeout_seconds=30)`

### **4. Backoff de reintentos (HTTP):**

El cliente HTTP intenta 3 veces con backoff exponencial: 500ms → 1000ms → 2000ms.
Ajustable en `ServicioAPI.h:MAX_REINTENTOS` y `BACKOFF_INICIAL_MS`.

### **5. Seguridad:**

- Los tokens JWT expiran en 1 hora (configurable)
- Se valida que el `device_id` del token coincida con el del body (anti-spoofing)
- Todos los headers de seguridad se incluyen automáticamente

---

## 🧪 TROUBLESHOOTING

### **Backend no inicia:**
```
Error: "Address already in use"
→ Cambiar puerto en .env: API_PORT=8001
```

### **Dispositivo no se autentica:**
```
Error: "credentials not found"
→ Verificar que el device_id y secret existan en MongoDB
→ Verificar que la URL del backend sea correcta (ping al servidor)
```

### **Alertas no se crean:**
```
Error: "No se puede guardar evento"
→ Verificar índices en MongoDB (ver sección "Base de datos")
→ Verificar permisos de escritura en events collection
```

### **Heartbeat no detecta offline:**
```
→ Verificar que el background task esté corriendo (GET /health)
→ Revisar logs: debe haber entrada cada 30 segundos
→ Verificar que sensor_readings collection tenga documentos recientes
```

---

## 📞 SOPORTE

Para preguntas o errores durante la implementación, revisar:

1. Logs del servidor: `tail -f uvicorn.log`
2. Logs del ESP32: `platformio device monitor -b 115200`
3. MongoDB Compass: Ver estructura de colecciones y documentos
4. Tests: `pytest tests/test_alerts.py -v -s` (con output detallado)

---

**Versión final:** v1.0 - Implementación completa, lista para producción con testing.  
**Próximos pasos:** FCM (notificaciones push v1.5), WebSockets (v2.0), Dashboard web (v1.6)
