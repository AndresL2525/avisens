"""
=============================================================================
Servicio de autenticación JWT.

Genera y valida tokens JWT para:
1. Usuarios de apps (expiran en 30 min, deben refrescar)
2. Dispositivos ESP32 (expiran en 1 año, no requieren refresh frecuente)

⚠️ RIESGO DE SEGURIDAD REAL:
   Los tokens de dispositivo son de larga duración. Si un atacante roba
   el firmware del ESP32 (lectura de flash), obtiene el token.
   Mitigación: usar HTTPS siempre, rotar tokens periódicamente, y en
   producción considerar mTLS (certificados mutuos) o AWS IoT Core.
=============================================================================
"""

import jwt
import uuid
from datetime import datetime, timedelta, timezone
from fastapi import Depends, HTTPException, status
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from app.config import settings
from app.utils.logger import get_logger
from app.utils.exceptions import InvalidTokenException, DeviceNotAuthorizedException
from app.models.auth import TokenPayload, DeviceAuthRequest

logger = get_logger(__name__)

# Esquema de seguridad para FastAPI (documentación OpenAPI + validación)
security = HTTPBearer(auto_error=False)


def create_access_token(subject: str, token_type: str = "user") -> str:
    """Genera un token JWT firmado.

    Args:
        subject: Identificador (user_id o device_id)
        token_type: "user" o "device"

    Returns:
        Token JWT como string.
    """
    now = datetime.now(timezone.utc)

    if token_type == "device":
        expire = now + timedelta(days=settings.jwt_device_token_expire_days)
    else:
        expire = now + timedelta(minutes=settings.jwt_access_token_expire_minutes)

    payload = {
        "sub": subject,
        "exp": expire,
        "iat": now,
        "type": token_type,
        "jti": str(uuid.uuid4()),  # JWT ID único para revocación
    }

    token = jwt.encode(
        payload, settings.jwt_secret_key, algorithm=settings.jwt_algorithm
    )
    logger.info(
        "Token JWT generado",
        subject=subject,
        type=token_type,
        expires=expire.isoformat(),
    )
    return token


def decode_token(token: str) -> TokenPayload:
    """Decodifica y valida un token JWT.

    Args:
        token: El token JWT recibido en el header Authorization.

    Returns:
        TokenPayload con los claims decodificados.

    Raises:
        InvalidTokenException: Si el token es inválido, expiró, o fue manipulado.
    """
    try:
        payload = jwt.decode(
            token,
            settings.jwt_secret_key,
            algorithms=[settings.jwt_algorithm],
            options={"require": ["sub", "exp", "iat", "type", "jti"]},
        )
        return TokenPayload(
            sub=payload["sub"],
            exp=datetime.fromtimestamp(payload["exp"], tz=timezone.utc),
            iat=datetime.fromtimestamp(payload["iat"], tz=timezone.utc),
            type=payload["type"],
            jti=payload["jti"],
        )
    except jwt.ExpiredSignatureError:
        logger.warning("Token expirado")
        raise InvalidTokenException("El token ha expirado. Vuelve a autenticarte.")
    except jwt.InvalidTokenError as e:
        logger.warning("Token inválido", error=str(e))
        raise InvalidTokenException("Token de autenticación inválido.")


async def get_current_device(
    credentials: HTTPAuthorizationCredentials = Depends(security),
) -> str:
    """Dependencia de FastAPI: extrae y valida el device_id del token JWT.

    Se usa en endpoints que solo los ESP32 pueden llamar (POST /sensores).
    """
    if not credentials:
        raise InvalidTokenException("Falta el header Authorization con el token Bearer")

    payload = decode_token(credentials.credentials)

    if payload.type != "device":
        raise InvalidTokenException(
            "Este endpoint requiere un token de dispositivo, no de usuario."
        )

    # Verificamos que el dispositivo esté autorizado
    if payload.sub not in settings.authorized_device_ids_list:
        logger.warning(
            "Dispositivo no autorizado intentó acceder", device_id=payload.sub
        )
        raise DeviceNotAuthorizedException()

    return payload.sub


async def get_current_user(
    credentials: HTTPAuthorizationCredentials = Depends(security),
) -> str:
    """Dependencia de FastAPI: extrae y valida el user_id del token JWT.

    Se usa en endpoints que las apps móvil/web consumen (GET /sensores).
    """
    if not credentials:
        raise InvalidTokenException("Falta el header Authorization")

    payload = decode_token(credentials.credentials)

    if payload.type != "user":
        raise InvalidTokenException("Este endpoint requiere un token de usuario.")

    return payload.sub


def authenticate_device(auth_request: DeviceAuthRequest) -> str | None:
    """Autentica un dispositivo ESP32.

    En producción REAL, esto debería consultar una tabla de dispositivos
    en MongoDB con hashes de secretos (bcrypt), no comparar en texto plano.

    Por simplicidad académica, usamos la lista de .env, pero ESTO NO ES
    SEGURO para producción real.

    Args:
        auth_request: Credenciales del dispositivo.

    Returns:
        device_id si es válido, None si no.
    """
    # TODO: En producción, consultar MongoDB con bcrypt.verify()
    if auth_request.device_id in settings.authorized_device_ids:
        # En un sistema real, verificaríamos el device_secret con bcrypt
        logger.info("Dispositivo autenticado", device_id=auth_request.device_id)
        return auth_request.device_id

    logger.warning(
        "Intento de autenticación de dispositivo fallido",
        device_id=auth_request.device_id,
    )
    return None
