"""
=============================================================================
Configuración compartida para pytest — Fixtures, mocks y base de datos simulada.
=============================================================================
"""

import pytest
import pytest_asyncio
import asyncio
from datetime import datetime, timezone, timedelta
from typing import AsyncGenerator
from mongomock_motor import AsyncMongoMockClient
import jwt
from app.config import settings
from httpx import AsyncClient, ASGITransport

# ═══════════════════════════════════════════════════════════
# ─── FIXTURES DE BASE DE DATOS ──────────────────────────────
# ═══════════════════════════════════════════════════════════


@pytest_asyncio.fixture
async def mock_db() -> AsyncGenerator:
    """
    Crea una base de datos MongoDB simulada (mongomock_motor).
    Se usa en lugar de MongoDB real para tests.
    """
    client = AsyncMongoMockClient()
    db = client[settings.mongodb_name]

    # Crear índices necesarios
    await db["sensor_readings"].create_index("device_id")
    await db["sensor_readings"].create_index("timestamp")
    await db["events"].create_index("device_id")
    await db["events"].create_index("tipo")
    await db["users"].create_index("email", unique=True)
    await db["devices"].create_index("device_id", unique=True)

    yield db

    # Limpiar después de cada test
    await client.drop_database(settings.mongodb_name)


# ═══════════════════════════════════════════════════════════
# ─── FIXTURES DE TOKENS JWT ─────────────────────────────────
# ═══════════════════════════════════════════════════════════


@pytest.fixture
def valid_device_token() -> str:
    """Genera un token JWT válido para dispositivo."""
    payload = {
        "sub": "galpon_test_01",
        "type": "device",
        "iat": datetime.now(timezone.utc),
        "exp": datetime.now(timezone.utc).timestamp() + 3600,
    }
    token = jwt.encode(payload, settings.jwt_secret_key, algorithm="HS256")
    return token


@pytest.fixture
def valid_user_token() -> str:
    """Genera un token JWT válido para usuario."""
    payload = {
        "sub": "user_test_01@example.com",
        "type": "user",
        "iat": datetime.now(timezone.utc),
        "exp": datetime.now(timezone.utc).timestamp() + 3600,
    }
    token = jwt.encode(payload, settings.jwt_secret_key, algorithm="HS256")
    return token


@pytest.fixture
def expired_token() -> str:
    """Genera un token JWT expirado."""
    past_time = datetime.now(timezone.utc) - timedelta(hours=1)
    payload = {
        "sub": "galpon_test_01",
        "type": "device",
        "iat": past_time,
        "exp": past_time.timestamp() + 3600,
    }
    token = jwt.encode(payload, settings.jwt_secret_key, algorithm="HS256")
    return token


@pytest.fixture
def invalid_token() -> str:
    """Token JWT firmado con clave incorrecta."""
    payload = {"sub": "galpon_test_01", "type": "device"}
    token = jwt.encode(
        payload, "wrong_secret_key", algorithm="HS256"  # Clave incorrecta
    )
    return token


# ═══════════════════════════════════════════════════════════
# ─── FIXTURES DE DATOS DE PRUEBA ────────────────────────────
# ═══════════════════════════════════════════════════════════


@pytest.fixture
def sample_sensor_reading():
    """Lectura de sensor válida para testing."""
    return {
        "device_id": "galpon_test_01",
        "temperatura": 28.5,
        "humedad": 65.0,
        "calidad_aire": 450,
        "timestamp": datetime.now(timezone.utc),
    }


@pytest.fixture
def sample_sensor_reading_outlier():
    """Lectura de sensor que dispara alerta (fuera de rango)."""
    return {
        "device_id": "galpon_test_02",
        "temperatura": 75.5,  # Fuera de rango (-10, 60)
        "humedad": 65.0,
        "calidad_aire": 450,
        "timestamp": datetime.now(timezone.utc),
    }


@pytest.fixture
def sample_event():
    """Evento de alerta para testing."""
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
# ─── OVERRIDE DE DEPENDENCIAS PARA TESTING ──────────────────
# ═══════════════════════════════════════════════════════════


@pytest_asyncio.fixture
async def app_with_mock_db(mock_db):
    """FastAPI app con base de datos mockeada."""
    from app.main import app

    async def override_get_database():
        return mock_db

    from app.database import get_database

    app.dependency_overrides[get_database] = override_get_database

    yield app

    app.dependency_overrides.clear()


@pytest_asyncio.fixture
async def async_client(app_with_mock_db) -> AsyncGenerator[AsyncClient, None]:
    """Cliente HTTP asíncrono para testing."""
    transport = ASGITransport(app=app_with_mock_db)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        yield client


# ═══════════════════════════════════════════════════════════
# ─── FIXTURES DE HELPERS ────────────────────────────────────
# ═══════════════════════════════════════════════════════════


@pytest_asyncio.fixture
async def populate_sensor_readings(mock_db):
    """Inserta lecturas de sensores de prueba en la DB."""
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
    """Inserta eventos de prueba en la DB."""
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
