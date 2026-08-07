"""
=============================================================================
Excepciones personalizadas y manejador global de errores.

FastAPI captura estas excepciones y las convierte en respuestas HTTP
estructuradas con código de estado apropiado.
=============================================================================
"""

from fastapi import Request, status
from fastapi.responses import JSONResponse
from fastapi.exceptions import RequestValidationError
from app.utils.logger import get_logger

logger = get_logger(__name__)


# ---------------------------------------------------------------------------
# Excepciones de Negocio
# ---------------------------------------------------------------------------

class AVISENSException(Exception):
    """Excepción base de la aplicación."""
    status_code: int = status.HTTP_500_INTERNAL_SERVER_ERROR
    detail: str = "Error interno del servidor"

    def __init__(self, detail: str | None = None):
        if detail:
            self.detail = detail
        super().__init__(self.detail)


class DeviceNotAuthorizedException(AVISENSException):
    """El dispositivo no está en la lista de autorizados."""
    status_code = status.HTTP_403_FORBIDDEN
    detail = "Dispositivo no autorizado"


class InvalidTokenException(AVISENSException):
    """Token JWT inválido o expirado."""
    status_code = status.HTTP_401_UNAUTHORIZED
    detail = "Token de autenticación inválido o expirado"


class SensorValidationException(AVISENSException):
    """Los datos del sensor no pasaron la validación."""
    status_code = status.HTTP_422_UNPROCESSABLE_ENTITY
    detail = "Datos de sensor inválidos"


class RateLimitExceededException(AVISENSException):
    """Se excedió el límite de peticiones."""
    status_code = status.HTTP_429_TOO_MANY_REQUESTS
    detail = "Demasiadas peticiones. Intenta más tarde."


class DatabaseConnectionException(AVISENSException):
    """Fallo de conexión a la base de datos."""
    status_code = status.HTTP_503_SERVICE_UNAVAILABLE
    detail = "Servicio de base de datos no disponible"


# ---------------------------------------------------------------------------
# Manejadores Globales
# ---------------------------------------------------------------------------

async def avisens_exception_handler(request: Request, exc: AVISENSException):
    """Maneja todas las excepciones personalizadas de AVÍSENS."""
    logger.warning(
        "Excepción de negocio capturada",
        path=request.url.path,
        status=exc.status_code,
        detail=exc.detail,
    )
    return JSONResponse(
        status_code=exc.status_code,
        content={
            "error": True,
            "code": exc.__class__.__name__,
            "message": exc.detail,
            "path": str(request.url.path),
        }
    )


async def validation_exception_handler(request: Request, exc: RequestValidationError):
    """Maneja errores de validación de Pydantic (inputs malformados).

    Esto es CRÍTICO para seguridad: un atacante podría enviar payloads
    maliciosos para explotar vulnerabilidades. Pydantic los bloquea antes
    de que lleguen a la lógica de negocio.
    """
    errors = []
    for error in exc.errors():
        # Sanitizamos: no exponemos detalles internos que puedan ayudar a un atacante
        field = ".".join(str(loc) for loc in error.get("loc", []))
        errors.append({
            "field": field,
            "message": error.get("msg", "Valor inválido"),
            "type": error.get("type", "unknown")
        })

    logger.warning(
        "Validación fallida",
        path=request.url.path,
        errors_count=len(errors),
        client_ip=request.client.host if request.client else "unknown",
    )

    return JSONResponse(
        status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
        content={
            "error": True,
            "code": "VALIDATION_ERROR",
            "message": "Los datos enviados no son válidos",
            "details": errors,
        }
    )


async def generic_exception_handler(request: Request, exc: Exception):
    """Manejador de último recurso para excepciones no controladas.

    En producción NUNCA exponemos el traceback al cliente.
    Solo lo logueamos internamente para debugging.
    """
    logger.error(
        "Error interno no controlado",
        path=request.url.path,
        exception_type=exc.__class__.__name__,
        exception_msg=str(exc),
        exc_info=True,  # Incluye traceback en el log
    )

    message = "Error interno del servidor"
    if not settings.is_production:
        # En desarrollo sí mostramos detalles para facilitar debugging
        message = f"{exc.__class__.__name__}: {str(exc)}"

    return JSONResponse(
        status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
        content={
            "error": True,
            "code": "INTERNAL_SERVER_ERROR",
            "message": message,
        }
    )


# Importamos settings aquí para evitar import circular
from app.config import settings
