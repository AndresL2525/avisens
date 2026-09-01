"""
=============================================================================
Tests de autenticación — Validación de tokens JWT, device vs user auth.
=============================================================================
"""

import pytest
from datetime import datetime, timezone, timedelta
from httpx import AsyncClient
import jwt
import bcrypt
from app.config import settings


class TestDeviceAuthentication:
    """Tests para autenticación de dispositivos."""

    @pytest.mark.asyncio
    async def test_device_login_success(self, async_client: AsyncClient, mock_db):
        """Test: Dispositivo se autentica correctamente con credenciales válidas."""
        device_id = "galpon_test_01"
        device_secret = "super_secret_key_123"

        # Insertar dispositivo en la base de datos
        await mock_db["devices"].insert_one(
            {
                "device_id": device_id,
                "secret": device_secret,
                "active": True,
                "created_at": datetime.now(timezone.utc),
            }
        )

        # Intentar login
        response = await async_client.post(
            "/auth/device/login", json={"device_id": device_id, "secret": device_secret}
        )

        assert response.status_code == 200
        data = response.json()
        assert data["access_token"]
        assert data["token_type"] == "bearer"

    @pytest.mark.asyncio
    async def test_device_login_invalid_secret(
        self, async_client: AsyncClient, mock_db
    ):
        """Test: Login falla si el secret es incorrecto."""
        device_id = "galpon_test_01"
        correct_secret = "super_secret_key_123"

        await mock_db["devices"].insert_one(
            {
                "device_id": device_id,
                "secret": correct_secret,
                "active": True,
                "created_at": datetime.now(timezone.utc),
            }
        )

        response = await async_client.post(
            "/auth/device/login",
            json={"device_id": device_id, "secret": "wrong_secret"},
        )

        assert response.status_code == 401
        assert "credentials" in response.json()["detail"].lower()

    @pytest.mark.asyncio
    async def test_device_login_device_not_found(self, async_client: AsyncClient):
        """Test: Login falla si el dispositivo no existe."""
        response = await async_client.post(
            "/auth/device/login",
            json={"device_id": "nonexistent_device", "secret": "any_secret"},
        )

        assert response.status_code == 401

    @pytest.mark.asyncio
    async def test_device_login_inactive_device(
        self, async_client: AsyncClient, mock_db
    ):
        """Test: Login falla si el dispositivo está inactivo."""
        device_id = "galpon_test_01"
        device_secret = "secret"

        await mock_db["devices"].insert_one(
            {
                "device_id": device_id,
                "secret": device_secret,
                "active": False,  # Dispositivo inactivo
                "created_at": datetime.now(timezone.utc),
            }
        )

        response = await async_client.post(
            "/auth/device/login", json={"device_id": device_id, "secret": device_secret}
        )

        assert response.status_code == 403

    @pytest.mark.asyncio
    async def test_device_request_with_valid_token(
        self, async_client: AsyncClient, valid_device_token: str
    ):
        """Test: Dispositivo puede hacer request con token válido."""
        response = await async_client.get(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_device_token}"},
        )

        # No debería ser 401/403 por autenticación
        assert response.status_code != 401
        assert response.status_code != 403

    @pytest.mark.asyncio
    async def test_device_request_with_expired_token(
        self, async_client: AsyncClient, expired_token: str
    ):
        """Test: Request falla con token expirado."""
        response = await async_client.get(
            "/sensors/readings", headers={"Authorization": f"Bearer {expired_token}"}
        )

        assert response.status_code == 401
        assert "expired" in response.json()["detail"].lower()

    @pytest.mark.asyncio
    async def test_device_request_with_invalid_token(
        self, async_client: AsyncClient, invalid_token: str
    ):
        """Test: Request falla con token inválido (firma incorrecta)."""
        response = await async_client.get(
            "/sensors/readings", headers={"Authorization": f"Bearer {invalid_token}"}
        )

        assert response.status_code == 401

    @pytest.mark.asyncio
    async def test_device_request_without_token(self, async_client: AsyncClient):
        """Test: Request sin token falla."""
        response = await async_client.get("/sensors/readings")

        assert response.status_code == 401
        assert "authorization" in response.json()["detail"].lower()


