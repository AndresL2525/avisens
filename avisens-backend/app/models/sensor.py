from pydantic import BaseModel, Field, field_validator
from datetime import datetime
from typing import Literal
from bson import ObjectId


class SensorReadingCreate(BaseModel):
    device_id: str = Field(..., min_length=1, max_length=50, examples=["galpon_01"])
    temperatura: float = Field(..., ge=-10.0, le=60.0, examples=[24.5])
    humedad: float = Field(..., ge=0.0, le=100.0, examples=[65.0])
    peso: float = Field(..., ge=0.0, le=5000.0, examples=[125.3])
    obstaculo: bool = Field(default=False)
    calidad_aire: int = Field(..., ge=0, le=4095, examples=[1200])
    voltaje_aire: float = Field(default=0.0, ge=0.0, le=3.3, examples=[0.96])
    timestamp: datetime | None = Field(default=None)

    @field_validator("device_id")
    @classmethod
    def validate_device_id(cls, v: str) -> str:
        if not v.replace("_", "").replace("-", "").isalnum():
            raise ValueError(
                "device_id solo puede contener letras, numeros, guiones y guiones bajos"
            )
        return v.strip().lower()

    @field_validator("temperatura", "humedad", "peso", "calidad_aire", "voltaje_aire")
    @classmethod
    def validate_not_nan(cls, v):
        import math

        if isinstance(v, float) and (math.isnan(v) or math.isinf(v)):
            raise ValueError("Valor numerico invalido (NaN o Inf)")
        return v


class SensorReadingResponse(BaseModel):
    id: str = Field(..., alias="_id")
    device_id: str
    temperatura: float
    humedad: float
    peso: float
    obstaculo: bool
    calidad_aire: int
    voltaje_aire: float
    timestamp: datetime
    received_at: datetime

    class Config:
        populate_by_name = True
        json_encoders = {ObjectId: str, datetime: lambda v: v.isoformat()}


class SensorStats(BaseModel):
    device_id: str
    count: int
    avg_temperatura: float
    min_temperatura: float
    max_temperatura: float
    avg_humedad: float
    min_humedad: float
    max_humedad: float
    avg_calidad_aire: float
    periodo: str
