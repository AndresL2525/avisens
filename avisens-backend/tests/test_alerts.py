"""
=============================================================================
Tests de alertas — Detección de outliers, heartbeat, notificaciones.
=============================================================================
"""

import pytest
from datetime import datetime, timezone, timedelta
from httpx import AsyncClient
from app.services.alert_service import AlertService


class TestOutlierDetection:
    """Tests para detección de anomalías en lecturas de sensores."""

    @pytest.mark.asyncio
    async def test_temperatura_out_of_range_high(self, mock_db):
        """Test: Temperatura > 60°C dispara alerta."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_test_01"
        temperatura = 75.5  # Fuera de rango (max 60°C)
        humedad = 65.0
        calidad_aire = 450
        timestamp = datetime.now(timezone.utc)

        event_id = await alert_service.check_sensor_outliers(
            device_id=device_id,
            temperatura=temperatura,
            humedad=humedad,
            calidad_aire=calidad_aire,
            timestamp=timestamp
        )

        assert event_id is not None, "Debería detectar temperatura fuera de rango"

        # Verificar que el evento se creó en la DB
        event = await mock_db["events"].find_one({"_id": __import__("bson").ObjectId(event_id)})
        assert event is not None
        assert event["tipo"] == "ALERTA"
        assert event["nivel"] == "critico"
        assert "temperatura" in event["metadata"]

    @pytest.mark.asyncio
    async def test_temperatura_out_of_range_low(self, mock_db):
        """Test: Temperatura < -10°C dispara alerta."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_test_01"
        temperatura = -20.0  # Fuera de rango (min -10°C)
        humedad = 65.0
        calidad_aire = 450
        timestamp = datetime.now(timezone.utc)

        event_id = await alert_service.check_sensor_outliers(
            device_id=device_id,
            temperatura=temperatura,
            humedad=humedad,
            calidad_aire=calidad_aire,
            timestamp=timestamp
        )

        assert event_id is not None, "Debería detectar temperatura fuera de rango bajo"

    @pytest.mark.asyncio
    async def test_temperatura_within_range(self, mock_db):
        """Test: Temperatura dentro de rango no dispara alerta (si no hay otros problemas)."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_test_01"
        temperatura = 28.5  # Dentro de rango
        humedad = 65.0
        calidad_aire = 450
        timestamp = datetime.now(timezone.utc)

        event_id = await alert_service.check_sensor_outliers(
            device_id=device_id,
            temperatura=temperatura,
            humedad=humedad,
            calidad_aire=calidad_aire,
            timestamp=timestamp
        )

        assert event_id is None, "No debería detectar anomalía para lecturas válidas"

    @pytest.mark.asyncio
    async def test_humedad_out_of_range_high(self, mock_db):
        """Test: Humedad > 100% dispara alerta."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_test_01"
        temperatura = 28.5
        humedad = 120.0  # Fuera de rango
        calidad_aire = 450
        timestamp = datetime.now(timezone.utc)

        event_id = await alert_service.check_sensor_outliers(
            device_id=device_id,
            temperatura=temperatura,
            humedad=humedad,
            calidad_aire=calidad_aire,
            timestamp=timestamp
        )

        assert event_id is not None, "Debería detectar humedad fuera de rango"

    @pytest.mark.asyncio
    async def test_humedad_stagnant_at_zero(self, mock_db):
        """Test: Humedad estancada en 0% (lectura previa también 0%) dispara alerta."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_test_01"

        # Insertar lectura anterior con humedad 0%
        await mock_db["sensor_readings"].insert_one({
            "device_id": device_id,
            "temperatura": 25.0,
            "humedad": 0.0,  # Igual a la próxima lectura
            "calidad_aire": 400,
            "timestamp": datetime.now(timezone.utc) - timedelta(seconds=2)
        })

        temperatura = 26.0
        humedad = 0.0  # Mismo valor que la anterior
        calidad_aire = 450
        timestamp = datetime.now(timezone.utc)

        event_id = await alert_service.check_sensor_outliers(
            device_id=device_id,
            temperatura=temperatura,
            humedad=humedad,
            calidad_aire=calidad_aire,
            timestamp=timestamp
        )

        assert event_id is not None, "Debería detectar humedad estancada en 0%"

    @pytest.mark.asyncio
    async def test_humedad_stagnant_at_hundred(self, mock_db):
        """Test: Humedad estancada en 100% dispara alerta."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_test_01"

        await mock_db["sensor_readings"].insert_one({
            "device_id": device_id,
            "temperatura": 25.0,
            "humedad": 100.0,
            "calidad_aire": 400,
            "timestamp": datetime.now(timezone.utc) - timedelta(seconds=2)
        })

        temperatura = 26.0
        humedad = 100.0
        calidad_aire = 450
        timestamp = datetime.now(timezone.utc)

        event_id = await alert_service.check_sensor_outliers(
            device_id=device_id,
            temperatura=temperatura,
            humedad=humedad,
            calidad_aire=calidad_aire,
            timestamp=timestamp
        )

        assert event_id is not None, "Debería detectar humedad estancada en 100%"


