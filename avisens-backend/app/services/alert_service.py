"""
=============================================================================
Servicio de alertas — Detección de anomalías, heartbeat y notificaciones.
=============================================================================

Responsabilidades:
- check_sensor_outliers: Detecta gradientes térmicos abruptos e incoherencias
- evaluate_device_heartbeat: Monitorea desconexiones de dispositivos
- dispatch_critical_notification: Envía notificaciones de eventos críticos
"""

from datetime import datetime, timedelta, timezone
from typing import Optional, Dict, Any
from motor.motor_asyncio import AsyncIOMotorDatabase
from app.models.event import EventCreate
from app.utils.logger import get_logger
from app.utils.exceptions import DatabaseConnectionException

logger = get_logger(__name__)

EVENTS_COLLECTION = "events"
SENSOR_READINGS_COLLECTION = "sensor_readings"


class AlertService:
    """Servicio de detección de anomalías y gestión de alertas."""

    def __init__(self, db: AsyncIOMotorDatabase):
        self.db = db
        self.events_collection = db[EVENTS_COLLECTION]
        self.readings_collection = db[SENSOR_READINGS_COLLECTION]

    async def check_sensor_outliers(
        self,
        device_id: str,
        temperatura: float,
        humedad: float,
        calidad_aire: int,
        timestamp: datetime
    ) -> Optional[str]:
        """
        Detecta anomalías en lecturas de sensores:
        1. Gradientes térmicos abruptos (|ΔT| > 10.0°C en ≤ 5s)
        2. Valores físicamente imposibles
        3. Humedad estancada en extremos (0% o 100% sostenida)

        Args:
            device_id: ID del dispositivo
            temperatura: Temperatura en °C
            humedad: Humedad en %
            calidad_aire: Valor raw de MQ135
            timestamp: Timestamp de la lectura actual

        Returns:
            ID del evento de alerta si se detectó anomalía, None si todo está bien.
        """
        try:
            # ─── 1. Validar rangos físicos plausibles ────────────────────
            anomalias = []

            if temperatura < -10.0 or temperatura > 60.0:
                anomalias.append(
                    f"Temperatura fuera de rango plausible: {temperatura}°C "
                    f"(rango válido: -10°C a 60°C)"
                )

            if humedad < 0.0 or humedad > 100.0:
                anomalias.append(
                    f"Humedad fuera de rango: {humedad}% (debe estar entre 0-100%)"
                )

            # ─── 2. Detectar humedad estancada en extremos ────────────────
            if humedad == 0.0 or humedad == 100.0:
                # Verificar si existe lectura previa muy reciente con mismo valor
                ultima_lectura = await self.readings_collection.find_one(
                    {
                        "device_id": device_id,
                        "timestamp": {"$lt": timestamp}
                    },
                    sort=[("timestamp", -1)]
                )

                if ultima_lectura:
                    humedad_previa = ultima_lectura.get("humedad", humedad)
                    if humedad_previa == humedad:
                        anomalias.append(
                            f"Humedad estancada en valor extremo: {humedad}% "
                            f"(puede indicar sensor bloqueado o defectuoso)"
                        )

            # ─── 3. Detectar gradientes térmicos abruptos (|ΔT| > 10°C en 5s) ──
            ultima_lectura = await self.readings_collection.find_one(
                {
                    "device_id": device_id,
                    "timestamp": {"$lt": timestamp}
                },
                sort=[("timestamp", -1)]
            )

            if ultima_lectura:
                temp_previa = ultima_lectura.get("temperatura")
                timestamp_previa = ultima_lectura.get("timestamp")

                if temp_previa is not None and timestamp_previa is not None:
                    delta_temp = abs(temperatura - temp_previa)
                    delta_tiempo = (timestamp - timestamp_previa).total_seconds()

                    if delta_tiempo <= 5.0 and delta_temp > 10.0:
                        anomalias.append(
                            f"Gradiente térmico abrupto: ΔT={delta_temp:.1f}°C en {delta_tiempo:.1f}s "
                            f"(umbral: >10°C en ≤5s)"
                        )

            # ─── 4. Si hay anomalías, registrar evento de alerta ─────────
            if anomalias:
                evento = EventCreate(
                    device_id=device_id,
                    tipo="ALERTA",
                    origen="AlertService",
                    mensaje=" | ".join(anomalias)[:500],  # Limitar a 500 chars
                    nivel="advertencia" if len(anomalias) == 1 else "critico",
                    timestamp=timestamp,
                    metadata={
                        "temperatura": temperatura,
                        "humedad": humedad,
                        "calidad_aire": calidad_aire,
                        "anomalias_detectadas": len(anomalias),
                        "tipos": [
                            "Rango_fuera_limites" if any("rango" in a.lower() for a in anomalias) else None,
                            "Humedad_estancada" if any("estancada" in a.lower() for a in anomalias) else None,
                            "Gradiente_termico" if any("Gradiente" in a for a in anomalias) else None,
                        ]
                    }
                )

                doc = evento.model_dump()
                doc["received_at"] = datetime.now(timezone.utc)

                result = await self.events_collection.insert_one(doc)

                logger.warning(
                    "Anomalía detectada en sensores",
                    device_id=device_id,
                    event_id=str(result.inserted_id),
                    anomalias=len(anomalias),
                    temp=temperatura,
                    hum=humedad,
                )

                return str(result.inserted_id)

            return None

        except Exception as e:
            logger.error(
                "Error en check_sensor_outliers",
                error=str(e),
                device_id=device_id
            )
            raise DatabaseConnectionException(
                "No se pudo evaluar anomalías de sensores"
            )

    async def evaluate_device_heartbeat(
        self,
        timeout_seconds: int = 30
    ) -> Dict[str, Any]:
        """
        Monitorea dispositivos inactivos. Identifica galpones que no envían
        lecturas de telemetría en más de `timeout_seconds` segundos.

        Evita spam de alertas: Solo crea evento si no existe uno activo/reciente
        (< 1 hora) del mismo dispositivo con tipo SISTEMA.

        Args:
            timeout_seconds: Segundos de inactividad antes de alertar (default 30)

        Returns:
            Dict con información de dispositivos offline detectados:
            {
                "offline_devices": ["device_1", "device_2"],
                "alerts_created": 2,
                "existing_alerts_skipped": 1
            }
        """
        try:
            now = datetime.now(timezone.utc)
            cutoff_time = now - timedelta(seconds=timeout_seconds)

            # ─── 1. Obtener el último timestamp por device_id ──────────────
            pipeline = [
                {
                    "$group": {
                        "_id": "$device_id",
                        "last_timestamp": {"$max": "$timestamp"},
                        "count": {"$sum": 1}
                    }
                },
                {
                    "$sort": {"last_timestamp": -1}
                }
            ]

            offline_devices = []
            alerts_created = 0
            existing_alerts_skipped = 0

            async for device_status in self.readings_collection.aggregate(pipeline):
                device_id = device_status["_id"]
                last_seen = device_status.get("last_timestamp")

                if last_seen and (now - last_seen) > timedelta(seconds=timeout_seconds):
                    offline_devices.append(device_id)

                    # ─── 2. Verificar si ya existe alerta activa/reciente ──
                    one_hour_ago = now - timedelta(hours=1)

                    existing_alert = await self.events_collection.find_one(
                        {
                            "device_id": device_id,
                            "tipo": "SISTEMA",
                            "origen": "AlertService_Heartbeat",
                            "timestamp": {"$gte": one_hour_ago}
                        }
                    )

                    if existing_alert:
                        existing_alerts_skipped += 1
                        logger.debug(
                            "Alerta de offline existente, evitando spam",
                            device_id=device_id,
                            existing_alert_id=str(existing_alert.get("_id"))
                        )
                        continue

                    # ─── 3. Crear nuevo evento de offline ──────────────────
                    evento = EventCreate(
                        device_id=device_id,
                        tipo="SISTEMA",
                        origen="AlertService_Heartbeat",
                        mensaje=(
                            f"Dispositivo inactivo: No emite datos desde "
                            f"{(now - last_seen).total_seconds():.0f} segundos"
                        ),
                        nivel="critico",
                        timestamp=now,
                        metadata={
                            "last_seen": last_seen.isoformat(),
                            "timeout_segundos": timeout_seconds,
                            "inactividad_segundos": int((now - last_seen).total_seconds()),
                            "total_lecturas": device_status.get("count", 0)
                        }
                    )

                    doc = evento.model_dump()
                    doc["received_at"] = now

                    result = await self.events_collection.insert_one(doc)
                    alerts_created += 1

                    logger.critical(
                        "Dispositivo offline detectado",
                        device_id=device_id,
                        event_id=str(result.inserted_id),
                        inactividad_segundos=int((now - last_seen).total_seconds())
                    )

            return {
                "offline_devices": offline_devices,
                "alerts_created": alerts_created,
                "existing_alerts_skipped": existing_alerts_skipped,
                "evaluation_time": now.isoformat(),
                "timeout_seconds": timeout_seconds
            }

        except Exception as e:
            logger.error(
                "Error en evaluate_device_heartbeat",
                error=str(e)
            )
            raise DatabaseConnectionException(
                "No se pudo evaluar heartbeat de dispositivos"
            )

    async def dispatch_critical_notification(
        self,
        event_id: str,
        event_dict: Dict[str, Any]
    ) -> bool:
        """
        Envía notificaciones (webhooks, logs, futuro: FCM) para eventos críticos.

        Estructura base para implementar:
        - Webhooks HTTP a servicios externos (Telegram, Slack, etc.)
        - Logging estructurado de eventos críticos
        - Futuro (v1.5): Firebase Cloud Messaging (FCM) para app móvil

        Args:
            event_id: ID del evento en MongoDB
            event_dict: Diccionario con datos del evento

        Returns:
            True si la notificación se despachó exitosamente
        """
        try:
            device_id = event_dict.get("device_id", "unknown")
            tipo = event_dict.get("tipo", "UNKNOWN")
            nivel = event_dict.get("nivel", "info")
            mensaje = event_dict.get("mensaje", "Sin descripción")

            # ─── Log estructurado de eventos críticos ────────────────────
            if nivel in ["critico", "alerta"]:
                logger.critical(
                    "Evento crítico despachado",
                    event_id=event_id,
                    device_id=device_id,
                    tipo=tipo,
                    nivel=nivel,
                    mensaje=mensaje[:100]  # Primeros 100 chars
                )
            else:
                logger.warning(
                    "Evento de advertencia despachado",
                    event_id=event_id,
                    device_id=device_id,
                    tipo=tipo,
                    mensaje=mensaje[:100]
                )

            # ─── Futura integración: Webhooks HTTP (v1.5) ────────────────
            # webhook_urls = await self.get_webhook_urls(device_id)
            # for url in webhook_urls:
            #     await self._send_webhook(url, event_dict)

            # ─── Futura integración: FCM (v1.5) ────────────────────────
            # if nivel == "critico":
            #     await self._send_fcm_notification(device_id, mensaje)

            return True

        except Exception as e:
            logger.error(
                "Error despachando notificación crítica",
                error=str(e),
                event_id=event_id
            )
            return False

    async def get_device_health_status(self, device_id: str) -> Dict[str, Any]:
        """
        Obtiene el estado de salud de un dispositivo: conectividad, eventos recientes.

        Args:
            device_id: ID del dispositivo

        Returns:
            Dict con estado de conectividad, última lectura, eventos recientes
        """
        try:
            now = datetime.now(timezone.utc)

            # Última lectura
            last_reading = await self.readings_collection.find_one(
                {"device_id": device_id},
                sort=[("timestamp", -1)]
            )

            # Eventos críticos en última hora
            one_hour_ago = now - timedelta(hours=1)
            critical_events_cursor = self.events_collection.find(
                {
                    "device_id": device_id,
                    "nivel": {"$in": ["critico", "alerta"]},
                    "timestamp": {"$gte": one_hour_ago}
                }
            )
            critical_events = await critical_events_cursor.to_list(length=10)

            is_online = False
            inactividad_segundos = None

            if last_reading:
                last_timestamp = last_reading.get("timestamp")
                if last_timestamp:
                    inactividad_segundos = int((now - last_timestamp).total_seconds())
                    is_online = inactividad_segundos < 30

            return {
                "device_id": device_id,
                "is_online": is_online,
                "last_reading_timestamp": last_reading.get("timestamp").isoformat() if last_reading else None,
                "inactividad_segundos": inactividad_segundos,
                "critical_events_1h": len(critical_events),
                "recent_events": [
                    {
                        "id": str(e.get("_id")),
                        "tipo": e.get("tipo"),
                        "nivel": e.get("nivel"),
                        "mensaje": e.get("mensaje")[:80],
                        "timestamp": e.get("timestamp").isoformat() if e.get("timestamp") else None
                    }
                    for e in critical_events[:5]
                ],
                "evaluation_time": now.isoformat()
            }

        except Exception as e:
            logger.error(
                "Error obteniendo health status",
                error=str(e),
                device_id=device_id
            )
            raise DatabaseConnectionException(
                "No se pudo obtener estado del dispositivo"
            )
