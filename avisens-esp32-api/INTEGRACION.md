# 🔌 Guía de Integración — ESP32 con Backend FastAPI

Esta guía explica cómo modificar tu proyecto AVÍSENS existente para que el ESP32 hable con tu nuevo backend FastAPI en vez de Firebase.

---

## 📁 Archivos nuevos que debes copiar

Copia estos archivos a las carpetas correspondientes de tu proyecto:

```
proyecto_iot/
├── include/
│   ├── TokenManager.h              ← NUEVO
│   ├── ServicioAPI.h               ← NUEVO (reemplaza ServicioFirebase.h)
│   ├── ServicioActuadoresAPI.h     ← NUEVO (reemplaza ServicioActuadoresFirebase.h)
│   └── config.h                    ← MODIFICAR (ver abajo)
│
└── src/
    ├── TokenManager.cpp            ← NUEVO
    ├── ServicioAPI.cpp             ← NUEVO (reemplaza ServicioFirebase.cpp)
    ├── ServicioActuadoresAPI.cpp   ← NUEVO (reemplaza ServicioActuadoresFirebase.cpp)
    └── main.cpp                    ← MODIFICAR (ver abajo)
```

---

## 1️⃣ Modificar `include/config.h`

**Agrega** estas líneas (no reemplaces todo, solo añade al final):

```cpp
// === BACKEND FASTAPI ===
#define API_BASE_URL "http://192.168.1.100:8000"  // Cambia por tu IP
#define DEVICE_ID "galpon_01"
#define DEVICE_SECRET "avisens_secret_2024"
```

> ⚠️ **IMPORTANTE:** `DEVICE_ID` debe estar en la lista `AUTHORIZED_DEVICE_IDS` de tu `.env` del backend.

---

## 2️⃣ Modificar `src/main.cpp`

### Cambios en los `#include`:

**REEMPLAZA:**
```cpp
#include "ServicioFirebase.h"
#include "ServicioActuadoresFirebase.h"
```

**POR:**
```cpp
#include "TokenManager.h"
#include "ServicioAPI.h"
#include "ServicioActuadoresAPI.h"
```

### Cambios en las instancias globales:

**AGREGA** después de las instancias existentes:
```cpp
TokenManager tokenManager;
```

### Cambios en `setup()`:

**REEMPLAZA** la sección de inicialización (donde estaba WiFi y Firebase):
```cpp
// ANTES (Firebase):
// ConexionWiFi::conectar();

// DESPUÉS (FastAPI):
ConexionWiFi::conectar();
tokenManager.iniciar();
ServicioAPI::iniciar(&tokenManager);
ServicioActuadoresAPI::iniciar(&tokenManager);
```

### Cambios en TAREA 3 (enviar a Firebase):

**REEMPLAZA:**
```cpp
// ANTES:
ServicioFirebase::enviarLecturaCompleta(
    ultimaTemperatura, ultimaHumedad, ultimoPeso,
    obstaculoDetectado, calidadAireValor);
ServicioActuadoresFirebase::reportarEstadoActuadores(actuadores);
```

**POR:**
```cpp
// DESPUÉS:
ServicioAPI::enviarLecturaCompleta(
    ultimaTemperatura, ultimaHumedad, ultimoPeso,
    obstaculoDetectado, calidadAireValor, calidadAireVoltaje);
ServicioActuadoresAPI::reportarEstadoActuadores(actuadores);
```

### Cambios en TAREA 4 (sincronizar órdenes remotas):

**REEMPLAZA** TODO el bloque de sincronización:
```cpp
// ANTES (Firebase):
// EstadoRemotoActuador remCalefactor = ServicioActuadoresFirebase::leerEstadoRemoto("calefactor");
// ... (todo ese bloque largo)

// DESPUÉS (FastAPI):
ComandoRemoto comandos[4];
int cantidad = 0;
if (ServicioActuadoresAPI::leerComandosPendientes(comandos, 4, cantidad)) {
    for (int i = 0; i < cantidad; i++) {
        if (!comandos[i].valido) continue;

        // Aplicar comando según el nombre
        if (comandos[i].nombre == "calefactor") {
            actuadores.establecerModoCalefactor(
                comandos[i].modo == "MANUAL" ? ModoActuador::MANUAL : ModoActuador::AUTO
            );
            if (comandos[i].modo == "MANUAL") {
                actuadores.ordenManualCalefactor(comandos[i].ordenManual);
            }
        }
        else if (comandos[i].nombre == "extractor") {
            actuadores.establecerModoExtractor(
                comandos[i].modo == "MANUAL" ? ModoActuador::MANUAL : ModoActuador::AUTO
            );
            if (comandos[i].modo == "MANUAL") {
                actuadores.ordenManualExtractor(comandos[i].ordenManual);
            }
        }
        else if (comandos[i].nombre == "humidificador") {
            actuadores.establecerModoHumidificador(
                comandos[i].modo == "MANUAL" ? ModoActuador::MANUAL : ModoActuador::AUTO
            );
            if (comandos[i].modo == "MANUAL") {
                actuadores.ordenManualHumidificador(comandos[i].ordenManual);
            }
        }
        else if (comandos[i].nombre == "alimentador") {
            if (comandos[i].ordenManual) {
                actuadores.solicitarDosisManualAlimentador();
            }
        }

        // Confirmar que ejecutamos el comando
        ServicioActuadoresAPI::confirmarComandoEjecutado(comandos[i].id);
    }
}
```

