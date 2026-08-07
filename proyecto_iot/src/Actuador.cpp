/*
 * ============================================================================
 *  Actuador.cpp
 * ----------------------------------------------------------------------------
 *  Implementación del control físico de un relé genérico.
 * ============================================================================
 */

#include "Actuador.h"

Actuador::Actuador(const char* nombre, int pin)
    : _nombre(nombre), _pin(pin), _encendido(false), _modo(ModoActuador::AUTO) {
}

void Actuador::iniciar() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW); // Empieza apagado por seguridad
    _encendido = false;
}

void Actuador::establecerEstado(bool encendido) {
    // Si ya está en el estado pedido, no hacemos nada (evita escribir
    // el pin innecesariamente y, más importante, evita generar un
    // evento/log duplicado cada vez que se llama esta función).
    if (encendido == _encendido) return;

    digitalWrite(_pin, encendido ? HIGH : LOW);
    _encendido = encendido;

    Serial.printf("[Actuador:%s] %s\n", _nombre, encendido ? "ENCENDIDO" : "APAGADO");
}

void Actuador::establecerModo(ModoActuador modo) {
    if (modo != _modo) {
        Serial.printf("[Actuador:%s] Modo cambiado a %s\n",
                      _nombre, modo == ModoActuador::AUTO ? "AUTO" : "MANUAL");
    }
    _modo = modo;
}

bool Actuador::estaEncendido() const {
    return _encendido;
}

ModoActuador Actuador::obtenerModo() const {
    return _modo;
}

const char* Actuador::obtenerNombre() const {
    return _nombre;
}
