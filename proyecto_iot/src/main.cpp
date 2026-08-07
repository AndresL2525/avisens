/*
 * ============================================================================
 *  main.cpp
 * ----------------------------------------------------------------------------
 *  Archivo principal del proyecto AVÍSENS.
 *
 *  Su única responsabilidad es ORQUESTAR los módulos: decide CUÁNDO se
 *  llama a cada uno, pero no contiene lógica interna de sensores, pantalla,
 *  actuadores ni de la API. Toda esa lógica vive en sus propios módulos.
 *
 *  Componentes usados:
 *    SENSORES:
 *      - DHT11             -> temperatura y humedad ambiente
 *      - HX711 + celda 1Kg -> peso del alimento en el plato
 *      - KY-032            -> detector de obstáculos por infrarrojos
 *      - MQ-135            -> calidad del aire
 *    SALIDA LOCAL:
 *      - OLED SH1106 I2C   -> visualización local
 *    ACTUADORES (relés):
 *      - Calefactor        -> temperatura baja
 *      - Extractor         -> temperatura alta O aire malo
 *      - Humidificador     -> humedad baja (con histéresis)
 *      - Alimentador       -> motorreductor + tornillo sin fin
 *    NUBE:
 *      - FastAPI (Backend propio) -> sensores, actuadores y eventos
 *
 *  DECISIÓN DE ARQUITECTURA: el ESP32 es AUTÓNOMO. Decide solo cuándo
 *  encender cada actuador según los sensores. Las apps (Android/Web)
 *  solo pueden VISUALIZAR y, opcionalmente, forzar un modo MANUAL que
 *  el ESP32 respeta hasta que se le devuelva el control en modo AUTO.
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
TokenManager tokenManager; // <-- NUEVO: gestor de tokens JWT

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

// Guardamos el estado previo de cada actuador para detectar CAMBIOS y
// generar un evento solo cuando algo realmente cambió (no en cada ciclo).
bool estadoPrevioCalefactor = false;
bool estadoPrevioExtractor = false;
bool estadoPrevioHumidificador = false;

// ──────────────────────────────────────────────────────────────────────
// Genera un evento en la API solo si el estado del actuador cambió
// desde la última vez que se revisó. Evita inundar /eventos con
// registros repetidos del mismo estado en cada ciclo.
// ──────────────────────────────────────────────────────────────────────
void registrarSiCambio(const char *nombreActuador, bool estadoActual, bool &estadoPrevio,
                       const char *mensajeEncendido, const char *mensajeApagado)
{
    if (estadoActual != estadoPrevio)
    {
        const char *mensaje = estadoActual ? mensajeEncendido : mensajeApagado;
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
    Serial.println("\n=== Iniciando AVÍSENS ===");

    // 1. Pantalla primero, para poder mostrar mensajes de progreso
    pantalla.iniciar();
    pantalla.mostrarBienvenida();

    // 2. Sensores
    sensorDHT.iniciar();
    sensorPeso.iniciar(); // OJO: no debe haber peso sobre la celda aquí
    sensorKY032.iniciar(KY032_PIN);
    sensorMQ135.iniciar(MQ135_PIN);

    // 3. Actuadores (arrancan siempre apagados, por seguridad)
    actuadores.iniciar();

    // 4. WiFi (con timeout de 15 segundos, no se bloquea para siempre)
    ConexionWiFi::conectar();

    // 5. Inicializar el gestor de tokens y los servicios de la API
    tokenManager.iniciar();
    ServicioAPI::iniciar(&tokenManager);
    ServicioActuadoresAPI::iniciar(&tokenManager);

    Serial.println("=== Sistema listo ===\n");

    // ────────────────────────────────────────────────────────────────
    // NOTA IMPORTANTE sobre el KY-032:
    // El pin "HABILITAR" (ENABLE) del módulo debe estar conectado a
    // VCC (5V o 3.3V) para que el sensor esté siempre activo.
    //
    // NOTA DE SEGURIDAD sobre los relés:
    // El ESP32 solo controla la BOBINA de cada relé (bajo voltaje).
    // La carga real (bombillo, extractor, etc.) va del lado de alto
    // voltaje del relé, completamente aislado del ESP32.
    // ────────────────────────────────────────────────────────────────
}

// ──────────────────────────────────────────────────────────────────────
// Bucle principal (loop)
// ──────────────────────────────────────────────────────────────────────
void loop()
{
    unsigned long ahora = millis();

    // ────────────────────────────────────────────────────────────────
    // TAREA 1: Leer sensores y EVALUAR actuadores en el mismo ciclo.
    // Los actuadores se evalúan justo después de leer, porque la
    // decisión automática depende de tener los datos más recientes.
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
            // Sensor falló: lo registramos como evento para que quede
            // en el historial y las apps puedan alertar al usuario.
            ServicioActuadoresAPI::registrarEvento(
                "FALLA_SENSOR", "DHT11",
                "Lectura fallida del sensor de temperatura/humedad", "alerta");
        }

        // --- Leer celda de carga ---
        ultimoPeso = sensorPeso.leerPeso();

        // --- Leer KY-032 (detector de obstáculos) ---
        obstaculoDetectado = sensorKY032.detectaObstaculo();

        // --- Leer MQ-135 (calidad del aire) ---
        calidadAireValor = sensorMQ135.leerValorCrudo();
        calidadAireVoltaje = sensorMQ135.leerVoltaje();

        Serial.printf("Temp: %.1f C | Hum: %.1f %% | Peso: %.1f g | Obs: %s | Aire: %d (%0.2fV)\n",
                      ultimaTemperatura, ultimaHumedad, ultimoPeso,
                      obstaculoDetectado ? "SI" : "NO", calidadAireValor, calidadAireVoltaje);

        // --- EVALUAR ACTUADORES con las lecturas que acabamos de tomar ---
        LecturasActuales lecturas;
        lecturas.temperatura = ultimaTemperatura;
        lecturas.humedad = ultimaHumedad;
        lecturas.calidadAire = calidadAireValor;
        lecturas.pesoPlato = ultimoPeso;

        actuadores.evaluar(lecturas);

        // --- Generar eventos solo si algún actuador CAMBIÓ de estado ---
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
    // TAREA 3: Enviar lecturas de sensores a la API (FastAPI)
    // ────────────────────────────────────────────────────────────────
    if (ahora - ultimoEnvioAPI >= INTERVALO_ENVIO_FIREBASE) // mantenemos mismo intervalo
    {
        ultimoEnvioAPI = ahora;

        ServicioAPI::enviarLecturaCompleta(
            ultimaTemperatura, ultimaHumedad, ultimoPeso,
            obstaculoDetectado, calidadAireValor, calidadAireVoltaje);

        // Reportamos también el estado de los actuadores en el mismo
        // intervalo, para que las apps siempre vean ambos sincronizados.
        ServicioActuadoresAPI::reportarEstadoActuadores(actuadores);
    }

    // ────────────────────────────────────────────────────────────────
    // TAREA 4: Sincronizar órdenes MANUALES desde la API (BAJAR datos)
    // Esta es la única tarea que hace GET en vez de PUT/POST -- es
    // cómo el ESP32 se entera si una app cambió el modo a MANUAL o
    // envió una orden de encendido/apagado.
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
                if (!comandos[i].valido)
                    continue;

                // Aplicar comando según el nombre
                if (comandos[i].nombre == "calefactor")
                {
                    actuadores.establecerModoCalefactor(
                        comandos[i].modo == "MANUAL" ? ModoActuador::MANUAL : ModoActuador::AUTO);
                    if (comandos[i].modo == "MANUAL")
                    {
                        actuadores.ordenManualCalefactor(comandos[i].ordenManual);
                    }
                }
                else if (comandos[i].nombre == "extractor")
                {
                    actuadores.establecerModoExtractor(
                        comandos[i].modo == "MANUAL" ? ModoActuador::MANUAL : ModoActuador::AUTO);
                    if (comandos[i].modo == "MANUAL")
                    {
                        actuadores.ordenManualExtractor(comandos[i].ordenManual);
                    }
                }
                else if (comandos[i].nombre == "humidificador")
                {
                    actuadores.establecerModoHumidificador(
                        comandos[i].modo == "MANUAL" ? ModoActuador::MANUAL : ModoActuador::AUTO);
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

                // Confirmar que ejecutamos el comando
                ServicioActuadoresAPI::confirmarComandoEjecutado(comandos[i].id);
            }
        }
    }

    // Nota: NO se usa delay() en el loop principal. Usar millis() de esta
    // forma permite que las tareas corran "en paralelo" sin bloquearse
    // entre sí.
}