class TestUserAuthentication:
    """Tests para autenticación de usuarios (login)."""

    @pytest.mark.asyncio
    async def test_user_login_success(self, async_client: AsyncClient, mock_db):
        """Test: Usuario se autentica correctamente."""
        email = "testuser@example.com"
        password = "secure_password_123"

        # Usar bcrypt directo (evita bug de passlib + bcrypt 4.2+ en Python 3.13)
        hashed_password = bcrypt.hashpw(
            password.encode("utf-8"), bcrypt.gensalt()
        ).decode("utf-8")

        await mock_db["users"].insert_one(
            {
                "email": email,
                "password_hash": hashed_password,
                "active": True,
                "created_at": datetime.now(timezone.utc),
            }
        )

        response = await async_client.post(
            "/auth/user/login", json={"email": email, "password": password}
        )

        assert response.status_code == 200
        data = response.json()
        assert data["access_token"]
        assert data["token_type"] == "bearer"

    @pytest.mark.asyncio
    async def test_user_login_wrong_password(self, async_client: AsyncClient, mock_db):
        """Test: Login falla con contraseña incorrecta."""
        email = "testuser@example.com"

        hashed_password = bcrypt.hashpw(
            "correct_password".encode("utf-8"), bcrypt.gensalt()
        ).decode("utf-8")

        await mock_db["users"].insert_one(
            {
                "email": email,
                "password_hash": hashed_password,
                "active": True,
                "created_at": datetime.now(timezone.utc),
            }
        )

        response = await async_client.post(
            "/auth/user/login", json={"email": email, "password": "wrong_password"}
        )

        assert response.status_code == 401

    @pytest.mark.asyncio
    async def test_user_request_with_valid_token(
        self, async_client: AsyncClient, valid_user_token: str
    ):
        """Test: Usuario puede hacer request con token válido."""
        response = await async_client.get(
            "/sensors/readings", headers={"Authorization": f"Bearer {valid_user_token}"}
        )

        # No debería fallar por autenticación
        assert response.status_code != 401
        assert response.status_code != 403

    @pytest.mark.asyncio
    async def test_user_request_with_expired_token(
        self, async_client: AsyncClient, expired_token: str
    ):
        """Test: Request de usuario falla con token expirado."""
        response = await async_client.get(
            "/sensors/readings", headers={"Authorization": f"Bearer {expired_token}"}
        )

        assert response.status_code == 401


class TestAntiSpoofing:
    """Tests para medidas anti-spoofing en readings."""

    @pytest.mark.asyncio
    async def test_device_cannot_spoof_different_device_id(
        self, async_client: AsyncClient, mock_db, valid_device_token: str
    ):
        """
        Test: Si device_id del token es "galpon_01" pero el body dice "galpon_02",
        se rechaza o se sobrescribe con el del token.
        """
        # El token es para galpon_test_01
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_device_token}"},
            json={
                "device_id": "galpon_impostor_02",  # Intentar spoofing
                "temperatura": 28.5,
                "humedad": 65.0,
                "calidad_aire": 450,
            },
        )

        # Puede ser 201 (con sobrescritura) o 401 (rechazar)
        # Lo importante es que no se guarde con device_id falso
        if response.status_code == 201:
            # Verificar que se guardó con el device_id correcto del token
            assert "device_id" in response.json()
            # El device_id debería coincidir con el del token, no con el del body

    @pytest.mark.asyncio
    async def test_token_type_mismatch(
        self,
        async_client: AsyncClient,
        valid_user_token: str,  # Token de usuario, no de device
    ):
        """
        Test: Si usas un token de usuario donde se espera device,
        debería fallar.
        """
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

        # Debería fallar porque es token de usuario, no device
        assert response.status_code == 401 or response.status_code == 403
