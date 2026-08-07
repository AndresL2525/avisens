"""
=============================================================================
Configuración centralizada del backend AVÍSENS.
=============================================================================
"""

from pydantic_settings import BaseSettings, SettingsConfigDict
from pydantic import Field, field_validator
from typing import List


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=".env", env_file_encoding="utf-8", case_sensitive=False, extra="ignore"
    )

    mongodb_uri: str = Field(...)
    mongodb_db_name: str = Field(default="avisens")

    jwt_secret_key: str = Field(..., min_length=32)
    jwt_algorithm: str = Field(default="HS256")
    jwt_access_token_expire_minutes: int = Field(default=30, ge=1)
    jwt_device_token_expire_days: int = Field(default=365, ge=1)

    api_host: str = Field(default="0.0.0.0")
    api_port: int = Field(default=8000, ge=1, le=65535)
    api_workers: int = Field(default=1, ge=1)
    environment: str = Field(
        default="development", pattern="^(development|staging|production)$"
    )

    rate_limit_per_minute: int = Field(default=60, ge=1)
    device_rate_limit_per_minute: int = Field(default=120, ge=1)

    # Estos se leen como str del .env y se convierten a lista via propiedades
    cors_origins: str = Field(default="*")
    authorized_device_ids: str = Field(default="")

    log_level: str = Field(
        default="INFO", pattern="^(DEBUG|INFO|WARNING|ERROR|CRITICAL)$"
    )
    log_format: str = Field(default="json", pattern="^(json|text)$")

    @property
    def cors_origins_list(self) -> List[str]:
        if self.cors_origins == "*":
            return ["*"]
        return [
            origin.strip() for origin in self.cors_origins.split(",") if origin.strip()
        ]

    @property
    def authorized_device_ids_list(self) -> List[str]:
        if not self.authorized_device_ids:
            return []
        return [
            device_id.strip()
            for device_id in self.authorized_device_ids.split(",")
            if device_id.strip()
        ]

    @property
    def is_production(self) -> bool:
        return self.environment == "production"


settings = Settings()
