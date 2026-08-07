"""
=============================================================================
Router de sensores.

Endpoints:
  POST /sensors/readings      → ESP32 envía lecturas (protegido con device token)
  GET  /sensors/readings      → Apps consultan últimas lecturas (protegido con user token)
  GET  /sensors/readings/history → Historial por rango de tiempo
  GET  /sensors/stats         → Estadísticas agregadas
=============================================================================
"""

from fastapi import APIRouter, Depends, Query, status
from typing import List, Optional
from app.models.sensor import SensorReadingCreate, SensorReadingResponse, SensorStats
from app.services.sensor_service import SensorService
from app.services.auth_service import get_current_device, get_current_user
from app.database import get_database
from app.utils.logger import get_logger

logger = get_logger(__name__)
router = APIRouter(prefix="/sensors", tags=["Sensores"])


def get_sensor_service(db=Depends(get_database)) -> SensorService:
    return SensorService(db)


@router.post(
    "/readings",
    response_model=dict,
    status_code=status.HTTP_201_CREATED,
    summary="Recibir lectura de sensores (ESP32)",
    description="El ESP32 envía los datos de los sensores cada 5 segundos."
)
async def create_sensor_reading(
    reading: SensorReadingCreate,
    device_id: str = Depends(get_current_device),
    service: SensorService = Depends(get_sensor_service)
):
    """Recibe y almacena una lectura de sensores del ESP32.

    El token JWT del dispositivo se valida automáticamente.
    Si el device_id del token no coincide con el del body, se rechaza
    (medida de seguridad contra spoofing).
    """
    # Seguridad: el token dice quién es el dispositivo. No confiamos ciegamente en el body.
    if reading.device_id != device_id:
        logger.warning(
            "Spoofing detectado: device_id del body no coincide con el token",
            token_device=device_id,
            body_device=reading.device_id,
        )
        reading.device_id = device_id  # Sobrescribimos con el valor autenticado

    reading_id = await service.create_reading(reading)

    return {
        "success": True,
        "id": reading_id,
        "device_id": device_id,
        "message": "Lectura almacenada correctamente"
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
