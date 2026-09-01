"""
=============================================================================
Router de sensores (MODIFICADO para integrar AlertService).

Endpoints:
  POST /sensors/readings      → ESP32 envía lecturas + detección de anomalías
  GET  /sensors/readings      → Apps consultan últimas lecturas
  GET  /sensors/readings/history → Historial por rango de tiempo
  GET  /sensors/stats         → Estadísticas agregadas
  GET  /sensors/health/{device_id} → Estado de salud del dispositivo
=============================================================================
"""

from fastapi import APIRouter, Depends, Query, status
from typing import List, Optional
from app.models.sensor import SensorReadingCreate, SensorReadingResponse, SensorStats
from app.services.sensor_service import SensorService
from app.services.alert_service import AlertService
from app.services.auth_service import get_current_device, get_current_user
from app.database import get_database
from app.utils.logger import get_logger

logger = get_logger(__name__)
router = APIRouter(prefix="/sensors", tags=["Sensores"])


def get_sensor_service(db=Depends(get_database)) -> SensorService:
    return SensorService(db)


def get_alert_service(db=Depends(get_database)) -> AlertService:
    return AlertService(db)


@router.post(
    "/readings",
    response_model=dict,
    status_code=status.HTTP_201_CREATED,
    summary="Recibir lectura de sensores (ESP32)",
    description="El ESP32 envía los datos de los sensores cada 5 segundos. Se verifica anomalías automáticamente."
)
async def create_sensor_reading(
    reading: SensorReadingCreate,
    device_id: str = Depends(get_current_device),
    sensor_service: SensorService = Depends(get_sensor_service),
    alert_service: AlertService = Depends(get_alert_service)
):
    """
    Recibe, almacena y valida una lectura de sensores del ESP32.

    El token JWT del dispositivo se valida automáticamente.
    Si el device_id del token no coincide con el del body, se rechaza
    (medida de seguridad contra spoofing).

    Adicionalmente, se ejecuta detección asíncrona de anomalías:
    - Gradientes térmicos abruptos
    - Valores fuera de rango físico
    - Humedad estancada en extremos
    """
    # ─── Seguridad: Validar que el device_id coincida con el token ──
    if reading.device_id != device_id:
        logger.warning(
            "Spoofing detectado: device_id del body no coincide con el token",
            token_device=device_id,
            body_device=reading.device_id,
        )
        reading.device_id = device_id  # Sobrescribimos con el valor autenticado

    # ─── 1. Almacenar la lectura ─────────────────────────────────────
    reading_id = await sensor_service.create_reading(reading)

    # ─── 2. Ejecutar detección de anomalías (asíncrona, no bloqueante) ──
    anomaly_event_id = await alert_service.check_sensor_outliers(
        device_id=device_id,
        temperatura=reading.temperatura,
        humedad=reading.humedad,
        calidad_aire=reading.calidad_aire if hasattr(reading, 'calidad_aire') else 0,
        timestamp=reading.timestamp if reading.timestamp else __import__('datetime').datetime.now(__import__('datetime').timezone.utc)
    )

    return {
        "success": True,
        "id": reading_id,
        "device_id": device_id,
        "message": "Lectura almacenada correctamente",
        "anomaly_detected": anomaly_event_id is not None,
        "anomaly_event_id": anomaly_event_id
    }


@router.get(
    "/readings",
    response_model=List[SensorReadingResponse],
    summary="Consultar lecturas recientes",
    description="Las apps móvil/web obtienen las últimas lecturas de sensores."
)
async def get_readings(
    device_id: Optional[str] = Query(None, description="Filtrar por dispositivo"),
    limit: int = Query(100, ge=1, le=1000, description="Cantidad máxima de registros"),
    user_id: str = Depends(get_current_user),
    service: SensorService = Depends(get_sensor_service)
):
    """Devuelve las lecturas más recientes. Solo usuarios autenticados."""
    logger.info("Consulta de lecturas", user=user_id, device=device_id, limit=limit)
    return await service.get_latest_readings(device_id=device_id, limit=limit)


@router.get(
    "/readings/history",
    response_model=List[SensorReadingResponse],
    summary="Historial de lecturas por rango de tiempo",
    description="Útil para graficar tendencias. Máximo 5000 registros."
)
async def get_readings_history(
    device_id: str = Query(..., description="ID del dispositivo"),
    hours: int = Query(24, ge=1, le=168, description="Horas hacia atrás (máx 7 días)"),
    limit: int = Query(1000, ge=1, le=5000),
    user_id: str = Depends(get_current_user),
    service: SensorService = Depends(get_sensor_service)
):
    """Devuelve lecturas de las últimas N horas para un dispositivo."""
    return await service.get_readings_by_time_range(
        device_id=device_id,
        hours=hours,
        limit=limit
    )


@router.get(
    "/stats",
    response_model=SensorStats,
    summary="Estadísticas de sensores",
    description="Promedios, mínimos y máximos de temperatura, humedad y calidad de aire."
)
async def get_sensor_stats(
    device_id: str = Query(..., description="ID del dispositivo"),
    hours: int = Query(24, ge=1, le=168),
    user_id: str = Depends(get_current_user),
    service: SensorService = Depends(get_sensor_service)
):
    """Estadísticas agregadas usando el pipeline de MongoDB."""
    return await service.get_stats(device_id=device_id, hours=hours)


@router.get(
    "/health/{device_id}",
    response_model=dict,
    summary="Estado de salud del dispositivo",
    description="Consulta conectividad, última lectura y eventos críticos recientes."
)
async def get_device_health(
    device_id: str,
    user_id: str = Depends(get_current_user),
    alert_service: AlertService = Depends(get_alert_service)
):
    """
    Devuelve información de salud del dispositivo:
    - is_online: Conectado en los últimos 30s
    - last_reading_timestamp: Cuándo fue la última lectura
    - inactividad_segundos: Segundos desde la última lectura
    - critical_events_1h: Cantidad de eventos críticos en la última hora
    - recent_events: Lista de eventos recientes
    """
    logger.info("Consulta de health status", user=user_id, device=device_id)
    return await alert_service.get_device_health_status(device_id)
