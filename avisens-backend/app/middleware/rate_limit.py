"""
=============================================================================
Rate Limiting con SlowAPI.

Protege contra:
- Fuerza bruta en login
- Spam de datos del ESP32
- Scraping de la API

SlowAPI usa un almacenamiento en memoria (MemoryStorage).
En producción con múltiples workers, necesitas RedisBackend.
=============================================================================
"""

from slowapi import Limiter, _rate_limit_exceeded_handler
from slowapi.util import get_remote_address
from slowapi.errors import RateLimitExceeded
from fastapi import Request
from app.config import settings

# Limiter global: identifica clientes por IP
limiter = Limiter(
    key_func=get_remote_address,
    default_limits=[f"{settings.rate_limit_per_minute} per minute"]
)


def setup_rate_limiting(app):
    """Configura el rate limiting en la aplicación FastAPI."""
    app.state.limiter = limiter
    app.add_exception_handler(RateLimitExceeded, _rate_limit_exceeded_handler)