class TestThermalGradient:
    """Tests para detección de gradientes térmicos abruptos."""

    @pytest.mark.asyncio
    async def test_thermal_gradient_exceeds_10c_in_5s(self, mock_db):
        """Test: ΔT > 10°C en ≤ 5s dispara alerta."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_test_01"

        # Insertar lectura anterior
        timestamp_anterior = datetime.now(timezone.utc) - timedelta(seconds=3)
        await mock_db["sensor_readings"].insert_one({
            "device_id": device_id,
            "temperatura": 20.0,  # Temperatura previa
            "humedad": 65.0,
            "calidad_aire": 400,
            "timestamp": timestamp_anterior
        })

        # Nueva lectura con ΔT = 15°C en 3 segundos
        temperatura = 35.0  # ΔT = 15°C
        humedad = 65.0
        calidad_aire = 450
        timestamp = datetime.now(timezone.utc)

        event_id = await alert_service.check_sensor_outliers(
            device_id=device_id,
            temperatura=temperatura,
            humedad=humedad,
            calidad_aire=calidad_aire,
            timestamp=timestamp
        )

        assert event_id is not None, "Debería detectar gradiente térmico abrupto"

        # Verificar metadata del evento
        event = await mock_db["events"].find_one({"_id": __import__("bson").ObjectId(event_id)})
        assert "Gradiente" in event["mensaje"] or "gradiente" in event["mensaje"].lower()

    @pytest.mark.asyncio
    async def test_thermal_gradient_within_tolerance(self, mock_db):
        """Test: ΔT ≤ 10°C en 5s no dispara alerta."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_test_01"

        timestamp_anterior = datetime.now(timezone.utc) - timedelta(seconds=5)
        await mock_db["sensor_readings"].insert_one({
            "device_id": device_id,
            "temperatura": 25.0,
            "humedad": 65.0,
            "calidad_aire": 400,
            "timestamp": timestamp_anterior
        })

        # ΔT = 8°C (dentro de tolerancia)
        temperatura = 33.0
        humedad = 65.0
        calidad_aire = 450
        timestamp = datetime.now(timezone.utc)

        event_id = await alert_service.check_sensor_outliers(
            device_id=device_id,
            temperatura=temperatura,
            humedad=humedad,
            calidad_aire=calidad_aire,
            timestamp=timestamp
        )

        # Podría haber alerta si hay otros problemas, pero no por gradiente
        if event_id:
            event = await mock_db["events"].find_one({"_id": __import__("bson").ObjectId(event_id)})
            assert "Gradiente" not in event.get("mensaje", "")

    @pytest.mark.asyncio
    async def test_thermal_gradient_outside_5s_window(self, mock_db):
        """Test: ΔT > 10°C pero después de > 5s no dispara alerta por gradiente."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_test_01"

        # Lectura anterior hace 10 segundos
        timestamp_anterior = datetime.now(timezone.utc) - timedelta(seconds=10)
        await mock_db["sensor_readings"].insert_one({
            "device_id": device_id,
            "temperatura": 20.0,
            "humedad": 65.0,
            "calidad_aire": 400,
            "timestamp": timestamp_anterior
        })

        # ΔT = 15°C pero en 10 segundos (fuera de ventana de 5s)
        temperatura = 35.0
        humedad = 65.0
        calidad_aire = 450
        timestamp = datetime.now(timezone.utc)

        event_id = await alert_service.check_sensor_outliers(
            device_id=device_id,
            temperatura=temperatura,
            humedad=humedad,
            calidad_aire=calidad_aire,
            timestamp=timestamp
        )

        # No debería alertar por gradiente (fuera de ventana de tiempo)
        if event_id:
            event = await mock_db["events"].find_one({"_id": __import__("bson").ObjectId(event_id)})
            assert "Gradiente" not in event.get("mensaje", "")


class TestDeviceHeartbeat:
    """Tests para detección de dispositivos offline."""

    @pytest.mark.asyncio
    async def test_device_offline_after_timeout(self, mock_db):
        """Test: Dispositivo sin lecturas por > 30s se marca como offline."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_offline_01"

        # Insertar última lectura hace 35 segundos
        await mock_db["sensor_readings"].insert_one({
            "device_id": device_id,
            "temperatura": 28.0,
            "humedad": 65.0,
            "calidad_aire": 400,
            "timestamp": datetime.now(timezone.utc) - timedelta(seconds=35)
        })

        result = await alert_service.evaluate_device_heartbeat(timeout_seconds=30)

        assert device_id in result["offline_devices"]
        assert result["alerts_created"] >= 1

    @pytest.mark.asyncio
    async def test_device_online_within_timeout(self, mock_db):
        """Test: Dispositivo con lectura reciente (< 30s) no se marca como offline."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_online_01"

        # Insertar lectura hace 15 segundos
        await mock_db["sensor_readings"].insert_one({
            "device_id": device_id,
            "temperatura": 28.0,
            "humedad": 65.0,
            "calidad_aire": 400,
            "timestamp": datetime.now(timezone.utc) - timedelta(seconds=15)
        })

        result = await alert_service.evaluate_device_heartbeat(timeout_seconds=30)

        assert device_id not in result["offline_devices"]

    @pytest.mark.asyncio
    async def test_heartbeat_prevents_spam_alerts(self, mock_db):
        """Test: No se crean alertas duplicadas en menos de 1 hora."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_spam_test_01"

        # Insertar lectura antigua
        await mock_db["sensor_readings"].insert_one({
            "device_id": device_id,
            "temperatura": 28.0,
            "humedad": 65.0,
            "calidad_aire": 400,
            "timestamp": datetime.now(timezone.utc) - timedelta(seconds=35)
        })

        # Insertar alerta existente hace 30 minutos
        await mock_db["events"].insert_one({
            "device_id": device_id,
            "tipo": "SISTEMA",
            "origen": "AlertService_Heartbeat",
            "mensaje": "Dispositivo inactivo",
            "nivel": "critico",
            "timestamp": datetime.now(timezone.utc) - timedelta(minutes=30),
            "received_at": datetime.now(timezone.utc),
            "metadata": {}
        })

        result = await alert_service.evaluate_device_heartbeat(timeout_seconds=30)

        # Debería detectar offline pero no crear nueva alerta (existe reciente)
        assert device_id in result["offline_devices"]
        assert result["existing_alerts_skipped"] >= 1

    @pytest.mark.asyncio
    async def test_heartbeat_multiple_devices(self, mock_db):
        """Test: Heartbeat detecta múltiples dispositivos offline simultáneamente."""
        alert_service = AlertService(mock_db)

        # Insertar varios dispositivos con diferentes estados
        now = datetime.now(timezone.utc)

        for i in range(3):
            await mock_db["sensor_readings"].insert_one({
                "device_id": f"galpon_{i:02d}",
                "temperatura": 28.0,
                "humedad": 65.0,
                "calidad_aire": 400,
                "timestamp": now - timedelta(seconds=35 + i*10)  # Todos hace > 30s
            })

        result = await alert_service.evaluate_device_heartbeat(timeout_seconds=30)

        assert len(result["offline_devices"]) == 3


