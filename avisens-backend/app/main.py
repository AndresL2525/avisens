"""
=============================================================================
AVÍSENS Backend - Punto de entrada principal.

FastAPI application con:
- Conexión async a MongoDB (Motor)
- Autenticación JWT (usuarios + dispositivos)
- Rate limiting (SlowAPI)
- Headers de seguridad
- Logging estructurado
- Manejo global de excepciones
- Documentación OpenAPI automática

Flujo de datos:
  ESP32 ──POST /sensors/readings──→ FastAPI ──→ MongoDB Atlas
  ESP32 ──GET  /actuators/commands──→ FastAPI ←── MongoDB
  App   ──GET  /sensors/readings──→ FastAPI ←── MongoDB
=============================================================================
"""

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager

from app.config import settings
from app.database import connect_db, close_db
from app.utils.logger import configure_logging, get_logger
from app.utils.exceptions import (
    AVISENSException,
    avisens_exception_handler,
    validation_exception_handler,
    generic_exception_handler,
)
from app.middleware.rate_limit import setup_rate_limiting, limiter
from app.middleware.security import SecurityHeadersMiddleware

from app.routers import auth, sensors, actuators, events

from fastapi.exceptions import RequestValidationError

logger = get_logger(__name__)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Gestiona el ciclo de vida de la aplicación.

    - startup: conecta a MongoDB, configura logging
    - shutdown: cierra conexiones limpiamente
    """
    # Startup
    configure_logging()
    logger.info("🚀 Iniciando AVÍSENS Backend", environment=settings.environment)

    try:
        await connect_db()
        logger.info("✅ Base de datos conectada")
    except Exception as e:
        logger.error("❌ Fallo al conectar base de datos", error=str(e))
        # En producción, podrías querer fallar silenciosamente o usar retry
        raise

    yield  # La aplicación corre aquí

    # Shutdown
    logger.info("🛑 Cerrando AVÍSENS Backend")
    await close_db()


# =============================================================================
# Crear aplicación FastAPI
# =============================================================================
app = FastAPI(
    title="AVÍSENS API",
    description="Backend IoT para monitoreo y automatización de granjas avícolas",
    version="1.0.0",
    docs_url="/docs" if not settings.is_production else None,  # Desactiva docs en prod
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
    """Endpoint de health check para monitoreo (Docker, Kubernetes, etc.)."""
    return {
        "status": "healthy",
        "service": "avisens-backend",
        "version": "1.0.0",
        "environment": settings.environment,
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
        reload=not settings.is_production,  # Hot reload solo en dev
    )
