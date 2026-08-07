/*
 * ============================================================================
 *  PantallaOLED.cpp
 * ----------------------------------------------------------------------------
 *  Implementación del manejo de la pantalla OLED SH1106 usando U8g2.
 * ============================================================================
 */

#include "PantallaOLED.h"
#include <U8g2lib.h>
#include <Wire.h>
#include "config.h"

// ──────────────────────────────────────────────────────────────────────
// Creación del objeto de la pantalla.
//
// U8G2_SH1106_128X64_NONAME_F_HW_I2C es el constructor específico para:
//   - Controlador: SH1106
//   - Resolución: 128x64
//   - Modo: Full buffer ("_F_") -> usa más RAM pero permite dibujar
//     formas complejas sin parpadeo.
//   - Interfaz: HW_I2C -> usa el bus I2C por hardware del ESP32
//     (pines fijos SDA=21 / SCL=22, los mismos definidos en config.h).
//
// El primer parámetro (U8G2_R0) indica la rotación de pantalla (0 grados).
// El segundo y tercer parámetro son los pines de reset y de selección,
// que en la mayoría de módulos OLED I2C no se usan, por eso van como
// U8X8_PIN_NONE.
// ──────────────────────────────────────────────────────────────────────
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

void PantallaOLED::iniciar()
{
    // Inicia el bus I2C explícitamente en los pines del ESP32.
    // Aunque el ESP32 ya usa 21/22 por defecto, dejarlo explícito
    // evita confusiones si en el futuro se cambia de placa.
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    u8g2.begin();
    u8g2.setI2CAddress(OLED_I2C_ADDR * 2); // U8g2 espera la dirección "shifted" (<<1)
    u8g2.setFont(u8g2_font_6x10_tf);       // Fuente legible y compacta

    Serial.println("[OLED] Pantalla inicializada.");
}

void PantallaOLED::mostrarBienvenida()
{
    u8g2.clearBuffer(); // Limpia el buffer interno (no la pantalla aún)
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(15, 25, "Iniciando...");
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(10, 45, "Proyecto IoT ESP32");
    u8g2.sendBuffer(); // Envía el buffer a la pantalla física
}

void PantallaOLED::mostrarDatos(float temperatura, float humedad, float peso, bool wifiConectado, bool obstaculo, int calidadAire)
{
    u8g2.clearBuffer();

    // WiFi
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, wifiConectado ? "WiFi: OK" : "WiFi: --");
    u8g2.drawHLine(0, 14, 128);

    // Temperatura
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "Temp: %.1f C", temperatura);
    u8g2.drawStr(0, 28, buffer);

    // Humedad
    snprintf(buffer, sizeof(buffer), "Hum:  %.1f %%", humedad);
    u8g2.drawStr(0, 42, buffer);

    // Peso
    snprintf(buffer, sizeof(buffer), "Peso: %.1f g", peso);
    u8g2.drawStr(0, 56, buffer);

    // KY-032 (obstáculo) en la columna derecha
    snprintf(buffer, sizeof(buffer), "Obs: %s", obstaculo ? "SI" : "NO");
    u8g2.drawStr(75, 28, buffer);

    // MQ-135 (calidad del aire)
    snprintf(buffer, sizeof(buffer), "Aire: %d", calidadAire);
    u8g2.drawStr(75, 42, buffer);

    u8g2.sendBuffer();
}

void PantallaOLED::mostrarError(const char* mensaje)
{
    // Pantalla simple de error: útil para detectar fallas de un
    // sensor sin tener que estar mirando el Monitor Serie.
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(5, 25, "ERROR");
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 45, mensaje);
    u8g2.sendBuffer();
}