"""
=============================================================================
Servicio de gestión de lecturas de sensores.
=============================================================================
"""

from datetime import datetime, timedelta, timezone
from typing import List, Optional
from bson import ObjectId
from motor.motor_asyncio import AsyncIOMotorDatabase

from app.models.sensor import SensorReadingCreate, SensorReadingResponse, SensorStats
from app.utils.logger import get_logger
from app.utils.exceptions import DatabaseConnectionException

logger = get_logger(__name__)

COLLECTION_NAME = "sensor_readings"


class SensorService:
    """Servicio CRUD para lecturas de sensores."""

    def __init__(self, db: AsyncIOMotorDatabase):
        self.collection = db[COLLECTION_NAME]

    async def create_reading(self, reading: SensorReadingCreate) -> str:
        """Guarda una nueva lectura de sensor en MongoDB.

        Args:
            reading: Datos validados del ESP32.

        Returns:
            ID del documento insertado.
        """
        try:
            doc = reading.model_dump()
            # Si el dispositivo no envió timestamp, usamos el del servidor
            if doc.get("timestamp") is None:
                doc["timestamp"] = datetime.now(timezone.utc)

            doc["received_at"] = datetime.now(timezone.utc)

            result = await self.collection.insert_one(doc)
            logger.info(
                "Lectura de sensor almacenada",
                device_id=reading.device_id,
                reading_id=str(result.inserted_id),
                temp=reading.temperatura,
                hum=reading.humedad,
            )
            return str(result.inserted_id)
        except Exception as e:
            logger.error("Error almacenando lectura", error=str(e), device_id=reading.device_id)
            raise DatabaseConnectionException("No se pudo guardar la lectura del sensor")

    async def get_latest_readings(
        self,
        device_id: Optional[str] = None,
        limit: int = 100
    ) -> List[SensorReadingResponse]:
        """Obtiene las lecturas más recientes.

        Args:
            device_id: Filtrar por dispositivo específico.
            limit: Máximo de registros (default 100, max 1000).
        """
        limit = min(limit, 1000)  # Protección contra queries masivos

        query = {}
        if device_id:
            query["device_id"] = device_id

        cursor = self.collection.find(query).sort("timestamp", -1).limit(limit)
        readings = []
        async for doc in cursor:
            doc["_id"] = str(doc["_id"])
            readings.append(SensorReadingResponse(**doc))

        return readings

    async def get_readings_by_time_range(
        self,
        device_id: str,
        hours: int = 24,
        limit: int = 1000
    ) -> List[SensorReadingResponse]:
        """Obtiene lecturas de las últimas N horas.

        Útil para mostrar gráficas de tendencia en la app.
        """
        since = datetime.now(timezone.utc) - timedelta(hours=hours)

        query = {
            "device_id": device_id,
            "timestamp": {"$gte": since}
        }

        cursor = self.collection.find(query).sort("timestamp", -1).limit(min(limit, 5000))
        readings = []
        async for doc in cursor:
            doc["_id"] = str(doc["_id"])
            readings.append(SensorReadingResponse(**doc))

        logger.info(
            "Consulta de lecturas por rango",
            device_id=device_id,
            hours=hours,
            count=len(readings),
        )
        return readings

    async def get_stats(self, device_id: str, hours: int = 24) -> SensorStats:
        """Calcula estadísticas agregadas (promedios, mínimos, máximos).

        Usa el pipeline de agregación de MongoDB para eficiencia.
        """
        since = datetime.now(timezone.utc) - timedelta(hours=hours)

        pipeline = [
            {"$match": {"device_id": device_id, "timestamp": {"$gte": since}}},
            {
                "$group": {
                    "_id": "$device_id",
                    "count": {"$sum": 1},
                    "avg_temperatura": {"$avg": "$temperatura"},
                    "min_temperatura": {"$min": "$temperatura"},
                    "max_temperatura": {"$max": "$temperatura"},
                    "avg_humedad": {"$avg": "$humedad"},
                    "min_humedad": {"$min": "$humedad"},
                    "max_humedad": {"$max": "$humedad"},
                    "avg_calidad_aire": {"$avg": "$calidad_aire"},
                }
            }
        ]

        result = await self.collection.aggregate(pipeline).to_list(length=1)

        if not result:
            return SensorStats(
                device_id=device_id,
                count=0,
                avg_temperatura=0.0,
                min_temperatura=0.0,
                max_temperatura=0.0,
                avg_humedad=0.0,
                min_humedad=0.0,
                max_humedad=0.0,
                avg_calidad_aire=0.0,
                periodo=f"{hours}h"
            )

        data = result[0]
        return SensorStats(
            device_id=device_id,
            count=data.get("count", 0),
            avg_temperatura=round(data.get("avg_temperatura", 0), 2),
            min_temperatura=round(data.get("min_temperatura", 0), 2),
            max_temperatura=round(data.get("max_temperatura", 0), 2),
            avg_humedad=round(data.get("avg_humedad", 0), 2),
            min_humedad=round(data.get("min_humedad", 0), 2),
            max_humedad=round(data.get("max_humedad", 0), 2),
            avg_calidad_aire=round(data.get("avg_calidad_aire", 0), 2),
            periodo=f"{hours}h"
        )
