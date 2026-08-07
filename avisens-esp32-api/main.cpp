/*
 * ============================================================================
 * main.cpp — VERSIÓN FASTAPI
 * ----------------------------------------------------------------------------
 * Este es el main.cpp MODIFICADO para usar el backend FastAPI en vez de
 * Firebase. Conserva toda la arquitectura original (no bloqueante, 4 tareas).
 * 
 * CAMBIOS REALIZADOS:
 *   1. Reemplazados ServicioFirebase → ServicioAPI
 *   2. Reemplazados ServicioActuadoresFirebase → ServicioActuadoresAPI
 *   3. Agregado TokenManager para persistir JWT en flash
 *   4. TAREA 4 ahora lee comandos pendientes como array (más eficiente)
 *   5. Agregada confirmación de ejecución de comandos
 * ============================================================================
 */

#include <Arduino.h>
#include "config.h"
#include "ConexionWiFi.h"
#include "SensorDHT.h"
#include "SensorPeso.h"
#include "SensorKY032.h"
#include "SensorMQ135.h"
#include "PantallaOLED.h"
#include "GestorActuadores.h"

// === NUEVO: Backend FastAPI ===
#include "TokenManager.h"
#include "ServicioAPI.h"
#include "ServicioActuadoresAPI.h"

// ──────────────────────────────────────────────────────────────────────
// Instancias de cada módulo.
// ──────────────────────────────────────────────────────────────────────
SensorDHT sensorDHT;
SensorPeso sensorPeso;
SensorKY032 sensorKY032;
SensorMQ135 sensorMQ135;
PantallaOLED pantalla;
GestorActuadores actuadores;

// === NUEVO: TokenManager ===
TokenManager tokenManager;

// ──────────────────────────────────────────────────────────────────────
// Variables de control de tiempo (sin delay() bloqueante).
// ──────────────────────────────────────────────────────────────────────
unsigned long ultimaLecturaSensores = 0;
unsigned long ultimoEnvioAPI = 0;
unsigned long ultimoRefrescoPantalla = 0;
unsigned long ultimaSincronizacionRemota = 0;

const unsigned long INTERVALO_SINCRONIZACION_REMOTA = 10000; // 10 segundos

// Última lectura válida de cada sensor.
float ultimaTemperatura = 0.0;
float ultimaHumedad = 0.0;
float ultimoPeso = 0.0;
bool obstaculoDetectado = false;
int calidadAireValor = 0;
float calidadAireVoltaje = 0.0;

// Guardamos el estado previo de cada actuador para detectar CAMBIOS.
bool estadoPrevioCalefactor = false;
bool estadoPrevioExtractor = false;
bool estadoPrevioHumidificador = false;

