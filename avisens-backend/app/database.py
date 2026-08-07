"""
=============================================================================
Conexión asíncrona a MongoDB usando Motor.
Motor es el driver async oficial de MongoDB para Python.
=============================================================================
"""

from motor.motor_asyncio import AsyncIOMotorClient
from pymongo.errors import ServerSelectionTimeoutError
from app.config import settings
from app.utils.logger import get_logger

logger = get_logger(__name__)

# Cliente MongoDB (se crea una sola vez al importar este módulo)
# Motor maneja el pool de conexiones automáticamente.
client: AsyncIOMotorClient | None = None


def get_database():
    """Retorna la instancia de la base de datos.

    En FastAPI se inyecta como dependencia en los endpoints.
    """
    if client is None:
        raise RuntimeError("La conexión a MongoDB no ha sido inicializada. Llama a connect_db() primero.")
    return client[settings.mongodb_db_name]


async def connect_db():
    """Inicializa la conexión a MongoDB.

    Se llama en el evento 'startup' de FastAPI.
    """
    global client
    try:
        client = AsyncIOMotorClient(
            settings.mongodb_uri,
            serverSelectionTimeoutMS=5000,  # Timeout de 5s para detectar fallos rápido
            maxPoolSize=50,                 # Máximo 50 conexiones simultáneas
            minPoolSize=5,                  # Mantiene 5 conexiones calientes
        )
        # Ping para verificar que la conexión funciona
        await client.admin.command("ping")
        logger.info("✅ Conexión a MongoDB establecida correctamente", db=settings.mongodb_db_name)
    except ServerSelectionTimeoutError as e:
        logger.error("❌ No se pudo conectar a MongoDB", error=str(e))
        raise


async def close_db():
    """Cierra la conexión a MongoDB.

    Se llama en el evento 'shutdown' de FastAPI.
    """
    global client
    if client:
        client.close()
        client = None
        logger.info("🔌 Conexión a MongoDB cerrada")


# Colecciones (se acceden vía get_database() en los servicios)
# - sensor_readings: lecturas de los sensores del ESP32
# - actuator_states: estado actual de los actuadores
# - events: eventos generados por el sistema (cambios, alarmas, fallas)
# - users: usuarios de las apps (si decides manejar usuarios aquí también)
