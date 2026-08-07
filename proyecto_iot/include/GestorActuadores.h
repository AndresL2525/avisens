/*
 * ============================================================================
 *  GestorActuadores.h
 * ----------------------------------------------------------------------------
 *  Aquí vive la LÓGICA DE DECISIÓN: qué actuador debe encenderse según
 *  las lecturas actuales de los sensores. Es el "cerebro" autónomo del
 *  ESP32 que mencionamos en el diseño: el ESP32 decide solo, y las apps
 *  solo pueden anular (modo MANUAL) o volver a ceder el control (modo
 *  AUTO) a través de Firebase.
 *
 *  Cada actuador se evalúa de forma INDEPENDIENTE -- no hay arbitraje
 *  entre ellos porque cada uno controla hardware distinto (el
 *  calefactor no compite con el extractor por el mismo relé), así que
 *  es perfectamente válido que el calefactor esté encendido y el
 *  extractor también, si las condiciones de ambos se cumplen a la vez
 *  (ej. temperatura baja + mala calidad de aire simultáneas).
 *
 *  Para evitar que un actuador esté "parpadeando" cerca del límite
 *  exacto del umbral, usamos HISTÉRESIS en los umbrales de doble
 *  límite (humedad). Para los umbrales de un solo límite (temperatura,
 *  aire) el ESP32 ya promedia varias lecturas en los propios sensores,
 *  lo cual amortigua el ruido.
 * ============================================================================
 */

#ifndef GESTOR_ACTUADORES_H
#define GESTOR_ACTUADORES_H

#include <Arduino.h>
#include "Actuador.h"
#include "Alimentador.h"

// Snapshot de lecturas que el gestor necesita para decidir. Mantenerlo
// como struct evita pasar 5 parámetros sueltos a cada función.
struct LecturasActuales
{
    float temperatura;
    float humedad;
    int calidadAire;
    float pesoPlato;
};

// Resultado de una decisión, usado para poder generar el evento/log
// correspondiente sin que GestorActuadores conozca nada de Firebase.
struct CambioActuador
{
    const char *nombre;
    bool encendido;
    const char *motivo;
};

class GestorActuadores
{
public:
    GestorActuadores();

    void iniciar();

    // Debe llamarse en cada ciclo de lectura de sensores. Internamente
    // decide el estado de cada actuador y los aplica.
    void evaluar(const LecturasActuales &lecturas);

    // ── Órdenes manuales desde Firebase (apps) ──────────────────────────
    void establecerModoCalefactor(ModoActuador modo);
    void establecerModoExtractor(ModoActuador modo);
    void establecerModoHumidificador(ModoActuador modo);
    void ordenManualCalefactor(bool encendido);
    void ordenManualExtractor(bool encendido);
    void ordenManualHumidificador(bool encendido);
    void solicitarDosisManualAlimentador();

    // ── Getters para reportar estado a Firebase / OLED ──────────────────
    const Actuador &calefactor() const;
    const Actuador &extractor() const;
    const Actuador &humidificador() const;
    const Alimentador &alimentador() const;

private:
    Actuador _calefactor;
    Actuador _extractor;
    Actuador _humidificador;
    Alimentador _alimentador;

    // Estado interno para la histéresis del humidificador (necesita
    // recordar si "ya estaba encendido" para no parpadear justo en el
    // límite entre UMBRAL_HUMEDAD_BAJA y UMBRAL_HUMEDAD_ALTA).
    bool _humidificadorEncendidoPrevio;

    void evaluarCalefactor(float temperatura);
    void evaluarExtractor(float temperatura, int calidadAire);
    void evaluarHumidificador(float humedad);
};

#endif // GESTOR_ACTUADORES_H
