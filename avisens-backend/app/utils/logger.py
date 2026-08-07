"""
=============================================================================
Logging estructurado para AVÍSENS.

Usa structlog para producir logs en formato JSON en producción,
lo cual facilita el análisis con herramientas como ELK, Datadog, etc.
En desarrollo usa formato legible para humanos.

IMPORTANTE: NUNCA loguees tokens JWT, contraseñas, ni datos PII.
=============================================================================
"""

import logging
import sys
import structlog
from app.config import settings


def configure_logging():
    """Configura el sistema de logging al iniciar la aplicación."""

    # Nivel de logging según entorno
    log_level = getattr(logging, settings.log_level.upper())

    if settings.log_format == "json":
        # Producción: JSON estructurado para parsing automático
        structlog.configure(
            processors=[
                structlog.stdlib.filter_by_level,
                structlog.stdlib.add_logger_name,
                structlog.stdlib.add_log_level,
                structlog.stdlib.PositionalArgumentsFormatter(),
                structlog.processors.TimeStamper(fmt="iso"),
                structlog.processors.StackInfoRenderer(),
                structlog.processors.format_exc_info,
                structlog.processors.JSONRenderer()
            ],
            context_class=dict,
            logger_factory=structlog.stdlib.LoggerFactory(),
            wrapper_class=structlog.stdlib.BoundLogger,
            cache_logger_on_first_use=True,
        )
    else:
        # Desarrollo: formato legible para humanos
        structlog.configure(
            processors=[
                structlog.stdlib.filter_by_level,
                structlog.stdlib.add_logger_name,
                structlog.stdlib.add_log_level,
                structlog.stdlib.PositionalArgumentsFormatter(),
                structlog.processors.TimeStamper(fmt="%Y-%m-%d %H:%M:%S"),
                structlog.processors.StackInfoRenderer(),
                structlog.processors.format_exc_info,
                structlog.dev.ConsoleRenderer(colors=True)
            ],
            context_class=dict,
            logger_factory=structlog.stdlib.LoggerFactory(),
            wrapper_class=structlog.stdlib.BoundLogger,
            cache_logger_on_first_use=True,
        )

    # Configurar el logger raíz de Python
    logging.basicConfig(
        format="%(message)s",
        stream=sys.stdout,
        level=log_level,
    )


def get_logger(name: str):
    """Obtiene un logger con contexto estructurado.

    Uso:
        logger = get_logger(__name__)
        logger.info("Datos recibidos", sensor_id="galpon_01", temp=25.3)
    """
    return structlog.get_logger(name)
