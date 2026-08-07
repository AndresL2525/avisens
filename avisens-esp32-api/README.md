# 🔌 AVÍSENS ESP32 — Cliente API para Backend FastAPI

Código del ESP32 para reemplazar Firebase y conectarse directamente al backend FastAPI + MongoDB Atlas.

## 📦 Contenido

| Archivo | Descripción |
|---------|-------------|
| `TokenManager.h/.cpp` | Almacena el JWT en memoria flash (NVS) del ESP32 |
| `ServicioAPI.h/.cpp` | Envía lecturas de sensores al backend |
| `ServicioActuadoresAPI.h/.cpp` | Sincroniza actuadores y eventos con el backend |
| `config_api_section.h` | Variables nuevas para agregar a tu `config.h` |
| `INTEGRACION.md` | Guía paso a paso para modificar tu proyecto existente |

## 🚀 Uso rápido

1. Copia los archivos `.h` a `include/` y `.cpp` a `src/`
2. Sigue `INTEGRACION.md` para modificar `main.cpp` y `config.h`
3. Asegúrate de que tu backend FastAPI esté corriendo
4. Sube el firmware al ESP32

## ⚠️ Limitaciones del ESP32

- **RAM limitada (520KB):** HTTPS consume mucha memoria. Usa HTTP para desarrollo o un proxy local.
- **Sin RTC:** No envía timestamps. El backend usa la hora del servidor.
- **Sin buffer offline:** Si la API cae, los datos se pierden (igual que con Firebase).
- **Token de 1 año:** Si roban el firmware, obtienen el token. Mitigación: HTTPS + rotación periódica.
