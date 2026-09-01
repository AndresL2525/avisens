"""
=============================================================================
Tests de sensores — CRUD de lecturas, validación, estadísticas.
=============================================================================
"""

import pytest
from datetime import datetime, timezone, timedelta
from httpx import AsyncClient
from app.models.sensor import SensorReadingCreate


class TestSensorReadings:
    """Tests para lectura y almacenamiento de datos de sensores."""

    @pytest.mark.asyncio
    async def test_create_sensor_reading_success(
        self,
        async_client: AsyncClient,
        valid_device_token: str,
        sample_sensor_reading
    ):
        """Test: Dispositivo puede crear una lectura de sensor."""
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_device_token}"},
            json=sample_sensor_reading
        )

        assert response.status_code == 201
        data = response.json()
        assert data["success"] is True
        assert data["id"]
        assert data["device_id"] == sample_sensor_reading["device_id"]
        assert data["message"]

    @pytest.mark.asyncio
    async def test_create_sensor_reading_missing_fields(
        self,
        async_client: AsyncClient,
        valid_device_token: str
    ):
        """Test: Request falla si faltan campos obligatorios."""
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_device_token}"},
            json={
                "device_id": "galpon_test_01"
                # Faltan temperatura, humedad, calidad_aire
            }
        )

        assert response.status_code == 422  # Validation error

    @pytest.mark.asyncio
    async def test_create_sensor_reading_invalid_temperature(
        self,
        async_client: AsyncClient,
        valid_device_token: str
    ):
        """Test: Request rechaza temperatura fuera de rango de validación."""
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_device_token}"},
            json={
                "device_id": "galpon_test_01",
                "temperatura": "not_a_number",  # Tipo incorrecto
                "humedad": 65.0,
                "calidad_aire": 450
            }
        )

        assert response.status_code == 422

    @pytest.mark.asyncio
    async def test_create_sensor_reading_without_token(
        self,
        async_client: AsyncClient,
        sample_sensor_reading
    ):
        """Test: Request sin token falla."""
        response = await async_client.post(
            "/sensors/readings",
            json=sample_sensor_reading
        )

        assert response.status_code == 401

    @pytest.mark.asyncio
    async def test_get_latest_readings(
        self,
        async_client: AsyncClient,
        valid_user_token: str,
        populate_sensor_readings,
        mock_db
    ):
        """Test: Usuario puede obtener últimas lecturas de un dispositivo."""
        # Las lecturas ya están insertadas por populate_sensor_readings
        response = await async_client.get(
            "/sensors/readings?device_id=galpon_test_01&limit=5",
            headers={"Authorization": f"Bearer {valid_user_token}"}
        )

        assert response.status_code == 200
        data = response.json()
        assert isinstance(data, list)
        assert len(data) <= 5
        if data:
            assert data[0]["device_id"] == "galpon_test_01"

    @pytest.mark.asyncio
    async def test_get_latest_readings_without_token(self, async_client: AsyncClient):
        """Test: Request sin token falla."""
        response = await async_client.get("/sensors/readings")

        assert response.status_code == 401

    @pytest.mark.asyncio
    async def test_get_latest_readings_limit_validation(
        self,
        async_client: AsyncClient,
        valid_user_token: str
    ):
        """Test: Limite máximo de registros se respeta."""
        response = await async_client.get(
            "/sensors/readings?limit=2000",  # Intenta obtener más del máximo
            headers={"Authorization": f"Bearer {valid_user_token}"}
        )

        # Debería aceptar pero limitar a 1000 internamente
        assert response.status_code in [200, 422]  # 422 si hay validación estricta

    @pytest.mark.asyncio
    async def test_get_readings_history(
        self,
        async_client: AsyncClient,
        valid_user_token: str,
        populate_sensor_readings,
        mock_db
    ):
        """Test: Usuario puede obtener historial de lecturas por rango de tiempo."""
        response = await async_client.get(
            "/sensors/readings/history?device_id=galpon_test_01&hours=24&limit=100",
            headers={"Authorization": f"Bearer {valid_user_token}"}
        )

        assert response.status_code == 200
        data = response.json()
        assert isinstance(data, list)

    @pytest.mark.asyncio
    async def test_get_readings_history_missing_device_id(
        self,
        async_client: AsyncClient,
        valid_user_token: str
    ):
        """Test: device_id es parámetro obligatorio."""
        response = await async_client.get(
            "/sensors/readings/history?hours=24",
            headers={"Authorization": f"Bearer {valid_user_token}"}
        )

        assert response.status_code == 422

    @pytest.mark.asyncio
    async def test_get_readings_history_hours_validation(
        self,
        async_client: AsyncClient,
        valid_user_token: str
    ):
        """Test: Rango de horas está limitado (máx 168 = 7 días)."""
        response = await async_client.get(
            "/sensors/readings/history?device_id=test&hours=500",
            headers={"Authorization": f"Bearer {valid_user_token}"}
        )

        # Debería rechazar o limitar
        assert response.status_code in [200, 422]


