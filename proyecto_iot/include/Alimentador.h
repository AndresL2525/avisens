/*
 * ============================================================================
 *  Alimentador.h
 * ----------------------------------------------------------------------------
 *  Controla el motorreductor del tornillo sin fin que dosifica alimento.
 *
 *  Reglas de seguridad (en este orden de prioridad):
 *    1. Nunca gira más de ALIMENTADOR_TIEMPO_MAX_MS de una sola vez,
 *       sin importar qué -- esto evita que un fallo de lectura del
 *       HX711 deje el motor girando indefinidamente y se desborde el
 *       plato o se dañe el motorreductor.
 *    2. Se detiene antes si el HX711 ya reporta el peso objetivo en
 *       el plato.
 *    3. No vuelve a alimentar automáticamente hasta que pase
 *       ALIMENTADOR_INTERVALO_MS desde la última vez, para no estar
 *       arrancando el motor en cada ciclo si el plato sigue vacío por
 *       alguna otra razón (ej. el plato fue retirado).
 *
 *  Igual que con los relés de clima, este módulo respeta un modo
 *  MANUAL: si el usuario ordena alimentar desde la app, se ejecuta una
 *  dosis sin importar el intervalo (pero el límite de tiempo máximo de
 *  giro sigue aplicando siempre, por seguridad).
 * ============================================================================
 */

#ifndef ALIMENTADOR_H
#define ALIMENTADOR_H

#include <Arduino.h>
#include "Actuador.h"

class Alimentador
{
public:
    Alimentador(int pinMotor);

    void iniciar();

    // Debe llamarse en cada vuelta del loop(). Internamente decide si
    // hay que seguir girando, detenerse, o esperar.
    // pesoActualPlato: la última lectura del HX711 en gramos.
    void actualizar(float pesoActualPlato);

    // Inicia una dosis manual inmediata (ignora el intervalo de espera,
    // pero respeta el tiempo máximo de giro y el peso objetivo).
    void solicitarDosisManual();

    bool estaActivo() const;
    unsigned long obtenerUltimaAlimentacion() const;

private:
    Actuador _motor;
    bool _girando;
    unsigned long _inicioGiro;
    unsigned long _ultimaAlimentacion;
    bool _dosisManualPendiente;

    void iniciarGiro();
    void detenerGiro(const char *razon);
};

#endif // ALIMENTADOR_H
