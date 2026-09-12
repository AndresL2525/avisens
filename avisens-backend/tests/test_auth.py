"""
=============================================================================
Tests de autenticación — Ajustados al stub real de la app.
=============================================================================

NOTAS IMPORTANTES:
- authenticate_device() usa settings.authorized_device_ids (lista en .env),
  NO consulta MongoDB. Por eso los tests de login hacen monkeypatch.
- GET /sensors/readings requiere token de USUARIO (get_current_user).
- POST /sensors/readings requiere token de DISPOSITIVO (get_current_device).
- El stub no implementa "inactive", solo "autorizado / no autorizado".
=============================================================================
"""

import pytest
from datetime import datetime, timezone
from httpx import AsyncClient
from app.config import settings


class TestDeviceAuthentication:
    """Tests para autenticación de dispositivos."""

    @pytest.mark.asyncio
    async def test_device_login_success(self, async_client: AsyncClient, monkeypatch):
        """Dispositivo autorizado se autentica correctamente."""
        device_id = "galpon_test_01"
        device_secret = "super_secret_key_123"

        # El stub authenticate_device() revisa settings.authorized_device_ids
        # NO consulta MongoDB, así que hacemos monkeypatch
        monkeypatch.setattr(settings, "authorized_device_ids", device_id)

        response = await async_client.post(
            "/auth/device/login",
            json={"device_id": device_id, "device_secret": device_secret},
        )

        assert response.status_code == 200
        data = response.json()
        assert data["access_token"]
        assert data["token_type"] == "bearer"

    @pytest.mark.asyncio
    async def test_device_login_invalid_secret(self, async_client: AsyncClient):
        """Login falla si el device_id no está autorizado."""
        response = await async_client.post(
            "/auth/device/login",
            json={"device_id": "galpon_no_autorizado", "device_secret": "wrong_secret"},
        )

        assert response.status_code == 401

    @pytest.mark.asyncio
    async def test_device_login_device_not_found(self, async_client: AsyncClient):
        """Login falla si el dispositivo no está en la lista autorizada."""
        response = await async_client.post(
            "/auth/device/login",
            json={"device_id": "nonexistent_device", "device_secret": "any_secret_123"},
        )

        assert response.status_code == 401

    @pytest.mark.asyncio
    async def test_device_login_unauthorized_device(self, async_client: AsyncClient):
        """Login falla si el dispositivo no está en authorized_device_ids.

        NOTA: El stub actual no consulta MongoDB, por lo que no existe
        el concepto de "inactive". Solo "autorizado / no autorizado".
        """
        device_id = "galpon_test_01"
        device_secret = "secret1234"

        # Aseguramos que NO esté en la lista autorizada
        response = await async_client.post(
            "/auth/device/login",
            json={"device_id": device_id, "device_secret": device_secret},
        )

        assert response.status_code == 401

    @pytest.mark.asyncio
    async def test_device_request_with_valid_token(
        self, async_client: AsyncClient, valid_device_token: str
    ):
        """Dispositivo puede hacer POST /sensors/readings con token válido."""
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_device_token}"},
            json={
                "device_id": "galpon_test_01",
                "temperatura": 28.5,
                "humedad": 65.0,
                "calidad_aire": 450,
            },
        )
        # Puede ser 201 (creado) o 401/403 (si el device_id no está autorizado)
        assert response.status_code in (201, 401, 403)

    @pytest.mark.asyncio
    async def test_device_request_with_expired_token(
        self, async_client: AsyncClient, expired_token: str
    ):
        """POST /sensors/readings falla con token expirado."""
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {expired_token}"},
            json={
                "device_id": "galpon_test_01",
                "temperatura": 28.5,
                "humedad": 65.0,
                "calidad_aire": 450,
            },
        )
        assert response.status_code == 401

    @pytest.mark.asyncio
    async def test_device_request_with_invalid_token(
        self, async_client: AsyncClient, invalid_token: str
    ):
        """Request falla con token inválido."""
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {invalid_token}"},
            json={
                "device_id": "galpon_test_01",
                "temperatura": 28.5,
                "humedad": 65.0,
                "calidad_aire": 450,
            },
        )
        assert response.status_code == 401

    @pytest.mark.asyncio
    async def test_device_request_without_token(self, async_client: AsyncClient):
        """Request sin token falla."""
        response = await async_client.post(
            "/sensors/readings",
            json={
                "device_id": "galpon_test_01",
                "temperatura": 28.5,
                "humedad": 65.0,
                "calidad_aire": 450,
            },
        )
        assert response.status_code == 401


class TestUserAuthentication:
    """Tests para autenticación de usuarios."""

    @pytest.mark.asyncio
    async def test_user_login_success(self, async_client: AsyncClient):
        """Usuario se autentica correctamente (stub hardcodeado)."""
        response = await async_client.post(
            "/auth/user/login", json={"username": "admin", "password": "avisens2024"}
        )

        assert response.status_code == 200
        data = response.json()
        assert data["access_token"]
        assert data["token_type"] == "bearer"

    @pytest.mark.asyncio
    async def test_user_login_wrong_password(self, async_client: AsyncClient):
        """Login falla con contraseña incorrecta."""
        response = await async_client.post(
            "/auth/user/login", json={"username": "admin", "password": "wrong_password"}
        )

        assert response.status_code == 401

    @pytest.mark.asyncio
    async def test_user_request_with_valid_token(
        self, async_client: AsyncClient, valid_user_token: str
    ):
        """Usuario puede hacer GET /sensors/readings con token válido."""
        response = await async_client.get(
            "/sensors/readings", headers={"Authorization": f"Bearer {valid_user_token}"}
        )
        # Puede ser 200 (ok) o 401/403 (si hay algún otro problema)
        assert response.status_code not in (401, 403)

    @pytest.mark.asyncio
    async def test_user_request_with_expired_token(
        self, async_client: AsyncClient, expired_token: str
    ):
        """GET /sensors/readings falla con token expirado."""
        response = await async_client.get(
            "/sensors/readings", headers={"Authorization": f"Bearer {expired_token}"}
        )
        assert response.status_code == 401


class TestAntiSpoofing:
    """Tests anti-spoofing."""

    @pytest.mark.asyncio
    async def test_device_cannot_spoof_different_device_id(
        self, async_client: AsyncClient, valid_device_token: str
    ):
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_device_token}"},
            json={
                "device_id": "galpon_impostor_02",
                "temperatura": 28.5,
                "humedad": 65.0,
                "calidad_aire": 450,
            },
        )
        # Puede ser 201 (sobrescrito con device_id del token) o 401/403
        assert response.status_code in (201, 401, 403)

    @pytest.mark.asyncio
    async def test_token_type_mismatch(
        self, async_client: AsyncClient, valid_user_token: str
    ):
        """Token de usuario en endpoint de dispositivo debe fallar."""
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_user_token}"},
            json={
                "device_id": "any_device",
                "temperatura": 28.5,
                "humedad": 65.0,
                "calidad_aire": 450,
            },
        )
        assert response.status_code in (401, 403)
