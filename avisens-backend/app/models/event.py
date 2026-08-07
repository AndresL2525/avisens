"""
=============================================================================
Modelos para eventos del sistema (logs estructurados).
=============================================================================
"""

from pydantic import BaseModel, Field, field_validator
from datetime import datetime
from typing import Literal
from bson import ObjectId


class EventCreate(BaseModel):
    device_id: str = Field(...)
    tipo: Literal["ACTUADOR", "FALLA_SENSOR", "SISTEMA", "ALERTA", "INFO"] = Field(...)
    origen: str = Field(..., max_length=100)
    mensaje: str = Field(..., max_length=500)
    nivel: Literal["info", "advertencia", "alerta", "critico"] = Field(default="info")
    timestamp: datetime | None = Field(default=None)
    metadata: dict = Field(default_factory=dict)

    @field_validator("mensaje")
    @classmethod
    def sanitize_mensaje(cls, v: str) -> str:
        v = v.strip()
        if len(v) > 500:
            v = v[:500]
        return v


class EventResponse(BaseModel):
    id: str = Field(..., alias="_id")
    device_id: str
    tipo: str
    origen: str
    mensaje: str
    nivel: str
    timestamp: datetime
    received_at: datetime
    metadata: dict

    class Config:
        populate_by_name = True
        json_encoders = {ObjectId: str, datetime: lambda v: v.isoformat()}
