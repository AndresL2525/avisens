"""
=============================================================================
Router de autenticación.

Endpoints:
  POST /auth/device/login   → El ESP32 obtiene su token JWT
  POST /auth/user/login     → Las apps obtienen token de usuario (stub)
  POST /auth/refresh        → Refrescar token (stub para futuro)
=============================================================================
"""

from fastapi import APIRouter, HTTPException, status
from app.models.auth import Token, DeviceAuthRequest, UserAuthRequest
from app.services.auth_service import create_access_token, authenticate_device
from app.utils.logger import get_logger

logger = get_logger(__name__)
router = APIRouter(prefix="/auth", tags=["Autenticación"])


@router.post("/device/login", response_model=Token, status_code=status.HTTP_200_OK)
async def device_login(auth_request: DeviceAuthRequest):
    """Autentica un dispositivo ESP32 y devuelve un token JWT.

    El ESP32 debe enviar su device_id y device_secret.
    El token tiene 1 año de validez (los dispositivos IoT no pueden
    refrescar tokens fácilmente sin intervención humana).

    ⚠️ IMPORTANTE: Este endpoint DEBE usarse solo UNA VEZ por dispositivo
    (al configurarlo), o cuando el token expire. El ESP32 debe almacenar
    el token en su memoria flash (SPIFFS/Preferences).
    """
    device_id = authenticate_device(auth_request)

    if not device_id:
        logger.warning("Autenticación de dispositivo fallida", device_id=auth_request.device_id)
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Credenciales de dispositivo inválidas",
            headers={"WWW-Authenticate": "Bearer"},
        )

    token = create_access_token(subject=device_id, token_type="device")

    return Token(
        access_token=token,
        token_type="bearer",
        token_kind="device"
    )


@router.post("/user/login", response_model=Token, status_code=status.HTTP_200_OK)
async def user_login(auth_request: UserAuthRequest):
    """Autentica un usuario de app y devuelve token JWT.

    ⚠️ STUB: En un sistema real, esto consultaría el backend de tus
    compañeros o una tabla local de usuarios con bcrypt.

    Por ahora, usa credenciales de prueba:
      username: admin
      password: avisens2024
    """
    # STUB: Autenticación básica para demostración
    # TODO: Integrar con el backend de autenticación de tus compañeros
    if auth_request.username == "admin" and auth_request.password == "avisens2024":
        token = create_access_token(subject=auth_request.username, token_type="user")
        return Token(
            access_token=token,
            token_type="bearer",
            token_kind="user"
        )

    raise HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Credenciales de usuario inválidas",
        headers={"WWW-Authenticate": "Bearer"},
    )
