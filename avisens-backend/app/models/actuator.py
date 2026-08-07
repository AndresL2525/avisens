from pydantic import BaseModel, Field, field_validator
from datetime import datetime
from typing import Literal
from bson import ObjectId


class ActuatorState(BaseModel):
    device_id: str = Field(...)
    nombre: Literal["calefactor", "extractor", "humidificador", "alimentador"] = Field(
        ...
    )
    estado: bool = Field(...)
    modo: Literal["AUTO", "MANUAL"] = Field(default="AUTO")
    orden_manual: bool | None = Field(default=None)
    updated_at: datetime = Field(default_factory=datetime.utcnow)

    class Config:
        json_encoders = {ObjectId: str, datetime: lambda v: v.isoformat()}


class ActuatorCommand(BaseModel):
    device_id: str = Field(...)
    nombre: Literal["calefactor", "extractor", "humidificador", "alimentador"] = Field(
        ...
    )
    modo: Literal["AUTO", "MANUAL"] = Field(...)
    orden_manual: bool | None = Field(default=None)

    @field_validator("orden_manual")
    @classmethod
    def validate_orden_manual(cls, v, info):
        values = info.data
        if values.get("modo") == "MANUAL" and v is None:
            raise ValueError("orden_manual es obligatorio cuando modo=MANUAL")
        return v


class ActuatorCommandResponse(BaseModel):
    success: bool
    device_id: str
    nombre: str
    modo: str
    message: str
    queued_at: datetime
