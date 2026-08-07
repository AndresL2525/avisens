"""
=============================================================================
Middleware de seguridad.

Añade headers de seguridad HTTP y sanitización básica.
=============================================================================
"""

from fastapi import Request
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.types import ASGIApp


class SecurityHeadersMiddleware(BaseHTTPMiddleware):
    """Añade headers de seguridad recomendados por OWASP.

    - X-Content-Type-Options: nosniff (evita MIME sniffing)
    - X-Frame-Options: DENY (evita clickjacking)
    - Strict-Transport-Security: fuerza HTTPS
    - Content-Security-Policy: reduce XSS
    """

    def __init__(self, app: ASGIApp):
        super().__init__(app)

    async def dispatch(self, request: Request, call_next):
        response = await call_next(request)

        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["X-Frame-Options"] = "DENY"
        response.headers["X-XSS-Protection"] = "1; mode=block"
        response.headers["Referrer-Policy"] = "strict-origin-when-cross-origin"

        # HSTS solo en producción (fuerza HTTPS)
        from app.config import settings
        if settings.is_production:
            response.headers["Strict-Transport-Security"] = "max-age=31536000; includeSubDomains"

        # CSP básico
        response.headers["Content-Security-Policy"] = "default-src 'self'"

        return response
