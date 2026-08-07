/*
 * ============================================================================
 *  Alimentador.cpp
 * ----------------------------------------------------------------------------
 *  Implementación de la lógica de dosificación de alimento.
 * ============================================================================
 */

#include "Alimentador.h"
#include "config.h"

Alimentador::Alimentador(int pinMotor)
    : _motor("alimentador", pinMotor),
      _girando(false),
      _inicioGiro(0),
      _ultimaAlimentacion(0),
      _dosisManualPendiente(false)
{
}

void Alimentador::iniciar()
{
    _motor.iniciar();
}

void Alimentador::solicitarDosisManual()
{
    _dosisManualPendiente = true;
}

void Alimentador::iniciarGiro()
{
    _motor.establecerEstado(true);
    _girando = true;
    _inicioGiro = millis();
}

void Alimentador::detenerGiro(const char *razon)
{
    _motor.establecerEstado(false);
    _girando = false;
    _ultimaAlimentacion = millis();
    Serial.printf("[Alimentador] Dosis finalizada (%s)\n", razon);
}

void Alimentador::actualizar(float pesoActualPlato)
{
    unsigned long ahora = millis();

    // ── Caso 1: ya está girando -> evaluar si debe detenerse ────────────
    if (_girando)
    {
        // Regla de seguridad #1: tiempo máximo de giro, SIEMPRE se respeta.
        if (ahora - _inicioGiro >= ALIMENTADOR_TIEMPO_MAX_MS)
        {
            detenerGiro("tiempo maximo alcanzado");
            return;
        }

        // Regla #2: si el plato ya alcanzó el peso objetivo, paramos antes.
        if (pesoActualPlato >= ALIMENTADOR_PESO_OBJETIVO_G)
        {
            detenerGiro("peso objetivo alcanzado");
            return;
        }

        // Si ninguna condición de parada se cumple, sigue girando.
        return;
    }

    // ── Caso 2: no está girando -> evaluar si debe arrancar ─────────────

    // Una dosis manual ordenada desde la app tiene prioridad inmediata,
    // sin importar el intervalo (pero respetando que el plato no esté
    // ya lleno).
    if (_dosisManualPendiente)
    {
        _dosisManualPendiente = false;
        if (pesoActualPlato < ALIMENTADOR_PESO_OBJETIVO_G)
        {
            iniciarGiro();
        }
        else
        {
            Serial.println("[Alimentador] Dosis manual ignorada: el plato ya esta lleno.");
        }
        return;
    }

    // Modo automático: solo alimenta si pasó el intervalo Y el plato
    // está por debajo del peso objetivo.
    bool yaPasoElIntervalo = (ahora - _ultimaAlimentacion) >= ALIMENTADOR_INTERVALO_MS;
    bool platoNecesitaAlimento = pesoActualPlato < ALIMENTADOR_PESO_OBJETIVO_G;

    if (yaPasoElIntervalo && platoNecesitaAlimento)
    {
        iniciarGiro();
    }
}

bool Alimentador::estaActivo() const
{
    return _girando;
}

unsigned long Alimentador::obtenerUltimaAlimentacion() const
{
    return _ultimaAlimentacion;
}
