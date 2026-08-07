from pydantic import BaseModel, Field, field_validator
from datetime import datetime
from typing import Literal


class Token(BaseModel):
    access_token: str
    token_type: str = Field(default="bearer")
    expires_at: datetime | None = None
    token_kind: Literal["user", "device"] = Field(default="user")


class DeviceAuthRequest(BaseModel):
    device_id: str = Field(..., min_length=1, max_length=50)
    device_secret: str = Field(..., min_length=8, max_length=256)

    @field_validator("device_id")
    @classmethod
    def validate_device_id(cls, v: str) -> str:
        if not v.replace("_", "").replace("-", "").isalnum():
            raise ValueError("device_id invalido")
        return v.strip().lower()


class UserAuthRequest(BaseModel):
    username: str = Field(..., min_length=3, max_length=50)
    password: str = Field(..., min_length=8, max_length=128)


class TokenPayload(BaseModel):
    sub: str = Field(...)
    exp: datetime
    iat: datetime
    type: Literal["user", "device"]
    jti: str = Field(...)
