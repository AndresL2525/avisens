# Documento de Diseño del Sistema (SSD)
# AVÍSENS — Sistema IoT para Granjas Avícolas (Enfoque Edge & MQTT)

---

## Tabla de Contenidos

1. [Introducción](#1-introducción)
2. [Descripción General del Sistema y Alcance IoT](#2-descripción-general-del-sistema-y-alcance-iot)
3. [Arquitectura del Sistema y Flujo de Comunicación](#3-arquitectura-del-sistema-y-flujo-de-comunicación)
4. [Diseño Detallado del Firmware Embebido (ESP32)](#4-diseño-detallado-del-firmware-embebido-esp32)
5. [Lógica de Control Local y Algoritmos PID](#5-lógica-de-control-local-y-algoritmos-pid)
6. [Contrato de Integración y Parámetros MQTT](#6-contrato-de-integración-y-parámetros-mqtt)
7. [Manejo de Errores, Robustez y Fail-Safe](#7-manejo-de-errores-robustez-y-fail-safe)
8. [Estándar de Documentación Externa y Calidad de Código](#8-estándar-de-documentación-externa-y-calidad-de-código)
9. [Plan de Pruebas y Validación](#9-plan-de-pruebas-y-validación)
10. [Apéndices](#10-apéndices)

---

## 1. Introducción

### 1.1 Propósito
Este documento define el diseño técnico formal del subsistema **IoT / Edge** del proyecto **AVÍSENS**. Establece la arquitectura de hardware y firmware del microcontrolador ESP32, los lazos de control autónomos (incluyendo algoritmos PID), el esquema de comunicaciones mediante el protocolo MQTT y el estándar de documentación desacoplada para garantizar código limpio y mantenible.

### 1.2 Alcance
El alcance de este diseño se delimita estrictamente al desarrollo embebido y su frontera de comunicación:
- Adquisición de señales de sensores ambientales y físicos (DHT22, MQ-135, HX711, sensor de presencia/obstáculo).
- Control de potencia mediante etapas de relés y transistores para calefacción, humidificación, extracción forzada y alimentación.
- Lógica de control en lazo cerrado y operación autónoma en caso de fallo de red.
- Protocolo de enlace telemétrico bidireccional cliente-broker basado en MQTT.
- Esquema de integración hacia el Backend / Frontend (contrato de mensajería).
- Estándar de documentación técnica externa para el firmware.

*Nota de exclusión:* Se excluye la implementación interna de microservicios de backend (FastAPI, Django, MongoDB), diseño de interfaces móviles/web (Kotlin, React) y pasarelas de autenticación de usuario (JWT).

### 1.3 Definiciones y Acrónimos

| Término | Definición |
|:---|:---|
| **SSD** | System Design Document (Documento de Diseño del Sistema) |
| **Edge Computing** | Procesamiento de control y filtrado directamente en el nodo sensor/actuador |
| **Broker MQTT** | Servidor central de enrutamiento y despacho de mensajes Publish/Subscribe |
| **Topic** | Cadena de caracteres jerárquica que actúa como canal de distribución en MQTT |
| **Payload** | Contenido útil transmitido en un paquete MQTT (serializado en JSON) |
| **QoS** | Quality of Service (Calidad de Servicio en entrega de paquetes MQTT) |
| **LWT** | Last Will and Testament (Mensaje de última voluntad emitido por desconexión no deseada) |
| **PID** | Proporcional - Integral - Derivativo (Control de lazo cerrado analógico/discreto) |
| **ADC** | Analog to Digital Converter (Conversor Analógico a Digital) |
| **WDT** | Watchdog Timer (Temporizador de reinicio de seguridad ante bloqueo) |

---

## 2. Descripción General del Sistema y Alcance IoT

### 2.1 Perspectiva del Producto
AVÍSENS opera como una solución modular de automatización avícola donde el ESP32 actúa como cerebro operativo del galpón. El dispositivo garantiza el bienestar animal manteniendo microclimas óptimos mediante lecturas periódicas y algoritmos de control local, publicando periódicamente su telemetría e informando su estado a una pasarela central (Broker MQTT) a la que acceden los servicios de backend y aplicaciones cliente.