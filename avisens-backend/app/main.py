"""
=============================================================================
AVÍSENS Backend - Punto de entrada principal (MODIFICADO con AlertService).

FastAPI application con:
- Conexión async a MongoDB (Motor)
- Autenticación JWT (usuarios + dispositivos)
- Rate limiting (SlowAPI)
- Headers de seguridad
- Logging estructurado
- Manejo global de excepciones
- Documentación OpenAPI automática
- Background task para heartbeat de dispositivos (AlertService)

Flujo de datos:
  ESP32 ──POST /sensors/readings──→ FastAPI ──→ MongoDB Atlas
  ESP32 ──GET  /actuators/commands──→ FastAPI ←── MongoDB
  App   ──GET  /sensors/readings──→ FastAPI ←── MongoDB
  [Background] Heartbeat check cada 30s ──→ Genera alertas de offline
=============================================================================
"""

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager
import asyncio

from app.config import settings
from app.database import connect_db, close_db, get_database
from app.utils.logger import configure_logging, get_logger
from app.utils.exceptions import (
    AVISENSException,
    avisens_exception_handler,
    validation_exception_handler,
    generic_exception_handler,
)
from app.middleware.rate_limit import setup_rate_limiting, limiter
from app.middleware.security import SecurityHeadersMiddleware
from app.services.alert_service import AlertService

from app.routers import auth, sensors, actuators, events

from fastapi.exceptions import RequestValidationError

logger = get_logger(__name__)

# Variables globales para control de background tasks
_heartbeat_task = None
_heartbeat_running = False


async def heartbeat_background_task():
    """
    Tarea en background que evalúa el heartbeat de todos los dispositivos
    cada 30 segundos. Detecta dispositivos inactivos y genera alertas.

    Se ejecuta de forma independiente en paralelo a las requests HTTP.
    """
    global _heartbeat_running
    _heartbeat_running = True

    try:
        await asyncio.sleep(10)  # Esperar 10s antes de empezar (estabilización)
        logger.info("✅ Background task de heartbeat iniciada")

        while _heartbeat_running:
            try:
                # Obtener instancia de la base de datos
                db = None
                try:
                    # Acceder a la DB a través del cliente global
                    from app.database import client
                    if client:
                        db = client[settings.mongodb_name]
                        alert_service = AlertService(db)

                        result = await alert_service.evaluate_device_heartbeat(
                            timeout_seconds=30
                        )

                        if result.get("offline_devices"):
                            logger.warning(
                                f"Heartbeat: Detectados {len(result['offline_devices'])} "
                                f"dispositivos offline",
                                offline_count=len(result["offline_devices"]),
                                alerts_created=result.get("alerts_created", 0),
                                skipped_alerts=result.get("existing_alerts_skipped", 0)
                            )
                        else:
                            logger.debug(
                                "Heartbeat: Todos los dispositivos online",
                                timestamp=result.get("evaluation_time")
                            )
                except Exception as inner_e:
                    logger.error(
                        "Error en heartbeat task",
                        error=str(inner_e)
                    )

                # Esperar 30 segundos antes del próximo chequeo
                await asyncio.sleep(30)

            except asyncio.CancelledError:
                logger.info("🛑 Background task de heartbeat cancelada")
                break
            except Exception as e:
                logger.error(
                    "Error no esperado en heartbeat",
                    error=str(e)
                )
                await asyncio.sleep(30)  # Reintentar después de 30s

    finally:
        _heartbeat_running = False
        logger.info("🛑 Background task de heartbeat finalizada")


@asynccontextmanager
async def lifespan(app: FastAPI):
    """
    Gestiona el ciclo de vida de la aplicación.

    - startup: conecta a MongoDB, configura logging, inicia background tasks
    - shutdown: cierra conexiones limpiamente, detiene background tasks
    """
    global _heartbeat_task

    # ─── STARTUP ──────────────────────────────────────────────────────
    configure_logging()
    logger.info("🚀 Iniciando AVÍSENS Backend", environment=settings.environment)

    try:
        await connect_db()
        logger.info("✅ Base de datos conectada")
    except Exception as e:
        logger.error("❌ Fallo al conectar base de datos", error=str(e))
        raise

    # ─── Iniciar background task de heartbeat ──────────────────────────
    try:
        _heartbeat_task = asyncio.create_task(heartbeat_background_task())
        logger.info("✅ Background task de heartbeat programada")
    except Exception as e:
        logger.error("❌ Fallo al iniciar background task", error=str(e))
        # No lanzar excepción aquí; la app puede seguir sin heartbeat

    yield  # La aplicación corre aquí

    # ─── SHUTDOWN ─────────────────────────────────────────────────────
    logger.info("🛑 Cerrando AVÍSENS Backend")

    # Detener background task
    global _heartbeat_running
    _heartbeat_running = False

    if _heartbeat_task:
        _heartbeat_task.cancel()
        try:
            await _heartbeat_task
        except asyncio.CancelledError:
            pass

    await close_db()
    logger.info("✅ Backend cerrado correctamente")


# =============================================================================
# Crear aplicación FastAPI
# =============================================================================
app = FastAPI(
    title="AVÍSENS API",
    description="Backend IoT para monitoreo y automatización de granjas avícolas",
    version="1.0.0",
    docs_url="/docs" if not settings.is_production else None,
    redoc_url="/redoc" if not settings.is_production else None,
    openapi_url="/openapi.json" if not settings.is_production else None,
    lifespan=lifespan,
)

# =============================================================================
# Middlewares
# =============================================================================

# 1. CORS (permite que la app React/Kotlin hable con el backend)
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins_list,
    allow_credentials=True,
    allow_methods=["GET", "POST", "PUT", "DELETE"],
    allow_headers=["*"],
)

# 2. Headers de seguridad
app.add_middleware(SecurityHeadersMiddleware)

# 3. Rate Limiting
setup_rate_limiting(app)

# =============================================================================
# Manejadores de excepciones globales
# =============================================================================
app.add_exception_handler(AVISENSException, avisens_exception_handler)
app.add_exception_handler(RequestValidationError, validation_exception_handler)
app.add_exception_handler(Exception, generic_exception_handler)

# =============================================================================
# Routers
# =============================================================================
app.include_router(auth.router)
app.include_router(sensors.router)
app.include_router(actuators.router)
app.include_router(events.router)


# =============================================================================
# Health Check
# =============================================================================
@app.get("/health", tags=["Sistema"], summary="Verificar estado del servidor")
async def health_check():
    """
    Endpoint de health check para monitoreo (Docker, Kubernetes, etc.).
    Incluye información sobre estado de background tasks.
    """
    return {
        "status": "healthy",
        "service": "avisens-backend",
        "version": "1.0.0",
        "environment": settings.environment,
        "background_tasks": {
            "heartbeat": "running" if _heartbeat_running else "stopped"
        }
    }


@app.get("/", tags=["Sistema"], include_in_schema=False)
async def root():
    """Redirección a la documentación."""
    return {
        "message": "Bienvenido a AVÍSENS API",
        "docs": "/docs",
        "health": "/health",
    }


# =============================================================================
# Punto de entrada (para ejecución directa sin Docker)
# =============================================================================
if __name__ == "__main__":
    import uvicorn

    uvicorn.run(
        "app.main:app",
        host=settings.api_host,
        port=settings.api_port,
        workers=settings.api_workers,
        reload=not settings.is_production,
    )
