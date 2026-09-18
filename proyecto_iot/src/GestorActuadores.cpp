#include "GestorActuadores.h"

GestorActuadores::GestorActuadores()
    : k1_(K1_PIN),
      k2_(K2_PIN),
      k3_(K3_PIN),
      k4_(K4_PIN),
      ultimoEstadoBomba_(false),
      ultimoControl_(0) {
}

void GestorActuadores::begin() {
  k1_.begin();
  k2_.begin();
  k3_.begin();
  k4_.begin();
  LOG_DEBUG("GestorActuadores inicializado");
}

void GestorActuadores::actualizar(
    float temperatura,
    float humedad,
    int rawNH3,
    float distanciaAgua,
    EstadoSensorUltrasonico estadoSensorUltrasonico,
    bool enErrorDHT,
    bool enErrorUltrasonico) {

  unsigned long ahora = millis();

  // ─── Fail-Safe: Si hay error crítico en sensores ───────
  if (enErrorDHT || enErrorUltrasonico) {
    LOG_WARN("Sensor en error — Entrando en Fail-Safe");
    failSafe();
    return;
  }

  // ─── Lógica de control con umbrales ───────────────────

  // Detectar si hay gases altos
  bool gasesAltos = (rawNH3 >= NH3_ALTO);

  // Calefacción: Activar si NO hay gases altos Y temperatura < TEMP_FRIO
  bool activarCalefaccion = !gasesAltos && (temperatura < TEMP_FRIO);

  // Ventilación (Ventilador + Extractor):
  // Activar si NO hay calefacción activa Y:
  //   - Temperatura >= TEMP_CALOR, O
  //   - Humedad > HUM_EXTRACTORES, O
  //   - Gases altos
  bool activarVentilacion = !activarCalefaccion &&
                            (temperatura >= TEMP_CALOR ||
                             humedad > HUM_EXTRACTORES ||
                             gasesAltos);

  // Bomba: Uso de histéresis (ON si dist > NIVEL_BOMBA_ON, OFF si dist <= NIVEL_BOMBA_OFF)
  bool activarBomba = calcularActivacionBomba(distanciaAgua, estadoSensorUltrasonico);

  // ─── Aplicar estados ──────────────────────────────────
  aplicarControl(activarCalefaccion, activarVentilacion, activarBomba);

  // ─── Log de monitoreo ──────────────────────────────────
  Serial.println("\n================================");
  Serial.print("Temp: ");
  Serial.print(temperatura, 1);
  Serial.print("°C | Hum: ");
  Serial.print(humedad, 1);
  Serial.println("%");
  Serial.print("NH3: ");
  Serial.print(rawNH3);
  Serial.print(" [");
  if (rawNH3 < NH3_MODERADO) Serial.print("NORMAL");
  else if (rawNH3 < NH3_ALTO) Serial.print("MODERADO");
  else Serial.print("ALTO");
  Serial.println("]");
  Serial.print("Agua: ");
  if (estadoSensorUltrasonico == EstadoSensorUltrasonico::OK) {
    Serial.print(distanciaAgua, 1);
    Serial.println(" cm");
  } else {
    Serial.println("ERROR/TIMEOUT");
  }
  Serial.println("--- ACTUADORES ---");
  Serial.print("K1 (Calef): ");
  Serial.println(k1_.getEstado() ? "ON" : "off");
  Serial.print("K2 (Ventil): ");
  Serial.println(k2_.getEstado() ? "ON" : "off");
  Serial.print("K3 (Extract): ");
  Serial.println(k3_.getEstado() ? "ON" : "off");
  Serial.print("K4 (Bomba): ");
  Serial.println(k4_.getEstado() ? "ON" : "off");
  Serial.println("================================");
}

void GestorActuadores::failSafe() {
  // En Fail-Safe: Desactivar calefacción y ventilación
  // Mantener bomba activa como medida de seguridad
  k1_.desactivar();
  k2_.desactivar();
  k3_.desactivar();
  k4_.activar();  // Bomba ON para drenaje de emergencia

  LOG_ERROR("FAIL-SAFE ACTIVADO — Bomba forzada ON");
}

void GestorActuadores::aplicarControl(
    bool activarCalefaccion,
    bool activarVentilacion,
    bool activarBomba) {

  // K1: Calefacción (solo si no hay ventilación) — omitido si está en MANUAL
  if (!manualK1_) {
    if (activarCalefaccion) {
      k1_.activar();
    } else {
      k1_.desactivar();
    }
  }

  // K2: Ventilador (complementa tanto calefacción como ventilación)
  if (!manualK2_) {
    if (activarCalefaccion || activarVentilacion) {
      k2_.activar();
    } else {
      k2_.desactivar();
    }
  }

  // K3: Extractor (solo si hay ventilación)
  if (!manualK3_) {
    if (activarVentilacion) {
      k3_.activar();
    } else {
      k3_.desactivar();
    }
  }

  // K4: Bomba
  if (!manualK4_) {
    if (activarBomba) {
      k4_.activar();
    } else {
      k4_.desactivar();
    }
  }
}

