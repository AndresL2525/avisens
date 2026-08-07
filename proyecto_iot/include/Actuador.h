/*
 * ============================================================================
 *  Actuador.h
 * ----------------------------------------------------------------------------
 *  Módulo genérico para controlar UN relé (calefactor, extractor,
 *  humidificador). Se reutiliza la misma clase para los 3, cambiando
 *  solo el pin — así evitamos repetir código 3 veces.
 *
 *  Cada actuador tiene un MODO:
 *    - AUTO:   el ESP32 decide solo, según la lectura de sensores.
 *    - MANUAL: el usuario (desde la app/web) impone un estado fijo,
 *              y el ESP32 lo respeta sin importar lo que digan los
 *              sensores, hasta que alguien vuelva a poner modo AUTO.
 *
 *  Esta clase NO decide cuándo encender o apagar (esa lógica vive en
 *  GestorActuadores.cpp); esta clase solo SABE encender/apagar el pin
 *  físico y recordar su propio estado y modo actual.
 * ============================================================================
 */

#ifndef ACTUADOR_H
#define ACTUADOR_H

#include <Arduino.h>

enum class ModoActuador
{
    AUTO,
    MANUAL
};

class Actuador
{
public:
    // nombre: solo para identificar en logs / Firebase (ej. "calefactor")
    Actuador(const char *nombre, int pin);

    // Configura el pin como salida y lo deja apagado.
    void iniciar();

    // Enciende o apaga el relé físicamente, y recuerda el estado actual.
    // Esta función la llaman tanto la lógica automática como las órdenes
    // manuales -- es el único lugar que realmente toca el pin.
    void establecerEstado(bool encendido);

    // Cambia entre modo AUTO y MANUAL.
    void establecerModo(ModoActuador modo);

    bool estaEncendido() const;
    ModoActuador obtenerModo() const;
    const char *obtenerNombre() const;

private:
    const char *_nombre;
    int _pin;
    bool _encendido;
    ModoActuador _modo;
};

#endif // ACTUADOR_H
