#include "SensorMQ135.h"

void SensorMQ135::iniciar(int pinADC)
{
    _pinADC = pinADC;
    // El pin ADC no necesita configurarse como INPUT, pero lo dejamos claro
    pinMode(_pinADC, INPUT);
    Serial.println("[MQ135] Sensor de calidad del aire inicializado.");
    Serial.println("[MQ135] NOTA: El sensor necesita precalentamiento de 24h.");
}

int SensorMQ135::leerValorCrudo()
{
    return analogRead(_pinADC); // 0-4095
}

float SensorMQ135::leerVoltaje()
{
    // 3.3V es el voltaje de referencia del ESP32
    return (analogRead(_pinADC) / 4095.0) * 3.3;
}

float SensorMQ135::leerPPM()
{
    // Esta es una fórmula genérica. REQUIERE CALIBRACIÓN con gas conocido.
    // Devuelve un valor aproximado en ppm (partes por millón)
    // Para el MQ-135, la relación es: ppm = 10^((Vout - Vref) / pendiente)
    // Como no tenemos calibración, devolvemos el voltaje como indicador.
    float voltaje = leerVoltaje();
    // Fórmula empírica para amoniaco (NH3) – solo referencia
    // ppm = 10^((voltaje * 3.3 - 0.2) / 0.15)   (ejemplo, no usar en producción)
    // Retornamos el voltaje para que el usuario pueda monitorear cambios.
    return voltaje * 100; // Escala ficticia para ver variaciones
}