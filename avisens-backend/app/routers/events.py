"""
=============================================================================
Router de eventos.

Endpoints:
  POST /events              → ESP32 o backend registra un evento
  GET  /events              → Apps consultan historial de eventos
=============================================================================
"""

from fastapi import APIRouter, Depends, Query, status
from typing import List, Optional
from datetime import datetime, timezone
from app.models.event import EventCreate, EventResponse
from app.services.auth_service import get_current_device, get_current_user
from app.database import get_database
from app.utils.logger import get_logger
from motor.motor_asyncio import AsyncIOMotorDatabase

logger = get_logger(__name__)
router = APIRouter(prefix="/events", tags=["Eventos"])

COLLECTION_NAME = "events"


def get_events_collection(db: AsyncIOMotorDatabase = Depends(get_database)):
    return db[COLLECTION_NAME]


@router.post(
    "",
    response_model=dict,
    status_code=status.HTTP_201_CREATED,
    summary="Registrar evento",
    description="El ESP32 o el backend registran eventos (cambios de actuador, fallas, alertas)."
)
async def create_event(
    event: EventCreate,
    device_id: str = Depends(get_current_device),
    collection = Depends(get_events_collection)
):
    """Almacena un evento en MongoDB.

    Los eventos son inmutables: una vez creados, no se modifican.
    """
    if event.device_id != device_id:
        event.device_id = device_id

    doc = event.model_dump()
    if doc.get("timestamp") is None:
        doc["timestamp"] = datetime.now(timezone.utc)
    doc["received_at"] = datetime.now(timezone.utc)

    result = await collection.insert_one(doc)

    logger.info(
        "Evento registrado",
        event_id=str(result.inserted_id),
        device_id=device_id,
        tipo=event.tipo,
        nivel=event.nivel,
    )

    return {
        "success": True,
        "id": str(result.inserted_id),
        "message": "Evento registrado correctamente"
    }


@router.get(
    "",
    response_model=List[EventResponse],
    summary="Consultar eventos",
    description="Historial de eventos con filtros por tipo, nivel y dispositivo."
)
async def get_events(
    device_id: Optional[str] = Query(None, description="Filtrar por dispositivo"),
    tipo: Optional[str] = Query(None, description="Filtrar por tipo: ACTUADOR, FALLA_SENSOR, SISTEMA, ALERTA, INFO"),
    nivel: Optional[str] = Query(None, description="Filtrar por nivel: info, advertencia, alerta, critico"),
    limit: int = Query(100, ge=1, le=1000),
    user_id: str = Depends(get_current_user),
    collection = Depends(get_events_collection)
):
    """Devuelve eventos filtrados. Útil para el panel de administración."""
    query = {}
    if device_id:
        query["device_id"] = device_id
    if tipo:
        query["tipo"] = tipo
    if nivel:
        query["nivel"] = nivel

    cursor = collection.find(query).sort("timestamp", -1).limit(limit)

    events = []
    async for doc in cursor:
        doc["_id"] = str(doc["_id"])
        events.append(EventResponse(**doc))

    logger.info("Consulta de eventos", user=user_id, filters=query, count=len(events))
    return events
