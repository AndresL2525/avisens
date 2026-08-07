/*
 * ============================================================================
 *  PantallaOLED.h
 * ----------------------------------------------------------------------------
 *  Módulo encargado ÚNICAMENTE de mostrar información en la pantalla OLED
 *  SH1106 de 128x64 píxeles, conectada por I2C.
 *
 *  Se usa la librería U8g2 porque tiene soporte nativo y muy estable
 *  para el controlador SH1106 (distinto del más común SSD1306; aunque
 *  se ven casi iguales, el controlador interno es diferente y usar la
 *  librería equivocada genera una imagen distorsionada o en blanco).
 * ============================================================================
 */

#ifndef PANTALLA_OLED_H
#define PANTALLA_OLED_H

#include <Arduino.h>

class PantallaOLED
{
public:
    // Inicializa la comunicación I2C y la pantalla.
    void iniciar();

    // Muestra una pantalla de arranque/bienvenida (opcional, pero útil
    // para confirmar visualmente que el sistema inició bien).
    void mostrarBienvenida();

    // Muestra los datos principales de los sensores: temperatura,
    // humedad y peso. Esta es la pantalla "normal" de operación.
    void mostrarDatos(float temperatura, float humedad, float peso, bool wifiConectado, bool obstaculo, int calidadAire);
    // Muestra un mensaje de error genérico (por ejemplo, si un sensor
    // falló) para que el operario lo note de inmediato sin necesidad
    // de revisar el Monitor Serie.
    void mostrarError(const char *mensaje);
};

#endif // PANTALLA_OLED_H