// ──────────────────────────────────────────────────────────────────────
// Genera un evento SOLO si el estado del actuador cambió.
// ──────────────────────────────────────────────────────────────────────
void registrarSiCambio(const char *nombreActuador, bool estadoActual, bool &estadoPrevio,
                       const char *mensajeEncendido, const char *mensajeApagado)
{
    if (estadoActual != estadoPrevio)
    {
        const char *mensaje = estadoActual ? mensajeEncendido : mensajeApagado;
        // === CAMBIO: usamos ServicioActuadoresAPI en vez de Firebase ===
        ServicioActuadoresAPI::registrarEvento("ACTUADOR", nombreActuador, mensaje, "info");
        estadoPrevio = estadoActual;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Configuración inicial (setup)
// ──────────────────────────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    Serial.println("\n=== Iniciando AVÍSENS (FastAPI Backend) ===");

    // 1. Pantalla primero
    pantalla.iniciar();
    pantalla.mostrarBienvenida();

    // 2. Sensores
    sensorDHT.iniciar();
    sensorPeso.iniciar();
    sensorKY032.iniciar(KY032_PIN);
    sensorMQ135.iniciar(MQ135_PIN);

    // 3. Actuadores
    actuadores.iniciar();

    // 4. WiFi
    ConexionWiFi::conectar();

    // === NUEVO: Inicializar servicios de API ===
    tokenManager.iniciar();
    ServicioAPI::iniciar(&tokenManager);
    ServicioActuadoresAPI::iniciar(&tokenManager);

    Serial.println("=== Sistema listo ===\n");
}

// ──────────────────────────────────────────────────────────────────────
// Bucle principal (loop)
// ──────────────────────────────────────────────────────────────────────
void loop()
{
    unsigned long ahora = millis();

    // ────────────────────────────────────────────────────────────────
    // TAREA 1: Leer sensores y EVALUAR actuadores
    // ────────────────────────────────────────────────────────────────
    if (ahora - ultimaLecturaSensores >= INTERVALO_LECTURA_SENSORES)
    {
        ultimaLecturaSensores = ahora;

        // --- Leer DHT11 ---
        LecturaDHT lecturaDHT = sensorDHT.leer();
        if (lecturaDHT.valida)
        {
            ultimaTemperatura = lecturaDHT.temperatura;
            ultimaHumedad = lecturaDHT.humedad;
        }
        else
        {
            ServicioActuadoresAPI::registrarEvento(
                "FALLA_SENSOR", "DHT11",
                "Lectura fallida del sensor de temperatura/humedad", "alerta");
        }

        // --- Leer celda de carga ---
        ultimoPeso = sensorPeso.leerPeso();

        // --- Leer KY-032 ---
        obstaculoDetectado = sensorKY032.detectaObstaculo();

        // --- Leer MQ-135 ---
        calidadAireValor = sensorMQ135.leerValorCrudo();
        calidadAireVoltaje = sensorMQ135.leerVoltaje();

        Serial.printf("Temp: %.1f C | Hum: %.1f %% | Peso: %.1f g | Obs: %s | Aire: %d (%.2fV)\n",
                      ultimaTemperatura, ultimaHumedad, ultimoPeso,
                      obstaculoDetectado ? "SI" : "NO", calidadAireValor, calidadAireVoltaje);

        // --- EVALUAR ACTUADORES ---
        LecturasActuales lecturas;
        lecturas.temperatura = ultimaTemperatura;
        lecturas.humedad = ultimaHumedad;
        lecturas.calidadAire = calidadAireValor;
        lecturas.pesoPlato = ultimoPeso;

        actuadores.evaluar(lecturas);

        // --- Eventos solo si CAMBIÓ ---
        registrarSiCambio("calefactor", actuadores.calefactor().estaEncendido(), estadoPrevioCalefactor,
                          "Calefactor activado: temperatura por debajo del umbral",
                          "Calefactor desactivado: temperatura normalizada");

        registrarSiCambio("extractor", actuadores.extractor().estaEncendido(), estadoPrevioExtractor,
                          "Extractor activado: temperatura alta o calidad de aire baja",
                          "Extractor desactivado: condiciones normalizadas");

        registrarSiCambio("humidificador", actuadores.humidificador().estaEncendido(), estadoPrevioHumidificador,
                          "Humidificador activado: humedad por debajo del umbral",
                          "Humidificador desactivado: humedad normalizada");
    }

    // ────────────────────────────────────────────────────────────────
    // TAREA 2: Refrescar la pantalla OLED
    // ────────────────────────────────────────────────────────────────
    if (ahora - ultimoRefrescoPantalla >= INTERVALO_REFRESCO_PANTALLA)
    {
        ultimoRefrescoPantalla = ahora;

        pantalla.mostrarDatos(
            ultimaTemperatura, ultimaHumedad, ultimoPeso,
            ConexionWiFi::estaConectado(), obstaculoDetectado, calidadAireValor);
    }

    // ────────────────────────────────────────────────────────────────
    // TAREA 3: Enviar lecturas al backend FastAPI
    // ────────────────────────────────────────────────────────────────
    if (ahora - ultimoEnvioAPI >= INTERVALO_ENVIO_FIREBASE)
    {
        ultimoEnvioAPI = ahora;

        // === CAMBIO: usamos ServicioAPI en vez de Firebase ===
        ServicioAPI::enviarLecturaCompleta(
            ultimaTemperatura, ultimaHumedad, ultimoPeso,
            obstaculoDetectado, calidadAireValor, calidadAireVoltaje);

        ServicioActuadoresAPI::reportarEstadoActuadores(actuadores);
    }

    // ────────────────────────────────────────────────────────────────
    // TAREA 4: Sincronizar órdenes MANUALES desde el backend
    // === CAMBIO COMPLETO: ahora leemos array de comandos pendientes ===
    // ────────────────────────────────────────────────────────────────
    if (ahora - ultimaSincronizacionRemota >= INTERVALO_SINCRONIZACION_REMOTA)
    {
        ultimaSincronizacionRemota = ahora;

        ComandoRemoto comandos[4];
        int cantidad = 0;

        if (ServicioActuadoresAPI::leerComandosPendientes(comandos, 4, cantidad))
        {
            for (int i = 0; i < cantidad; i++)
            {
                if (!comandos[i].valido) continue;

                // Aplicar comando según nombre
                if (comandos[i].nombre == "calefactor")
                {
                    actuadores.establecerModoCalefactor(
                        comandos[i].modo == "MANUAL" ? ModoActuador::MANUAL : ModoActuador::AUTO
                    );
                    if (comandos[i].modo == "MANUAL")
                    {
                        actuadores.ordenManualCalefactor(comandos[i].ordenManual);
                    }
                }
                else if (comandos[i].nombre == "extractor")
                {
                    actuadores.establecerModoExtractor(
                        comandos[i].modo == "MANUAL" ? ModoActuador::MANUAL : ModoActuador::AUTO
                    );
                    if (comandos[i].modo == "MANUAL")
                    {
                        actuadores.ordenManualExtractor(comandos[i].ordenManual);
                    }
                }
                else if (comandos[i].nombre == "humidificador")
                {
                    actuadores.establecerModoHumidificador(
                        comandos[i].modo == "MANUAL" ? ModoActuador::MANUAL : ModoActuador::AUTO
                    );
                    if (comandos[i].modo == "MANUAL")
                    {
                        actuadores.ordenManualHumidificador(comandos[i].ordenManual);
                    }
                }
                else if (comandos[i].nombre == "alimentador")
                {
                    if (comandos[i].ordenManual)
                    {
                        actuadores.solicitarDosisManualAlimentador();
                    }
                }

                // === NUEVO: Confirmamos al backend que ejecutamos el comando ===
                ServicioActuadoresAPI::confirmarComandoEjecutado(comandos[i].id);
            }
        }
    }
}