### Cambios en `registrarSiCambio()`:

**REEMPLAZA** la llamada interna:
```cpp
// ANTES:
// ServicioActuadoresFirebase::registrarEvento("ACTUADOR", nombreActuador, mensaje, "info");

// DESPUÉS:
ServicioActuadoresAPI::registrarEvento("ACTUADOR", nombreActuador, mensaje, "info");
```

### Cambios en falla de sensor:

**REEMPLAZA:**
```cpp
// ANTES:
// ServicioActuadoresFirebase::registrarEvento("FALLA_SENSOR", "DHT11", "...", "alerta");

// DESPUÉS:
ServicioActuadoresAPI::registrarEvento("FALLA_SENSOR", "DHT11", "Lectura fallida del sensor de temperatura/humedad", "alerta");
```

---

## 3️⃣ Modificar `platformio.ini`

**No necesitas cambiar librerías.** Seguimos usando:
- `ArduinoJson` (ya la tienes)
- `HTTPClient` (incluido en el core del ESP32)
- `Preferences` (incluido en el core del ESP32)

**PERO** si quieres usar HTTPS en producción, necesitas asegurar que el ESP32 tenga suficiente heap. Agrega esto a `build_flags`:

```ini
build_flags = 
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    ; Descomenta la siguiente línea solo si usas HTTPS:
    ; -DUSE_HTTPS
```

---

## 4️⃣ Probar la conexión

### Paso 1: Asegúrate de que tu backend corra
```bash
cd avisens-backend
uvicorn app.main:app --host 0.0.0.0 --port 8000
```

### Paso 2: Obtén la IP de tu PC
```bash
# Linux/Mac:
ipconfig getifaddr en0

# Windows:
ipconfig
```

### Paso 3: Actualiza `config.h`
```cpp
#define API_BASE_URL "http://192.168.1.100:8000"  // Tu IP aquí
```

### Paso 4: Sube el firmware al ESP32
```bash
pio run -t upload
```

### Paso 5: Abre el Monitor Serie
```bash
pio device monitor --baud 115200
```

Deberías ver:
```
[ServicioAPI] Inicializado
[ServicioAPI] No hay token. Intentando autenticación...
[ServicioAPI] ✅ Autenticación exitosa. Token guardado.
[ServicioAPI] ✅ Lectura enviada correctamente.
```

---

## 5️⃣ Verificar en el backend

```bash
# Consultar lecturas recientes
curl "http://localhost:8000/sensors/readings?device_id=galpon_01&limit=5" \
  -H "Authorization: Bearer <TOKEN_USUARIO>"
```

Deberías ver los datos que acaba de enviar el ESP32.

---

## 🔧 Troubleshooting

### "Sin conexión WiFi"
- Verifica que el ESP32 y tu PC estén en la **misma red WiFi**.
- Si usas un firewall, permite el puerto 8000.

### "Error de conexión" (código -1)
- Tu PC puede estar bloqueando conexiones entrantes.
- Prueba desde tu celular: `http://192.168.1.100:8000/health` en el navegador.
- Si no responde, es problema de red, no del ESP32.

### "401 Unauthorized" en loop infinito
- Verifica que `DEVICE_ID` esté en `AUTHORIZED_DEVICE_IDS` del `.env` del backend.
- Verifica que `DEVICE_SECRET` no esté vacío.

### "422 Validation Error"
- Revisa el Monitor Serie: el JSON que envía el ESP32 puede tener un campo fuera de rango.
- Ejemplo: temperatura > 60°C o calidad_aire > 4095.

### Out of Memory (reinicios del ESP32)
- El ESP32 tiene solo 520KB de RAM. Si usas HTTPS, consume mucha memoria.
- **Solución:** Usa HTTP para desarrollo, o un proxy local.
- Reduce `StaticJsonDocument` si es posible.

---

## 🚀 Migración completa a HTTPS (producción)

Cuando tu backend esté en la nube con HTTPS:

1. Cambia `API_BASE_URL` a `https://tu-dominio.com`
2. En `ServicioAPI.cpp`, reemplaza `http.begin(url)` por:
   ```cpp
   // Para certificados autofirmados (NO recomendado en producción)
   // client.setInsecure();  // ⚠️ Solo para pruebas

   // Para certificados reales (Let's Encrypt, etc.)
   // El ESP32 tiene certificados raíz embebidos que confían en Let's Encrypt
   ```
3. Considera usar un **proxy local** (Raspberry Pi) que hable HTTP con el ESP32 y HTTPS con la nube. Esto es la opción más robusta para IoT.

---

## 📊 Diferencias clave vs Firebase

| Aspecto | Firebase (antes) | FastAPI (ahora) |
|---------|-----------------|-----------------|
| Auth | Secret legacy en URL | JWT Bearer header |
| Envío sensores | `PUT /sensores.json` | `POST /sensors/readings` |
| Leer comandos | `GET /actuadores/xxx.json` | `GET /actuators/commands` (array) |
| Confirmar comando | No existía | `POST /actuators/commands/{id}/executed` |
| Eventos | `POST /eventos.json` | `POST /events` |
| Timestamp | `millis()` del ESP32 | Server time (más confiable) |
| Offline | Datos perdidos | Datos perdidos (igual que antes) |
