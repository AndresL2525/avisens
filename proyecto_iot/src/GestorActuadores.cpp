/*
 * ============================================================================
 *  GestorActuadores.cpp
 * ----------------------------------------------------------------------------
 *  Implementación de la lógica de decisión automática para cada actuador.
 * ============================================================================
 */

#include "GestorActuadores.h"
#include "config.h"

GestorActuadores::GestorActuadores()
    : _calefactor("calefactor", RELE_CALEFACTOR_PIN),
      _extractor("extractor", RELE_EXTRACTOR_PIN),
      _humidificador("humidificador", RELE_HUMIDIFICADOR_PIN),
      _alimentador(RELE_ALIMENTADOR_PIN),
      _humidificadorEncendidoPrevio(false)
{
}

void GestorActuadores::iniciar()
{
    _calefactor.iniciar();
    _extractor.iniciar();
    _humidificador.iniciar();
    _alimentador.iniciar();
    Serial.println("[GestorActuadores] Todos los actuadores inicializados (apagados).");
}

// ────────────────────────────────────────────────────────────────────────
// CALEFACTOR: se enciende cuando la temperatura está por debajo del
// umbral frío. Único límite, sin histéresis (el promedio de lecturas
// del DHT11 ya amortigua el ruido).
// ────────────────────────────────────────────────────────────────────────
void GestorActuadores::evaluarCalefactor(float temperatura)
{
    if (_calefactor.obtenerModo() == ModoActuador::MANUAL)
    {
        return; // El usuario tiene el control; no tocamos nada aquí.
    }

    bool deberiaEstarEncendido = (temperatura < UMBRAL_TEMP_BAJA);
    _calefactor.establecerEstado(deberiaEstarEncendido);
}

// ────────────────────────────────────────────────────────────────────────
// EXTRACTOR: se enciende si la temperatura está alta O si la calidad
// del aire está mala. Es un OR -- cualquiera de las dos condiciones
// es suficiente, porque ventilar ayuda en ambos casos.
// ────────────────────────────────────────────────────────────────────────
void GestorActuadores::evaluarExtractor(float temperatura, int calidadAire)
{
    if (_extractor.obtenerModo() == ModoActuador::MANUAL)
    {
        return;
    }

    bool temperaturaAlta = (temperatura > UMBRAL_TEMP_ALTA);
    bool aireMalo = (calidadAire > UMBRAL_AIRE_MALO);

    _extractor.establecerEstado(temperaturaAlta || aireMalo);
}

// ────────────────────────────────────────────────────────────────────────
// HUMIDIFICADOR: usa HISTÉRESIS de doble umbral para evitar parpadeo.
// Se ENCIENDE cuando la humedad cae por debajo de UMBRAL_HUMEDAD_BAJA,
// y permanece encendido hasta que suba por encima de
// UMBRAL_HUMEDAD_ALTA. Entre esos dos valores, simplemente mantiene
// el estado que ya tenía.
// ────────────────────────────────────────────────────────────────────────
void GestorActuadores::evaluarHumidificador(float humedad)
{
    if (_humidificador.obtenerModo() == ModoActuador::MANUAL)
    {
        return;
    }

    bool nuevoEstado = _humidificadorEncendidoPrevio;

    if (humedad < UMBRAL_HUMEDAD_BAJA)
    {
        nuevoEstado = true;
    }
    else if (humedad > UMBRAL_HUMEDAD_ALTA)
    {
        nuevoEstado = false;
    }
    // Si está entre los dos umbrales, nuevoEstado se queda como estaba.

    _humidificador.establecerEstado(nuevoEstado);
    _humidificadorEncendidoPrevio = nuevoEstado;
}

void GestorActuadores::evaluar(const LecturasActuales &lecturas)
{
    evaluarCalefactor(lecturas.temperatura);
    evaluarExtractor(lecturas.temperatura, lecturas.calidadAire);
    evaluarHumidificador(lecturas.humedad);
    _alimentador.actualizar(lecturas.pesoPlato);
}

// ── Órdenes manuales: cambian el modo y, si aplica, fuerzan un estado ───
void GestorActuadores::establecerModoCalefactor(ModoActuador modo)
{
    _calefactor.establecerModo(modo);
}
void GestorActuadores::establecerModoExtractor(ModoActuador modo)
{
    _extractor.establecerModo(modo);
}
void GestorActuadores::establecerModoHumidificador(ModoActuador modo)
{
    _humidificador.establecerModo(modo);
}

void GestorActuadores::ordenManualCalefactor(bool encendido)
{
    if (_calefactor.obtenerModo() == ModoActuador::MANUAL)
    {
        _calefactor.establecerEstado(encendido);
    }
}
void GestorActuadores::ordenManualExtractor(bool encendido)
{
    if (_extractor.obtenerModo() == ModoActuador::MANUAL)
    {
        _extractor.establecerEstado(encendido);
    }
}
void GestorActuadores::ordenManualHumidificador(bool encendido)
{
    if (_humidificador.obtenerModo() == ModoActuador::MANUAL)
    {
        _humidificador.establecerEstado(encendido);
    }
}

void GestorActuadores::solicitarDosisManualAlimentador()
{
    _alimentador.solicitarDosisManual();
}

// ── Getters ──────────────────────────────────────────────────────────────
const Actuador &GestorActuadores::calefactor() const { return _calefactor; }
const Actuador &GestorActuadores::extractor() const { return _extractor; }
const Actuador &GestorActuadores::humidificador() const { return _humidificador; }
const Alimentador &GestorActuadores::alimentador() const { return _alimentador; }