// ═══════════════════════════════════════════════════════════
// ─── CONTROL REMOTO / MODO MANUAL ─────────────────────────
// ═══════════════════════════════════════════════════════════

void GestorActuadores::establecerManual(uint8_t rele, bool estado) {
  switch (rele) {
    case 1:
      manualK1_ = true;
      k1_.setEstado(estado);
      break;
    case 2:
      manualK2_ = true;
      k2_.setEstado(estado);
      break;
    case 3:
      manualK3_ = true;
      k3_.setEstado(estado);
      break;
    case 4:
      manualK4_ = true;
      k4_.setEstado(estado);
      break;
    default:
      LOG_WARN("establecerManual: número de relé inválido: " + String(rele));
      return;
  }
  LOG_WARN("Relé " + String(rele) + " -> MANUAL " + (estado ? "ON" : "OFF"));
}

void GestorActuadores::establecerAutomatico(uint8_t rele) {
  switch (rele) {
    case 1:
      manualK1_ = false;
      break;
    case 2:
      manualK2_ = false;
      break;
    case 3:
      manualK3_ = false;
      break;
    case 4:
      manualK4_ = false;
      break;
    default:
      LOG_WARN("establecerAutomatico: número de relé inválido: " + String(rele));
      return;
  }
  LOG_DEBUG("Relé " + String(rele) + " devuelto a modo AUTOMÁTICO");
}

bool GestorActuadores::esManual(uint8_t rele) const {
  switch (rele) {
    case 1: return manualK1_;
    case 2: return manualK2_;
    case 3: return manualK3_;
    case 4: return manualK4_;
    default: return false;
  }
}

bool GestorActuadores::getEstado(uint8_t rele) const {
  switch (rele) {
    case 1: return k1_.getEstado();
    case 2: return k2_.getEstado();
    case 3: return k3_.getEstado();
    case 4: return k4_.getEstado();
    default: return false;
  }
}

uint8_t GestorActuadores::releDesdeNombre(const String& nombre) {
  String n = nombre;
  n.toLowerCase();

  // NOTA IMPORTANTE: el Literal de ActuatorCommand.nombre en el backend
  // (ver SSD_AVISENS.md, 4.3.2) es exactamente:
  //   "calefactor" | "extractor" | "humidificador" | "alimentador"
  // Tu hardware real no tiene humidificador: K2 es un VENTILADOR.
  // Se acepta "humidificador" como alias de K2 para que el backend
  // funcione tal cual está documentado, pero deberías decidir si:
  //   a) renombras el Literal del backend a "ventilador", o
  //   b) dejas "humidificador" como el nombre lógico que usa la app,
  //      aunque físicamente mueva el relé del ventilador.
  // "bomba" (K4) no está en el Literal del backend a propósito: la
  // bomba se controla solo de forma automática/fail-safe, no manual.
  if (n == "calefactor" || n == "k1") return 1;
  if (n == "ventilador" || n == "humidificador" || n == "k2") return 2;
  if (n == "extractor"  || n == "k3") return 3;
  if (n == "bomba"      || n == "k4") return 4;

  return 0;  // No reconocido
}

String GestorActuadores::nombreDesdeRele(uint8_t rele) {
  switch (rele) {
    case 1: return "calefactor";
    case 2: return "ventilador";
    case 3: return "extractor";
    case 4: return "bomba";
    default: return "desconocido";
  }
}

bool GestorActuadores::calcularActivacionBomba(
    float distancia,
    EstadoSensorUltrasonico estado) {

  // Si hay error en el sensor, mantener último estado (conservador)
  if (estado != EstadoSensorUltrasonico::OK) {
    return ultimoEstadoBomba_;
  }

  // Histéresis: ON si dist > NIVEL_BOMBA_ON, OFF si dist <= NIVEL_BOMBA_OFF
  if (distancia > NIVEL_BOMBA_ON) {
    ultimoEstadoBomba_ = true;
  } else if (distancia <= NIVEL_BOMBA_OFF) {
    ultimoEstadoBomba_ = false;
  }
  // Si está entre los dos umbrales, mantener estado anterior

  return ultimoEstadoBomba_;
}

void GestorActuadores::forzarRele(uint8_t rele, bool estado) {
  switch (rele) {
    case 1:
      k1_.setEstado(estado);
      break;
    case 2:
      k2_.setEstado(estado);
      break;
    case 3:
      k3_.setEstado(estado);
      break;
    case 4:
      k4_.setEstado(estado);
      break;
    default:
      LOG_WARN("Relé inválido");
  }
}
