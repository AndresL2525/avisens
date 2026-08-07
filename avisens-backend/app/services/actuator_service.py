"""
=============================================================================
Servicio de gestión de actuadores.

Mantiene el estado deseado de cada actuador. El ESP32 consulta
periódicamente este estado y lo aplica.
=============================================================================
"""

from datetime import datetime, timezone
from typing import List, Optional
from motor.motor_asyncio import AsyncIOMotorDatabase

from app.models.actuator import ActuatorState, ActuatorCommand, ActuatorCommandResponse
from app.utils.logger import get_logger
from app.utils.exceptions import DatabaseConnectionException

logger = get_logger(__name__)

COLLECTION_NAME = "actuator_commands"
STATE_COLLECTION = "actuator_states"


class ActuatorService:
    """Servicio para comandos y estados de actuadores."""

    def __init__(self, db: AsyncIOMotorDatabase):
        self.commands = db[COLLECTION_NAME]
        self.states = db[STATE_COLLECTION]

    async def send_command(self, command: ActuatorCommand) -> ActuatorCommandResponse:
        """Registra un comando para que el ESP32 lo lea.

        No controla el hardware directamente; solo almacena la ORDEN.
        El ESP32 la consulta vía GET y la ejecuta.
        """
        try:
            doc = command.model_dump()
            doc["queued_at"] = datetime.now(timezone.utc)
            doc["executed"] = False  # El ESP32 marcará como True cuando lo aplique

            result = await self.commands.insert_one(doc)

            # Actualizamos también el estado "deseable" para consultas rápidas
            await self.states.update_one(
                {"device_id": command.device_id, "nombre": command.nombre},
                {
                    "$set": {
                        "modo": command.modo,
                        "orden_manual": command.orden_manual,
                        "updated_at": datetime.now(timezone.utc),
                    }
                },
                upsert=True  # Crea si no existe
            )

            logger.info(
                "Comando de actuador registrado",
                device_id=command.device_id,
                actuator=command.nombre,
                modo=command.modo,
            )

            return ActuatorCommandResponse(
                success=True,
                device_id=command.device_id,
                nombre=command.nombre,
                modo=command.modo,
                message=f"Comando {command.nombre} → {command.modo} registrado correctamente",
                queued_at=doc["queued_at"]
            )
        except Exception as e:
            logger.error("Error registrando comando", error=str(e))
            raise DatabaseConnectionException("No se pudo registrar el comando")

    async def get_pending_commands(self, device_id: str) -> List[dict]:
        """Obtiene comandos pendientes para un dispositivo (lo llama el ESP32).

        El ESP32 debe confirmar ejecución vía otro endpoint.
        """
        query = {"device_id": device_id, "executed": False}
        cursor = self.commands.find(query).sort("queued_at", -1)

        commands = []
        async for doc in cursor:
            doc["_id"] = str(doc["_id"])
            commands.append(doc)

        return commands

    async def get_actuator_state(self, device_id: str, nombre: str) -> Optional[ActuatorState]:
        """Obtiene el estado actual de un actuador específico."""
        doc = await self.states.find_one({"device_id": device_id, "nombre": nombre})
        if doc:
            doc["_id"] = str(doc["_id"])
            return ActuatorState(**doc)
        return None

    async def report_actuator_state(self, state: ActuatorState) -> None:
        """El ESP32 reporta el estado REAL de los actuadores.

        Esto permite a las apps ver si el comando fue realmente aplicado.
        """
        await self.states.update_one(
            {"device_id": state.device_id, "nombre": state.nombre},
            {"$set": state.model_dump()},
            upsert=True
        )
        logger.debug("Estado de actuador actualizado", device_id=state.device_id, nombre=state.nombre)

    async def mark_command_executed(self, command_id: str) -> bool:
        """Marca un comando como ejecutado por el ESP32."""
        from bson.objectid import ObjectId
        result = await self.commands.update_one(
            {"_id": ObjectId(command_id)},
            {"$set": {"executed": True, "executed_at": datetime.now(timezone.utc)}}
        )
        return result.modified_count > 0
