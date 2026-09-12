"""
=============================================================================
Configuración compartida para pytest — Fixtures, mocks y base de datos simulada.
=============================================================================
"""

import pytest
import pytest_asyncio
from datetime import datetime, timezone, timedelta
from typing import AsyncGenerator
from mongomock_motor import AsyncMongoMockClient
import jwt
import uuid
from app.config import settings
from httpx import AsyncClient, ASGITransport

# ═══════════════════════════════════════════════════════════
# ─── FIXTURES DE BASE DE DATOS ──────────────────────────────
# ═══════════════════════════════════════════════════════════


@pytest_asyncio.fixture
async def mock_db() -> AsyncGenerator:
    """Crea una base de datos MongoDB simulada (mongomock_motor)."""
    client = AsyncMongoMockClient()
    db = client[settings.mongodb_db_name]

    await db["sensor_readings"].create_index("device_id")
    await db["sensor_readings"].create_index("timestamp")
    await db["events"].create_index("device_id")
    await db["events"].create_index("tipo")
    await db["users"].create_index("email", unique=True)
    await db["devices"].create_index("device_id", unique=True)

    yield db


# ═══════════════════════════════════════════════════════════
# ─── HELPERS JWT ────────────────────────────────────────────
# ═══════════════════════════════════════════════════════════


def _make_token(sub: str, token_type: str, exp_offset: int = 3600):
    """Crea token JWT con jti (requerido por la app)."""
    now = datetime.now(timezone.utc)
    payload = {
        "sub": sub,
        "type": token_type,
        "jti": str(uuid.uuid4()),
        "iat": now,
        "exp": now.timestamp() + exp_offset,
    }
    return jwt.encode(payload, settings.jwt_secret_key, algorithm="HS256")


# ═══════════════════════════════════════════════════════════
# ─── FIXTURES DE TOKENS JWT ─────────────────────────────────
# ═══════════════════════════════════════════════════════════


@pytest.fixture
def valid_device_token() -> str:
    """Token JWT válido para dispositivo."""
    return _make_token("galpon_test_01", "device")


@pytest.fixture
def valid_user_token() -> str:
    """Token JWT válido para usuario."""
    return _make_token("user_test_01@example.com", "user")


@pytest.fixture
def expired_token() -> str:
    """Token JWT expirado."""
    return _make_token("galpon_test_01", "device", exp_offset=-3600)


@pytest.fixture
def invalid_token() -> str:
    """Token JWT firmado con clave incorrecta."""
    payload = {
        "sub": "galpon_test_01",
        "type": "device",
        "jti": str(uuid.uuid4()),
    }
    return jwt.encode(payload, "wrong_secret_key", algorithm="HS256")


# ═══════════════════════════════════════════════════════════
# ─── FIXTURES DE DATOS DE PRUEBA ────────────────────────────
# ═══════════════════════════════════════════════════════════


@pytest.fixture
def sample_sensor_reading():
    return {
        "device_id": "galpon_test_01",
        "temperatura": 28.5,
        "humedad": 65.0,
        "calidad_aire": 450,
        "timestamp": datetime.now(timezone.utc),
    }


@pytest.fixture
def sample_sensor_reading_outlier():
    return {
        "device_id": "galpon_test_02",
        "temperatura": 75.5,
        "humedad": 65.0,
        "calidad_aire": 450,
        "timestamp": datetime.now(timezone.utc),
    }


@pytest.fixture
def sample_event():
    return {
        "device_id": "galpon_test_01",
        "tipo": "ALERTA",
        "origen": "AlertService",
        "mensaje": "Anomalía detectada en sensores",
        "nivel": "advertencia",
        "timestamp": datetime.now(timezone.utc),
        "metadata": {"temperatura": 28.5, "humedad": 65.0, "anomalias_detectadas": 1},
    }


# ═══════════════════════════════════════════════════════════
# ─── OVERRIDE DE DEPENDENCIAS ───────────────────────────────
# ═══════════════════════════════════════════════════════════


@pytest_asyncio.fixture
async def app_with_mock_db(mock_db):
    from app.main import app

    async def override_get_database():
        return mock_db

    from app.database import get_database

    app.dependency_overrides[get_database] = override_get_database

    yield app
    app.dependency_overrides.clear()


@pytest_asyncio.fixture
async def async_client(app_with_mock_db) -> AsyncGenerator[AsyncClient, None]:
    transport = ASGITransport(app=app_with_mock_db)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        yield client


# ═══════════════════════════════════════════════════════════
# ─── FIXTURES DE HELPERS ────────────────────────────────────
# ═══════════════════════════════════════════════════════════


@pytest_asyncio.fixture
async def populate_sensor_readings(mock_db):
    readings = [
        {
            "device_id": "galpon_test_01",
            "temperatura": 28.0 + i * 0.5,
            "humedad": 60.0 + i,
            "calidad_aire": 400 + i * 10,
            "timestamp": datetime.now(timezone.utc),
            "received_at": datetime.now(timezone.utc),
        }
        for i in range(10)
    ]
    result = await mock_db["sensor_readings"].insert_many(readings)
    return result.inserted_ids


@pytest_asyncio.fixture
async def populate_events(mock_db):
    events = [
        {
            "device_id": "galpon_test_01",
            "tipo": "ALERTA",
            "origen": "AlertService",
            "mensaje": f"Evento de alerta #{i}",
            "nivel": "advertencia",
            "timestamp": datetime.now(timezone.utc),
            "received_at": datetime.now(timezone.utc),
            "metadata": {},
        }
        for i in range(5)
    ]
    result = await mock_db["events"].insert_many(events)
    return result.inserted_ids