class TestCriticalNotification:
    """Tests para envío de notificaciones críticas."""

    @pytest.mark.asyncio
    async def test_dispatch_critical_notification_success(self, mock_db):
        """Test: Notificación crítica se despacha sin errores."""
        alert_service = AlertService(mock_db)

        event_id = "test_event_id_123"
        event_dict = {
            "device_id": "galpon_test_01",
            "tipo": "ALERTA",
            "nivel": "critico",
            "mensaje": "Alerta de anomalía crítica en sensores"
        }

        result = await alert_service.dispatch_critical_notification(event_id, event_dict)

        assert result is True, "Notificación debería despacharse exitosamente"

    @pytest.mark.asyncio
    async def test_dispatch_notification_with_missing_fields(self, mock_db):
        """Test: Notificación se despacha incluso con campos faltantes."""
        alert_service = AlertService(mock_db)

        event_id = "test_event_id_123"
        event_dict = {
            "device_id": "galpon_test_01"
            # Faltan tipo, nivel, mensaje
        }

        result = await alert_service.dispatch_critical_notification(event_id, event_dict)

        # Debería manejarse sin fallar
        assert isinstance(result, bool)


class TestDeviceHealthStatus:
    """Tests para obtener estado de salud de dispositivo."""

    @pytest.mark.asyncio
    async def test_device_health_online(self, mock_db):
        """Test: Dispositivo con lectura reciente se reporta como online."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_test_01"

        await mock_db["sensor_readings"].insert_one({
            "device_id": device_id,
            "temperatura": 28.0,
            "humedad": 65.0,
            "calidad_aire": 400,
            "timestamp": datetime.now(timezone.utc) - timedelta(seconds=5)
        })

        health = await alert_service.get_device_health_status(device_id)

        assert health["device_id"] == device_id
        assert health["is_online"] is True
        assert health["inactividad_segundos"] < 30

    @pytest.mark.asyncio
    async def test_device_health_offline(self, mock_db):
        """Test: Dispositivo inactivo > 30s se reporta como offline."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_offline_01"

        await mock_db["sensor_readings"].insert_one({
            "device_id": device_id,
            "temperatura": 28.0,
            "humedad": 65.0,
            "calidad_aire": 400,
            "timestamp": datetime.now(timezone.utc) - timedelta(seconds=60)
        })

        health = await alert_service.get_device_health_status(device_id)

        assert health["device_id"] == device_id
        assert health["is_online"] is False
        assert health["inactividad_segundos"] > 30

    @pytest.mark.asyncio
    async def test_device_health_no_readings(self, mock_db):
        """Test: Dispositivo sin lecturas se reporta como offline."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_never_connected_01"

        health = await alert_service.get_device_health_status(device_id)

        assert health["device_id"] == device_id
        assert health["is_online"] is False
        assert health["last_reading_timestamp"] is None

    @pytest.mark.asyncio
    async def test_device_health_with_recent_events(self, mock_db):
        """Test: Se incluyen eventos críticos recientes en health status."""
        alert_service = AlertService(mock_db)

        device_id = "galpon_test_01"

        # Lectura reciente
        await mock_db["sensor_readings"].insert_one({
            "device_id": device_id,
            "temperatura": 28.0,
            "humedad": 65.0,
            "calidad_aire": 400,
            "timestamp": datetime.now(timezone.utc) - timedelta(seconds=5)
        })

        # Evento crítico reciente
        await mock_db["events"].insert_one({
            "device_id": device_id,
            "tipo": "ALERTA",
            "nivel": "critico",
            "mensaje": "Alerta de temperatura alta",
            "timestamp": datetime.now(timezone.utc) - timedelta(minutes=15),
            "received_at": datetime.now(timezone.utc),
            "_id": __import__("bson").ObjectId()
        })

        health = await alert_service.get_device_health_status(device_id)

        assert health["critical_events_1h"] >= 1
        assert len(health["recent_events"]) >= 1