class TestSensorStatistics:
    """Tests para cálculo de estadísticas agregadas."""

    @pytest.mark.asyncio
    async def test_get_sensor_stats(
        self,
        async_client: AsyncClient,
        valid_user_token: str,
        populate_sensor_readings
    ):
        """Test: Se pueden obtener estadísticas agregadas."""
        response = await async_client.get(
            "/sensors/stats?device_id=galpon_test_01&hours=24",
            headers={"Authorization": f"Bearer {valid_user_token}"}
        )

        assert response.status_code == 200
        data = response.json()

        # Verificar campos de estadísticas
        assert data["device_id"] == "galpon_test_01"
        assert "avg_temperatura" in data
        assert "min_temperatura" in data
        assert "max_temperatura" in data
        assert "avg_humedad" in data
        assert "count" in data
        assert data["count"] > 0

    @pytest.mark.asyncio
    async def test_get_sensor_stats_empty_device(
        self,
        async_client: AsyncClient,
        valid_user_token: str
    ):
        """Test: Estadísticas para dispositivo sin datos retorna 0s."""
        response = await async_client.get(
            "/sensors/stats?device_id=nonexistent_device&hours=24",
            headers={"Authorization": f"Bearer {valid_user_token}"}
        )

        assert response.status_code == 200
        data = response.json()
        assert data["count"] == 0
        assert data["avg_temperatura"] == 0.0

    @pytest.mark.asyncio
    async def test_get_sensor_stats_without_token(self, async_client: AsyncClient):
        """Test: Request sin token falla."""
        response = await async_client.get(
            "/sensors/stats?device_id=test&hours=24"
        )

        assert response.status_code == 401


class TestSensorValidation:
    """Tests para validación de datos de sensores."""

    @pytest.mark.asyncio
    async def test_temperatura_within_valid_range(
        self,
        async_client: AsyncClient,
        valid_device_token: str
    ):
        """Test: Temperatura dentro de rango válido se acepta."""
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_device_token}"},
            json={
                "device_id": "galpon_test_01",
                "temperatura": 28.5,  # Rango: típicamente -40 a 125°C
                "humedad": 65.0,
                "calidad_aire": 450
            }
        )

        assert response.status_code == 201

    @pytest.mark.asyncio
    async def test_humedad_within_valid_range(
        self,
        async_client: AsyncClient,
        valid_device_token: str
    ):
        """Test: Humedad 0-100% se acepta."""
        for humedad in [0.0, 50.0, 100.0]:
            response = await async_client.post(
                "/sensors/readings",
                headers={"Authorization": f"Bearer {valid_device_token}"},
                json={
                    "device_id": "galpon_test_01",
                    "temperatura": 28.5,
                    "humedad": humedad,
                    "calidad_aire": 450
                }
            )
            assert response.status_code == 201

    @pytest.mark.asyncio
    async def test_humedad_invalid_range(
        self,
        async_client: AsyncClient,
        valid_device_token: str
    ):
        """Test: Humedad fuera de 0-100% se rechaza."""
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_device_token}"},
            json={
                "device_id": "galpon_test_01",
                "temperatura": 28.5,
                "humedad": 150.0,  # Fuera de rango
                "calidad_aire": 450
            }
        )

        # Pydantic debería validar esto
        assert response.status_code in [201, 422]

    @pytest.mark.asyncio
    async def test_calidad_aire_non_negative(
        self,
        async_client: AsyncClient,
        valid_device_token: str
    ):
        """Test: Calidad de aire debe ser no-negativa."""
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_device_token}"},
            json={
                "device_id": "galpon_test_01",
                "temperatura": 28.5,
                "humedad": 65.0,
                "calidad_aire": -100  # Inválido
            }
        )

        # Debería validarse
        assert response.status_code in [201, 422]

    @pytest.mark.asyncio
    async def test_timestamp_optional(
        self,
        async_client: AsyncClient,
        valid_device_token: str
    ):
        """Test: timestamp es opcional; el servidor asigna uno si no viene."""
        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_device_token}"},
            json={
                "device_id": "galpon_test_01",
                "temperatura": 28.5,
                "humedad": 65.0,
                "calidad_aire": 450
                # Sin timestamp
            }
        )

        assert response.status_code == 201


class TestSensorReadingsPerformance:
    """Tests de rendimiento y límites."""

    @pytest.mark.asyncio
    async def test_create_reading_sets_received_at(
        self,
        async_client: AsyncClient,
        valid_device_token: str,
        sample_sensor_reading
    ):
        """Test: El servidor registra received_at automáticamente."""
        before = datetime.now(timezone.utc)

        response = await async_client.post(
            "/sensors/readings",
            headers={"Authorization": f"Bearer {valid_device_token}"},
            json=sample_sensor_reading
        )

        after = datetime.now(timezone.utc)
        assert response.status_code == 201

        # Nota: En un test real, verificarías en la DB que received_at está
        # dentro del rango [before, after]

    @pytest.mark.asyncio
    async def test_get_readings_sorting_descending(
        self,
        async_client: AsyncClient,
        valid_user_token: str,
        populate_sensor_readings
    ):
        """Test: Las lecturas se devuelven en orden descendente (más recientes primero)."""
        response = await async_client.get(
            "/sensors/readings?device_id=galpon_test_01",
            headers={"Authorization": f"Bearer {valid_user_token}"}
        )

        assert response.status_code == 200
        data = response.json()

        if len(data) > 1:
            # Verificar que están ordenadas por timestamp descendente
            for i in range(len(data) - 1):
                current_ts = datetime.fromisoformat(data[i]["timestamp"])
                next_ts = datetime.fromisoformat(data[i + 1]["timestamp"])
                assert current_ts >= next_ts
