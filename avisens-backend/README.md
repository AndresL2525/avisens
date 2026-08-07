# 🐔 AVÍSENS Backend — API IoT para Granjas Avícolas

Backend profesional en **FastAPI + MongoDB Atlas** para recibir, almacenar y servir datos de sensores IoT. Diseñado como microservicio independiente del backend de autenticación de usuarios.

---

## 📋 Tabla de Contenidos

- [Arquitectura](#-arquitectura)
- [Stack Tecnológico](#-stack-tecnológico)
- [Instalación](#-instalación)
- [Configuración](#-configuración)
- [Uso](#-uso)
- [Endpoints](#-endpoints)
- [Seguridad](#-seguridad)
- [Docker](#-docker)
- [Riesgos y Limitaciones](#-riesgos-y-limitaciones-honestos)
- [Próximos Pasos](#-próximos-pasos)

---

## 🏗️ Arquitectura

```
┌─────────────┐      POST /sensors/readings      ┌─────────────────┐
│   ESP32     │ ─────────────────────────────────→ │                 │
│  (Galpón)   │      GET /actuators/commands       │  FastAPI        │
│             │ ←───────────────────────────────── │  + MongoDB      │
└─────────────┘                                    │  + JWT          │
                                                   │                 │
┌─────────────┐      GET /sensors/readings         └────────┬────────┘
│  App Móvil  │ ─────────────────────────────────→            │
│  (Kotlin)   │      POST /actuators/commands               │
│             │ ←─────────────────────────────────            │
└─────────────┘                                             │
                                                            ▼
                                                    ┌───────────────┐
                                                    │  MongoDB      │
                                                    │  Atlas        │
                                                    └───────────────┘
```

---

## 🛠️ Stack Tecnológico

| Capa | Tecnología | Por qué |
|------|-----------|---------|
| **Framework** | FastAPI | Async nativo, validación Pydantic, docs automáticas |
| **Base de datos** | MongoDB Atlas (Motor) | Async, schema flexible, gratis para prototipos |
| **Auth** | PyJWT | Tokens firmados con HS256, sin estado en servidor |
| **Rate Limit** | SlowAPI | Protección contra abuso por IP |
| **Logging** | structlog | JSON estructurado para análisis en producción |
| **Container** | Docker + multi-stage | Imagen final ~150MB, usuario no-root |
| **Validación** | Pydantic v2 | Primera línea de defensa contra datos maliciosos |

---

## 🚀 Instalación

### Opción A: Sin Docker (desarrollo)

```bash
# 1. Clonar
git clone <tu-repo>
cd avisens-backend

# 2. Crear entorno virtual
python -m venv venv
source venv/bin/activate  # Windows: venv\Scripts\activate

# 3. Instalar dependencias
pip install -r requirements.txt

# 4. Configurar variables de entorno
cp .env.example .env
# Edita .env con tus credenciales de MongoDB Atlas y JWT

# 5. Ejecutar
uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

### Opción B: Con Docker (producción)

```bash
# 1. Configurar .env
cp .env.example .env
# Edita .env

# 2. Construir y ejecutar
docker-compose up --build -d

# 3. Ver logs
docker-compose logs -f backend

# 4. Detener
docker-compose down
```

---

## ⚙️ Configuración

Edita `.env`:

```env
# MongoDB Atlas (obligatorio)
MONGODB_URI=mongodb+srv://usuario:password@cluster.mongodb.net/avisens?retryWrites=true&w=majority

# JWT (genera una clave fuerte)
JWT_SECRET_KEY=$(python -c "import secrets; print(secrets.token_hex(32))")

# Dispositivos autorizados (IDs de tus ESP32)
AUTHORIZED_DEVICE_IDS=galpon_01,galpon_02

# Entorno
ENVIRONMENT=development
```

---

## 📡 Uso

### 1. Autenticar dispositivo ESP32

```bash
curl -X POST http://localhost:8000/auth/device/login \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "galpon_01",
    "device_secret": "cualquier_secreto_por_ahora"
  }'
```

**Respuesta:**
```json
{
  "access_token": "eyJhbGciOiJIUzI1NiIs...",
  "token_type": "bearer",
  "token_kind": "device"
}
```

### 2. Enviar lectura de sensores (ESP32)

```bash
curl -X POST http://localhost:8000/sensors/readings \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <TOKEN_DEL_PASO_1>" \
  -d '{
    "device_id": "galpon_01",
    "temperatura": 24.5,
    "humedad": 65.0,
    "peso": 125.3,
    "obstaculo": false,
    "calidad_aire": 1200,
    "voltaje_aire": 0.96
  }'
```

### 3. Autenticar usuario de app

```bash
curl -X POST http://localhost:8000/auth/user/login \
  -H "Content-Type: application/json" \
  -d '{
    "username": "admin",
    "password": "avisens2024"
  }'
```

### 4. Consultar lecturas (App)

```bash
curl "http://localhost:8000/sensors/readings?device_id=galpon_01&limit=10" \
  -H "Authorization: Bearer <TOKEN_USUARIO>"
```

### 5. Enviar comando a actuador (App)

```bash
curl -X POST http://localhost:8000/actuators/commands \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <TOKEN_USUARIO>" \
  -d '{
    "device_id": "galpon_01",
    "nombre": "calefactor",
    "modo": "MANUAL",
    "orden_manual": true
  }'
```

### 6. Consultar comandos pendientes (ESP32)

```bash
curl "http://localhost:8000/actuators/commands" \
  -H "Authorization: Bearer <TOKEN_DISPOSITIVO>"
```

---

## 🔌 Endpoints

| Método | Ruta | Auth | Descripción |
|--------|------|------|-------------|
| `POST` | `/auth/device/login` | Público | ESP32 obtiene token |
| `POST` | `/auth/user/login` | Público | App obtiene token |
| `POST` | `/sensors/readings` | Device JWT | ESP32 envía datos |
| `GET` | `/sensors/readings` | User JWT | App consulta datos |
| `GET` | `/sensors/readings/history` | User JWT | Historial por tiempo |
| `GET` | `/sensors/stats` | User JWT | Estadísticas agregadas |
| `POST` | `/actuators/commands` | User JWT | App envía comando |
| `GET` | `/actuators/commands` | Device JWT | ESP32 lee pendientes |
| `POST` | `/actuators/commands/{id}/executed` | Device JWT | Confirmar ejecución |
| `POST` | `/actuators/state` | Device JWT | ESP32 reporta estado |
| `GET` | `/actuators/state` | User JWT | App consulta estado |
| `POST` | `/events` | Device JWT | Registrar evento |
| `GET` | `/events` | User JWT | Consultar eventos |
| `GET` | `/health` | Público | Health check |

Documentación interactiva: `http://localhost:8000/docs`

---

## 🔒 Seguridad Implementada

| Medida | Implementación | Nivel |
|--------|---------------|-------|
| **Validación de inputs** | Pydantic con rangos físicos | ✅ Crítico |
| **Sanitización device_id** | Solo alfanumérico + `_` `-` | ✅ Crítico |
| **JWT firmado** | HS256 con clave de 256 bits | ✅ Crítico |
| **Rate limiting** | 60 req/min por IP | ✅ Importante |
| **Headers de seguridad** | HSTS, CSP, X-Frame-Options | ✅ Importante |
| **Usuario no-root en Docker** | `USER avisens` | ✅ Buena práctica |
| **Sin secrets en código** | Todo vía `.env` | ✅ Crítico |
| **CORS restringido** | Orígenes configurables | ✅ Importante |
| **Spoofing detection** | Token device_id vs body device_id | ✅ Importante |
| **NaN/Inf rejection** | Previene corrupción de DB | ✅ Buena práctica |
| **Limites de query** | Max 1000-5000 registros | ✅ Importante |
| **Multi-stage Docker** | Imagen mínima, sin build tools | ✅ Buena práctica |

---

## 🐳 Docker

```bash
# Construir imagen
docker build -t avisens-backend .

# Ejecutar con variables de entorno
docker run -d \
  --name avisens \
  -p 8000:8000 \
  --env-file .env \
  avisens-backend

# Ver estado
docker ps
docker logs avisens
```

---

## ⚠️ Riesgos y Limitaciones (Honestos)

> Este es un proyecto académico. Para producción real, considera estos puntos:

### 🔴 Riesgos de Seguridad

1. **Token de dispositivo de larga duración (1 año)**
   - **Problema:** Si alguien roba el firmware del ESP32 (lectura de flash), obtiene el token.
   - **Mitigación actual:** HTTPS obligatorio. En producción real, usa **mTLS** (certificados mutuos) o servicios como **AWS IoT Core** / **Azure IoT Hub**.
   - **Alternativa:** Tokens cortos + mecanismo de refresh automático en el ESP32 (complejo por RAM limitada).

2. **Device secret en texto plano (stub)**
   - **Problema:** `authenticate_device()` compara en texto plano. No usa bcrypt.
   - **Mitigación:** En producción, almacena hashes bcrypt en MongoDB y verifica con `passlib`.

3. **Sin lista de revocación de tokens (JWT)**
   - **Problema:** No puedes invalidar un token robado hasta que expire.
   - **Mitigación:** Implementa una blacklist en Redis/MongoDB, o usa tokens cortos con refresh.

4. **Rate limiting en memoria (SlowAPI)**
   - **Problema:** Si ejecutas múltiples workers/contenedores, cada uno tiene su propio contador. Un atacante puede distribuir ataques.
   - **Mitigación:** Usa **RedisBackend** de SlowAPI para estado compartido.

5. **Sin HTTPS en desarrollo**
   - **Problema:** Los tokens viajan en texto plano si usas HTTP.
   - **Mitigación:** En producción, siempre HTTPS. Usa **Cloudflare**, **Nginx + Let's Encrypt**, o el TLS del proveedor cloud.

### 🟡 Limitaciones Técnicas

6. **MongoDB Atlas gratuito (M0)**
   - Límite de 512MB de almacenamiento. Con datos cada 5 segundos, llenas ~1.5 meses.
   - **Solución:** Implementar TTL (auto-eliminación) o agregar compresión/agregación.

7. **ESP32 y TLS/HTTPS**
   - El ESP32 tiene poca RAM (520KB). Las peticiones HTTPS con certificados grandes pueden fallar o ser lentas.
   - **Solución:** Usa certificados pequeños (ECDSA en vez de RSA), o un proxy local que hable HTTP con el ESP32 y HTTPS con la nube.

8. **Sin reintentos con backoff en el ESP32**
   - Si tu API cae, el ESP32 pierde datos. No hay buffer circular ni reintentos exponenciales.
   - **Solución:** Agregar cola en SPIFFS del ESP32 (complejo pero factible).

9. **Sin particionamiento de datos**
   - Todas las lecturas van a una sola colección. A escala, las queries se vuelven lentas.
   - **Solución:** Time-series collection de MongoDB 5.0+, o particionar por mes.

10. **Auth de usuarios es un stub**
    - No hay tabla real de usuarios. Solo hay un usuario hardcodeado (`admin`/`avisens2024`).
    - **Solución:** Integrar con el backend de autenticación de tus compañeros, o implementar registro/login real con bcrypt.

### 🟢 Decisiones conscientes (aceptables para el alcance)

- **No usamos Redis:** Para 1-2 dispositivos, el rate limiting en memoria es suficiente.
- **No usamos Celery/Colas:** El ESP32 consulta directamente, no necesitamos workers asíncronos.
- **No usamos WebSockets:** El ESP32 hace polling cada 10 segundos. A esta escala, HTTP REST es suficiente y más simple.
- **No usamos GraphQL:** REST es más fácil de implementar en el ESP32 con ArduinoJson.

---

## 📈 Próximos Pasos

1. [ ] Implementar `ServicioAPI.cpp` en el ESP32 (reemplazar Firebase)
2. [ ] Agregar TTL en MongoDB para auto-borrar datos antiguos
3. [ ] Integrar autenticación real con el backend de compañeros
4. [ ] Agregar tests con pytest (`tests/` ya creado)
5. [ ] Configurar GitHub Actions para CI/CD
6. [ ] Implementar notificaciones push (Firebase Cloud Messaging o similar)
7. [ ] Agregar dashboard web con gráficas de tendencia

---

## 👤 Autor

**AVÍSENS Team** — SENA ADSO, Ficha 3229446  
Centro de Teleinformática y Producción Industrial, Regional Cauca

---

## 📜 Licencia

MIT — Proyecto académico.
