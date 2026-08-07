#ifndef CONTROL_SERVO_H
#define CONTROL_SERVO_H

#include <Arduino.h>

class ControlServo
{
public:
    void iniciar();

    void abrir();

    void cerrar();

private:
    bool abierto = false;
};

#endif