"""
=============================================================================
Router de actuadores.

Endpoints:
  POST /actuators/commands    → App envía comando (user token)
  GET  /actuators/commands    → ESP32 consulta comandos pendientes (device token)
  POST /actuators/state       → ESP32 reporta estado real (device token)
  GET  /actuators/state       → App consulta estado actual (user token)
=============================================================================
"""

from fastapi import APIRouter, Depends, Query, status
from typing import List, Optional
from app.models.actuator import ActuatorCommand, ActuatorCommandResponse, ActuatorState
from app.services.actuator_service import ActuatorService
from app.services.auth_service import get_current_device, get_current_user
from app.database import get_database
from app.utils.logger import get_logger

logger = get_logger(__name__)
router = APIRouter(prefix="/actuators", tags=["Actuadores"])


def get_actuator_service(db=Depends(get_database)) -> ActuatorService:
    return ActuatorService(db)


@router.post(
    "/commands",
    response_model=ActuatorCommandResponse,
    status_code=status.HTTP_201_CREATED,
    summary="Enviar comando a actuador",
    description="La app móvil/web envía una orden que el ESP32 ejecutará."
)
async def send_actuator_command(
    command: ActuatorCommand,
    user_id: str = Depends(get_current_user),
    service: ActuatorService = Depends(get_actuator_service)
):
    """Registra un comando para que el ESP32 lo lea y ejecute.

    El ESP32 consulta periódicamente los comandos pendientes vía GET.
    """
    logger.info(
        "Comando enviado desde app",
        user=user_id,
        device=command.device_id,
        actuator=command.nombre,
        modo=command.modo,
    )
    return await service.send_command(command)


@router.get(
    "/commands",
    response_model=List[dict],
    summary="Consultar comandos pendientes (ESP32)",
    description="El ESP32 llama este endpoint cada 10 segundos para saber si hay órdenes nuevas."
)
async def get_pending_commands(
    device_id: str = Depends(get_current_device),
    service: ActuatorService = Depends(get_actuator_service)
):
    """Devuelve comandos no ejecutados para este dispositivo.

    El ESP32 debe marcar como ejecutados vía POST /actuators/commands/{id}/executed.
    """
    commands = await service.get_pending_commands(device_id)
    logger.debug("Comandos pendientes consultados", device_id=device_id, count=len(commands))
    return commands


@router.post(
    "/commands/{command_id}/executed",
    response_model=dict,
    summary="Confirmar ejecución de comando (ESP32)",
    description="El ESP32 confirma que ya aplicó el comando."
)
async def mark_command_executed(
    command_id: str,
    device_id: str = Depends(get_current_device),
    service: ActuatorService = Depends(get_actuator_service)
):
    """El ESP32 marca un comando como ejecutado."""
    success = await service.mark_command_executed(command_id)
    return {
        "success": success,
        "command_id": command_id,
        "message": "Comando marcado como ejecutado" if success else "Comando no encontrado"
    }


@router.post(
    "/state",
    response_model=dict,
    status_code=status.HTTP_200_OK,
    summary="Reportar estado real (ESP32)",
    description="El ESP32 reporta el estado REAL de los actuadores después de aplicarlos."
)
async def report_actuator_state(
    state: ActuatorState,
    device_id: str = Depends(get_current_device),
    service: ActuatorService = Depends(get_actuator_service)
):
    """El ESP32 reporta el estado actual de sus actuadores.

    Esto permite a las apps saber si el comando fue realmente aplicado.
    """
    # Seguridad: validamos que el device_id del token coincida
    if state.device_id != device_id:
        state.device_id = device_id

    await service.report_actuator_state(state)
    return {
        "success": True,
        "message": "Estado de actuadores actualizado"
    }


@router.get(
    "/state",
    response_model=Optional[ActuatorState],
    summary="Consultar estado de actuador",
    description="La app consulta el estado actual de un actuador específico."
)
async def get_actuator_state(
    device_id: str = Query(..., description="ID del dispositivo"),
    nombre: str = Query(..., description="Nombre del actuador"),
    user_id: str = Depends(get_current_user),
    service: ActuatorService = Depends(get_actuator_service)
):
    """Devuelve el último estado reportado por el ESP32."""
    return await service.get_actuator_state(device_id, nombre)
