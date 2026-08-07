/*
 * ============================================================================
 * SECCIÓN NUEVA para config.h — Variables del Backend FastAPI
 * ============================================================================
 * 
 * AGREGA estas líneas a tu config.h existente (después de las de Firebase
 * o reemplazándolas). No borres las credenciales de WiFi ni los pines.
 * 
 * Si quieres mantener Firebase como fallback temporal, deja ambas secciones
 * y usa un #define para elegir cuál usar.
 * ============================================================================
 */

// -----------------------------------------------------------------------------
// NUEVO: Backend FastAPI (reemplaza Firebase)
// -----------------------------------------------------------------------------
// URL base de tu API. 
// En desarrollo local: "http://192.168.1.100:8000" (IP de tu PC)
// En producción: "https://tu-api.onrender.com" o tu dominio
#define API_BASE_URL "http://192.168.1.100:8000"

// ID único de este dispositivo. Debe estar en AUTHORIZED_DEVICE_IDS del .env
#define DEVICE_ID "galpon_01"

// Secreto compartido del dispositivo. En producción real, esto debería
// ser un hash, no texto plano. Por ahora, cualquier string funciona.
#define DEVICE_SECRET "avisens_secret_2024"

// -----------------------------------------------------------------------------
// LEGACY: Firebase (puedes borrar esto cuando migres completamente)
// -----------------------------------------------------------------------------
// #define FIREBASE_HOST "https://..."
// #define FIREBASE_AUTH "..."
// #define FIREBASE_PATH "/sensores"
