#include "SistemaFSM.h"
#include "Nodo.h"
#include "Mensajeria.h"

SistemaFSM::SistemaFSM()
    : estado_(EstadoSistema::INIT),
      ultimoEstadoAtendido_(EstadoSistema::INIT),
      ciclosArranque_(0),
      ciclosEnCalibracion_(0),
      calibracionCompletada_(false),
      fallosAcumulados_(0)
{
}

bool SistemaFSM::enFailSafe() const
{
  return estado_ == EstadoSistema::ERROR || estado_ == EstadoSistema::SHUTDOWN;
}

void SistemaFSM::rearmar()
{
  LOG_WARN("Comando de rearme recibido");
  estado_ = EstadoSistema::INIT;
  ciclosArranque_ = 0;
  ciclosEnCalibracion_ = 0;
  calibracionCompletada_ = false;
}

void SistemaFSM::marcarCalibracionCompletada()
{
  calibracionCompletada_ = true;
}

void SistemaFSM::evaluarSensoresCriticos(bool errorCritico)
{
  if (!errorCritico)
  {
    return;
  }

  fallosAcumulados_++;
  if (fallosAcumulados_ >= MAX_FALLOS_SENSOR && estado_ != EstadoSistema::ERROR)
  {
    estado_ = EstadoSistema::ERROR;
    LOG_ERROR("Fallos acumulados >= " + String(MAX_FALLOS_SENSOR) + ", transicion a ERROR");
  }
}

void SistemaFSM::aplicarFailSafe()
{
  gestorActuadores.failSafe();
  controlServo.cerrarEmergencia();
  alimentador.detener();
  persiana.detener();
}

void SistemaFSM::avanzar()
{
  switch (estado_)
  {
  case EstadoSistema::INIT:
    ciclosArranque_++;
    if (ciclosArranque_ >= CICLOS_ARRANQUE_MIN)
    {
      estado_ = EstadoSistema::CALIBRATION;
      ciclosEnCalibracion_ = 0;
      LOG_DEBUG("[FSM] INIT -> CALIBRATION");
    }
    break;

  case EstadoSistema::CALIBRATION:
    ciclosEnCalibracion_++;
    if (calibracionCompletada_ ||
        (ciclosEnCalibracion_ * PERIODO_TAREA_CONTROL_MS) >= TIMEOUT_CALIBRACION_MS)
    {
      estado_ = EstadoSistema::MONITORING;
      LOG_DEBUG("[FSM] CALIBRATION -> MONITORING");
    }
    break;

  case EstadoSistema::MONITORING:
    break;

  case EstadoSistema::ACTUATION:
    estado_ = EstadoSistema::MONITORING;
    break;

  case EstadoSistema::ERROR:
    if (ultimoEstadoAtendido_ != EstadoSistema::ERROR)
    {
      aplicarFailSafe();
      Mensajeria::encolarEvento(
          "SensoresCriticos",
          "Fallos persistentes en sensores criticos, sistema en fail-safe",
          "critico",
          fallosAcumulados_);

      LOG_ERROR("FAIL-SAFE activado: K1-K3 OFF, K4 ON");
      ultimoEstadoAtendido_ = EstadoSistema::ERROR;
    }

    if (!sensorDHT.enError() && !sensorUltrasonico.enError())
    {
      estado_ = EstadoSistema::MONITORING;
      fallosAcumulados_ = 0;
      LOG_DEBUG("[FSM] Sensores recuperados: ERROR -> MONITORING");
    }
    break;

  case EstadoSistema::SHUTDOWN:
    if (ultimoEstadoAtendido_ != EstadoSistema::SHUTDOWN)
    {
      gestorActuadores.failSafe();
      alimentador.setHabilitado(false);
      persiana.setHabilitado(false);
      LOG_ERROR("[FSM] SHUTDOWN: actuadores en configuracion segura");
    }
    break;

  default:
    estado_ = EstadoSistema::MONITORING;
  }

  if (estado_ != EstadoSistema::ERROR)
  {
    ultimoEstadoAtendido_ = estado_;
  }
}
