# 🏗️ Arquitectura Técnica — Galpón Inteligente

Documento de diseño arquitectónico para la automatización del galpón avícola.

---

## 1. Introducción Arquitectónica

### Objetivos de Diseño

✅ **Modularidad**: Cada componente es independiente y reutilizable  
✅ **No-bloqueante**: Operación en tiempo real con FreeRTOS  
✅ **Robustez**: Fail-safe automático ante fallos de sensores  
✅ **Escalabilidad**: Fácil agregar nuevos sensores/actuadores  
✅ **Mantenibilidad**: Código limpio, documentado, industrial  

### Decisiones Clave

| Decisión | Razón | Trade-off |
|----------|-------|-----------|
| **Dos cores** | Core 0 para sensores críticos, Core 1 para WiFi | Mayor complejidad FreeRTOS |
| **Máquinas de Estado** | Eliminar bloqueos, control predecible | Más código, más estados |
| **Filtro media móvil** | Reducir ruido en sensores analógicos | Latencia de 10 muestras (~20ms) |
| **Watchdog Timer** | Detectar deadlocks de tarea | Reinicio forzado si no responde |
| **Histéresis en bomba** | Evitar oscilación ON/OFF continua | Rangos separados (6cm vs 3cm) |

---

## 2. Capas Arquitectónicas

```
┌────────────────────────────────────────────────────────┐
│  Capa de Aplicación (main.cpp)                         │
│  ├─ Máquina de estado global                          │
│  ├─ Orquestación de tareas                            │
│  └─ Watchdog Timer                                    │
└────────────────────────────────────────────────────────┘
         ↓
┌────────────────────────────────────────────────────────┐
│  Capa de Servicios (GestorActuadores)                  │
│  ├─ Lógica de control (umbrales)                      │
│  ├─ Histéresis de bomba                               │
│  └─ Fail-safe coordinado                              │
└────────────────────────────────────────────────────────┘
         ↓
┌────────────────────────────────────────────────────────┐
│  Capa de Dispositivos (Sensores + Actuadores)          │
│  ├─ SensorDHT, SensorMQ135, SensorUltrasonico         │
│  ├─ ControlServo, Alimentador, Persiana              │
│  └─ Cada uno con su FSM privada                      │
└────────────────────────────────────────────────────────┘
         ↓
┌────────────────────────────────────────────────────────┐
│  Capa de Periféricos (GPIO, ADC, PWM)                  │
│  ├─ ESP32 pins, interfaces hardware                   │
│  └─ Filtrado analógico (MovingAverage)                │
└────────────────────────────────────────────────────────┘
```

---

## 3. Máquinas de Estado Finitas (Modelo Mealy)

En el modelo de Mealy, las salidas del sistema se generan a partir de la interacción instantánea entre el **Estado Actual ($Q_t$)** y las **Entradas presentes ($X$)**, permitiendo una reacción inmediata sin esperar a que concluya el ciclo de reloj. 

Notación formal en transiciones: **`Entradas (X) / Salidas (Y)`**

---

### 3.1 FSM del Sistema Global (`main.cpp`)

Coordina el ciclo de vida del microcontrolador, la tara inicial y el disparo del protocolo Fail-Safe.

**Variables de Entrada ($X$):**
* $C$: Contador de ciclos de arranque iniciales ($1$: $\ge 10\text{ ciclos}$, $0$: En proceso).
* $T$: Calibración / Tara de celda HX711 completada o timeout ($1$: Listo, $0$: Pendiente).
* $F$: Fallos acumulados de sensores críticos ($1$: $\ge 3\text{ fallos}$, $0$: Sensores OK).
* $R$: Comando de rearme manual / Reset ($1$: Activo, $0$: Inactivo).

**Salidas del Sistema ($Y_1 Y_0$):**
* $00$: Modo Configuración / Standby.
* $01$: Operación Normal (Lazo cerrado de lectura y control activo).
* $10$: **Fail-Safe Activo** (Relés K1, K2, K3 apagados en `HIGH`; Bomba K4 encendida en `LOW` por seguridad).

<svg viewBox="0 0 650 380" width="100%" xmlns="http://www.w3.org/2000/svg" style="background: transparent; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;">
  <defs>
    <marker id="arr1" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="6" markerHeight="6" orient="auto-angle">
      <path d="M 0 1.5 L 8 5 L 0 8.5 z" fill="#1e293b" />
    </marker>
    <style>
      .state { fill: #ffffff; stroke: #1e293b; stroke-width: 2.5; }
      .state-txt { font-size: 16px; font-weight: bold; fill: #1e293b; text-anchor: middle; dominant-baseline: central; }
      .trans { fill: none; stroke: #1e293b; stroke-width: 1.8; }
      .lbl { font-size: 13px; font-weight: 600; fill: #0f172a; text-anchor: middle; }
      .lbl-bg { fill: #ffffff; opacity: 0.95; }
    </style>
  </defs>

  <!-- Nodos -->
  <circle cx="160" cy="110" r="34" class="state" />
  <text x="160" y="110" class="state-txt">S₀: INIT</text>

  <circle cx="480" cy="110" r="34" class="state" />
  <text x="480" y="110" class="state-txt">S₁: CALIB</text>

  <circle cx="480" cy="270" r="34" class="state" />
  <text x="480" y="270" class="state-txt">S₂: MONIT</text>

  <circle cx="160" cy="270" r="34" class="state" />
  <text x="160" y="270" class="state-txt">S₃: ERROR</text>

  <!-- Self-Loops -->
  <path d="M 140 78 C 120 20, 200 20, 180 78" class="trans" marker-end="url(#arr1)" />
  <text x="160" y="28" class="lbl">C=0 / 00</text>

  <path d="M 512 88 C 570 70, 570 150, 512 132" class="trans" marker-end="url(#arr1)" />
  <text x="575" y="114" class="lbl">T=0 / 00</text>

  <path d="M 512 250 C 570 230, 570 310, 512 292" class="trans" marker-end="url(#arr1)" />
  <text x="575" y="274" class="lbl">F=0 / 01</text>

  <path d="M 128 292 C 70 310, 70 230, 128 250" class="trans" marker-end="url(#arr1)" />
  <text x="65" y="274" class="lbl">R=0 / 10</text>

  <!-- Transiciones -->
  <path d="M 194 110 L 446 110" class="trans" marker-end="url(#arr1)" />
  <rect x="295" y="93" width="55" height="18" class="lbl-bg" rx="3" />
  <text x="320" y="106" class="lbl">C=1 / 00</text>

  <path d="M 480 144 L 480 236" class="trans" marker-end="url(#arr1)" />
  <rect x="420" y="181" width="55" height="18" class="lbl-bg" rx="3" />
  <text x="448" y="194" class="lbl">T=1 / 01</text>

  <path d="M 446 270 L 194 270" class="trans" marker-end="url(#arr1)" />
  <rect x="295" y="253" width="55" height="18" class="lbl-bg" rx="3" />
  <text x="320" y="266" class="lbl">F=1 / 10</text>

  <path d="M 160 236 L 160 144" class="trans" marker-end="url(#arr1)" />
  <rect x="165" y="181" width="60" height="18" class="lbl-bg" rx="3" />
  <text x="195" y="194" class="lbl">R=1 / 00</text>
</svg>

| Estado Actual ($Q_1 Q_0$) | Entradas ($C, T, F, R$) | Estado Siguiente ($D_1 D_0$) | Salidas ($Y_1 Y_0$) | Acción FSM / Periféricos |
| :---: | :---: | :---: | :---: | :--- |
| **$S_0$: INIT (00)** | $0 - - -$ | **$S_0$ (00)** | **$00$** | Espera de estabilización del hardware. |
| **$S_0$: INIT (00)** | $1 - - -$ | **$S_1$ (01)** | **$00$** | Inicia rutina de tara para celda HX711. |
| **$S_1$: CALIB (01)** | $- 0 - -$ | **$S_1$ (01)** | **$00$** | Espera comando `TARA` o timeout de 15 s. |
| **$S_1$: CALIB (01)** | $- 1 - -$ | **$S_2$ (10)** | **$01$** | Tara finalizada $\rightarrow$ Habilita lazo de control. |
| **$S_2$: MONIT (10)** | $- - 0 -$ | **$S_2$ (10)** | **$01$** | Muestreo regular cada 2 s y control de relés. |
| **$S_2$: MONIT (10)** | $- - 1 -$ | **$S_3$ (11)** | **$10$** | **Disparo de Fail-Safe:** Desactiva K1-K3, activa K4. |
| **$S_3$: ERROR (11)** | $- - - 0$ | **$S_3$ (11)** | **$10$** | Mantiene actuadores en posición segura. |
| **$S_3$: ERROR (11)** | $- - - 1$ | **$S_0$ (00)** | **$00$** | Rearme del sistema por comando `RESET`. |

---

### 3.2 FSM de Puerta (`ControlServo`)

Controla el servomotor de acceso basándose en la detección de presencia del sensor infrarrojo KY-032.

**Variables de Entrada ($X$):**
* $P$: Entrada del sensor KY-032 ($0$: Presencia detectada / LOW, $1$: Despejado / HIGH).
* $T_{300}$: Timer de rotación angular cumplido ($1$: $t \ge 300\text{ ms}$, $0$: $t < 300\text{ ms}$).
* $T_{2s}$: Timer de apertura mantenida cumplido ($1$: $t \ge 2000\text{ ms}$, $0$: $t < 2000\text{ ms}$).

**Salidas Mealy ($Y_1 Y_0$ - Posición del Servo):**
* $00$: **Neutro / Reposo** ($93^\circ$ - Detiene el giro).
* $01$: **Giro Sentido Apertura** ($0^\circ$).
* $10$: **Giro Sentido Cierre** ($180^\circ$).

<svg viewBox="0 0 650 380" width="100%" xmlns="http://www.w3.org/2000/svg" style="background: transparent; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;">
  <defs>
    <marker id="arr2" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="6" markerHeight="6" orient="auto-angle">
      <path d="M 0 1.5 L 8 5 L 0 8.5 z" fill="#1e293b" />
    </marker>
  </defs>

  <!-- Nodos -->
  <circle cx="160" cy="110" r="34" class="state" />
  <text x="160" y="110" class="state-txt">S₀: CERR</text>

  <circle cx="480" cy="110" r="34" class="state" />
  <text x="480" y="110" class="state-txt">S₁: ABRI</text>

  <circle cx="480" cy="270" r="34" class="state" />
  <text x="480" y="270" class="state-txt">S₂: ABIER</text>

  <circle cx="160" cy="270" r="34" class="state" />
  <text x="160" y="270" class="state-txt">S₃: CIERR</text>

  <!-- Self-Loops -->
  <path d="M 140 78 C 120 20, 200 20, 180 78" class="trans" marker-end="url(#arr2)" />
  <text x="160" y="28" class="lbl">P=1 / 00</text>

  <path d="M 512 88 C 570 70, 570 150, 512 132" class="trans" marker-end="url(#arr2)" />
  <text x="580" y="114" class="lbl">T₃₀₀=0 / 01</text>

  <path d="M 512 250 C 570 230, 570 310, 512 292" class="trans" marker-end="url(#arr2)" />
  <text x="580" y="274" class="lbl">T₂ₛ=0 / 00</text>

  <path d="M 128 292 C 70 310, 70 230, 128 250" class="trans" marker-end="url(#arr2)" />
  <text x="60" y="274" class="lbl">T₃₀₀=0 / 10</text>

  <!-- Transiciones -->
  <path d="M 194 110 L 446 110" class="trans" marker-end="url(#arr2)" />
  <rect x="290" y="93" width="60" height="18" class="lbl-bg" rx="3" />
  <text x="320" y="106" class="lbl">P=0 / 01</text>

  <path d="M 480 144 L 480 236" class="trans" marker-end="url(#arr2)" />
  <rect x="405" y="181" width="70" height="18" class="lbl-bg" rx="3" />
  <text x="440" y="194" class="lbl">T₃₀₀=1 / 00</text>

  <path d="M 446 270 L 194 270" class="trans" marker-end="url(#arr2)" />
  <rect x="290" y="253" width="60" height="18" class="lbl-bg" rx="3" />
  <text x="320" y="266" class="lbl">T₂ₛ=1 / 10</text>

  <path d="M 160 236 L 160 144" class="trans" marker-end="url(#arr2)" />
  <rect x="165" y="181" width="75" height="18" class="lbl-bg" rx="3" />
  <text x="202" y="194" class="lbl">T₃₀₀=1 / 00</text>
</svg>

| Estado Actual ($Q_1 Q_0$) | Entradas ($P, T_{300}, T_{2s}$) | Estado Siguiente ($D_1 D_0$) | Salidas ($Y_1 Y_0$) | Acción Física del Servomotor |
| :---: | :---: | :---: | :---: | :--- |
| **$S_0$: CERRADA (00)** | $1 - -$ | **$S_0$ (00)** | **$00$** | Sin presencia $\rightarrow$ Servo detenido en reposo ($93^\circ$). |
| **$S_0$: CERRADA (00)** | $0 - -$ | **$S_1$ (01)** | **$01$** | Presencia detectada $\rightarrow$ Gira a abrir ($0^\circ$). |
| **$S_1$: ABRIENDO (01)** | $- 0 -$ | **$S_1$ (01)** | **$01$** | Temporizador $< 300\text{ ms} \rightarrow$ Continúa giro ($0^\circ$). |
| **$S_1$: ABRIENDO (01)** | $- 1 -$ | **$S_2$ (10)** | **$00$** | $300\text{ ms}$ alcanzados $\rightarrow$ Detiene motor en $93^\circ$. |
| **$S_2$: ABIERTA (10)** | $- - 0$ | **$S_2$ (10)** | **$00$** | Puerta abierta en espera de paso ($t < 2\text{ s}$). |
| **$S_2$: ABIERTA (10)** | $- - 1$ | **$S_3$ (11)** | **$10$** | Tiempo expirado $\rightarrow$ Gira a cerrar ($180^\circ$). |
| **$S_3$: CERRANDO (11)** | $- 0 -$ | **$S_3$ (11)** | **$10$** | Temporizador $< 300\text{ ms} \rightarrow$ Continúa giro ($180^\circ$). |
| **$S_3$: CERRANDO (11)** | $- 1 -$ | **$S_0$ (00)** | **$00$** | Cierre completado $\rightarrow$ Reposo neutro ($93^\circ$). |

---

### 3.3 FSM de Persiana (`Persiana`)

Gestiona el ciclo de renovación de aire mediante el puente H L293D (Canal A).

**Variables de Entrada ($X$):**
* $T_{5m}$: Temporizador de intervalo entre ciclos ($1$: $t \ge 5\text{ min}$, $0$: En espera).
* $T_{3s}$: Temporizador de recorrido mecánico ($1$: $t \ge 3\text{ s}$, $0$: En movimiento).
* $T_{500}$: Temporizador de amortiguación/pausa ($1$: $t \ge 500\text{ ms}$, $0$: En pausa).

**Salidas Mealy ($Y_1 Y_0$ - Líneas de control L293D Canal A: $\{EN1, IN1, IN2\}$):**
* $00$: **Motor Parado** (`EN1=0, IN1=0, IN2=0`).
* $01$: **Giro Adelante / Apertura** (`EN1=1, IN1=1, IN2=0`).
* $10$: **Giro Atrás / Cierre** (`EN1=1, IN1=0, IN2=1`).

<svg viewBox="0 0 650 380" width="100%" xmlns="http://www.w3.org/2000/svg" style="background: transparent; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;">
  <defs>
    <marker id="arr3" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="6" markerHeight="6" orient="auto-angle">
      <path d="M 0 1.5 L 8 5 L 0 8.5 z" fill="#1e293b" />
    </marker>
  </defs>

  <!-- Nodos -->
  <circle cx="160" cy="110" r="34" class="state" />
  <text x="160" y="110" class="state-txt">S₀: QUIET</text>

  <circle cx="480" cy="110" r="34" class="state" />
  <text x="480" y="110" class="state-txt">S₁: ABRI</text>

  <circle cx="480" cy="270" r="34" class="state" />
  <text x="480" y="270" class="state-txt">S₂: PAUS</text>

  <circle cx="160" cy="270" r="34" class="state" />
  <text x="160" y="270" class="state-txt">S₃: CIERR</text>

  <!-- Self-Loops -->
  <path d="M 140 78 C 120 20, 200 20, 180 78" class="trans" marker-end="url(#arr3)" />
  <text x="160" y="28" class="lbl">T₅ₘ=0 / 00</text>

  <path d="M 512 88 C 570 70, 570 150, 512 132" class="trans" marker-end="url(#arr3)" />
  <text x="580" y="114" class="lbl">T₃ₛ=0 / 01</text>

  <path d="M 512 250 C 570 230, 570 310, 512 292" class="trans" marker-end="url(#arr3)" />
  <text x="585" y="274" class="lbl">T₅₀₀=0 / 00</text>

  <path d="M 128 292 C 70 310, 70 230, 128 250" class="trans" marker-end="url(#arr3)" />
  <text x="60" y="274" class="lbl">T₃ₛ=0 / 10</text>

  <!-- Transiciones -->
  <path d="M 194 110 L 446 110" class="trans" marker-end="url(#arr3)" />
  <rect x="290" y="93" width="60" height="18" class="lbl-bg" rx="3" />
  <text x="320" y="106" class="lbl">T₅ₘ=1 / 01</text>

  <path d="M 480 144 L 480 236" class="trans" marker-end="url(#arr3)" />
  <rect x="415" y="181" width="60" height="18" class="lbl-bg" rx="3" />
  <text x="445" y="194" class="lbl">T₃ₛ=1 / 00</text>

  <path d="M 446 270 L 194 270" class="trans" marker-end="url(#arr3)" />
  <rect x="285" y="253" width="70" height="18" class="lbl-bg" rx="3" />
  <text x="320" y="266" class="lbl">T₅₀₀=1 / 10</text>

  <path d="M 160 236 L 160 144" class="trans" marker-end="url(#arr3)" />
  <rect x="165" y="181" width="65" height="18" class="lbl-bg" rx="3" />
  <text x="198" y="194" class="lbl">T₃ₛ=1 / 00</text>
</svg>

| Estado Actual ($Q_1 Q_0$) | Entradas ($T_{5m}, T_{3s}, T_{500}$) | Estado Siguiente ($D_1 D_0$) | Salidas ($Y_1 Y_0$) | Estado Físico del Motor |
| :---: | :---: | :---: | :---: | :--- |
| **$S_0$: QUIETA (00)** | $0 - -$ | **$S_0$ (00)** | **$00$** | En reposo (espera de 5 minutos). |
| **$S_0$: QUIETA (00)** | $1 - -$ | **$S_1$ (01)** | **$01$** | Intervalo cumplido $\rightarrow$ Inicia apertura de persiana. |
| **$S_1$: ABRIENDO (01)** | $- 0 -$ | **$S_1$ (01)** | **$01$** | Desplazamiento activo ($t < 3\text{ s}$). |
| **$S_1$: ABRIENDO (01)** | $- 1 -$ | **$S_2$ (10)** | **$00$** | Apertura completada $\rightarrow$ Frena motor. |
| **$S_2$: PAUSA (10)** | $- - 0$ | **$S_2$ (10)** | **$00$** | Pausa intermedia para evitar picos de corriente inductiva. |
| **$S_2$: PAUSA (10)** | $- - 1$ | **$S_3$ (11)** | **$10$** | Pausa finalizada $\rightarrow$ Inicia cierre de persiana. |
| **$S_3$: CERRANDO (11)** | $- 0 -$ | **$S_3$ (11)** | **$10$** | Desplazamiento activo de retorno ($t < 3\text{ s}$). |
| **$S_3$: CERRANDO (11)** | $- 1 -$ | **$S_0$ (00)** | **$00$** | Fin del recorrido $\rightarrow$ Motor detenido en espera. |

---

### 3.4 FSM del Tornillo Sinfín / Alimentador (`Alimentador`)

Controla la dosificación periódica de alimento balanceado accionando el canal PWM del motor (Canal B).

**Variables de Entrada ($X$):**
* $T_{5m}$: Temporizador de ciclo ($1$: $t \ge 5\text{ min}$, $0$: $t < 5\text{ min}$).
* $E$: Bandera lógica de habilitación general ($1$: Habilitado, $0$: Bloqueado por seguridad o paro).

**Salida Mealy ($Y$ - Comando de Potencia PWM L293D Canal B: $\{EN2, IN3, IN4\}$):**
* $0$: **Apagado** (`EN2=0, IN3=0, IN4=0`).
* $1$: **Encendido al 50% PWM** (`EN2=128, IN3=1, IN4=0`).

<svg viewBox="0 0 520 220" width="100%" xmlns="http://www.w3.org/2000/svg" style="background: transparent; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;">
  <defs>
    <marker id="arr4" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="6" markerHeight="6" orient="auto-angle">
      <path d="M 0 1.5 L 8 5 L 0 8.5 z" fill="#1e293b" />
    </marker>
  </defs>

  <!-- Nodos -->
  <circle cx="120" cy="110" r="34" class="state" />
  <text x="120" y="110" class="state-txt">S₀: OFF</text>

  <circle cx="400" cy="110" r="34" class="state" />
  <text x="400" y="110" class="state-txt">S₁: ON</text>

  <!-- Self-Loops -->
  <path d="M 98 88 C 40 70, 40 150, 98 132" class="trans" marker-end="url(#arr4)" />
  <text x="35" y="114" class="lbl">T'=1 + E'=1 / 0</text>

  <path d="M 422 88 C 480 70, 480 150, 422 132" class="trans" marker-end="url(#arr4)" />
  <text x="490" y="114" class="lbl">T=0 · E=1 / 1</text>

  <!-- Transiciones -->
  <!-- S0 -> S1 (Arriba curvado) -->
  <path d="M 152 98 C 240 50, 280 50, 368 98" class="trans" marker-end="url(#arr4)" />
  <rect x="225" y="55" width="70" height="18" class="lbl-bg" rx="3" />
  <text x="260" y="68" class="lbl">T=1 · E=1 / 1</text>

  <!-- S1 -> S0 (Abajo curvado) -->
  <path d="M 368 122 C 280 170, 240 170, 152 122" class="trans" marker-end="url(#arr4)" />
  <rect x="225" y="148" width="70" height="18" class="lbl-bg" rx="3" />
  <text x="260" y="161" class="lbl">T=1 + E'=1 / 0</text>
</svg>

| Estado Actual ($Q$) | Entradas ($T_{5m}, E$) | Estado Siguiente ($D$) | Salida ($Y$) | Acción del Sinfín |
| :---: | :---: | :---: | :---: | :--- |
| **$S_0$: APAGADO (0)** | $0 -$ | **$S_0$ (0)** | **$0$** | Espera de 5 minutos antes de volver a dispensar. |
| **$S_0$: APAGADO (0)** | $- 0$ | **$S_0$ (0)** | **$0$** | Dosificador bloqueado (deshabilitado externamente). |
| **$S_0$: APAGADO (0)** | $1 1$ | **$S_1$ (1)** | **$1$** | Ciclo activo $\rightarrow$ Motor arranca al 50% PWM (`EN2=128`). |
| **$S_1$: ENCENDIDO (1)** | $0 1$ | **$S_1$ (1)** | **$1$** | Sinfín dosificando ración ($t < 5\text{ min}$). |
| **$S_1$: ENCENDIDO (1)** | $1 -$ | **$S_0$ (0)** | **$0$** | Ración completada $\rightarrow$ Motor detenido. |
| **$S_1$: ENCENDIDO (1)** | $- 0$ | **$S_0$ (0)** | **$0$** | Parada inmediata por desactivación de habilitación. |
---

## 4. Gestión de Sensores

### 4.1 DHT22 (Temperatura + Humedad)

```cpp
struct LecturaDHT {
  float temperatura;     // °C
  float humedad;         // % RH
  bool valida;           // Validación de lectura
  unsigned long timestamp;
};
```

**Protocolo**: DHT (one-wire simplificado)  
**Latencia típica**: ~2.25ms  
**Fiabilidad**: Contador de fallos → 3 fallos consecutivos = ERROR  
**Acción en error**: Sistema entra en fail-safe

### 4.2 MQ135 (Gases: NH3/CO2)

```cpp
struct LecturaMQ135 {
  int rawValue;          // 0-4095 (ADC 12-bit)
  float voltaje;         // 3.3V max
  bool valida;
  unsigned long timestamp;
};
```

**Filtrado**: Media móvil circular de 10 muestras
  - Reduce ruido ~80%
  - Latencia: ~20ms (10 * 2ms)

**Niveles**:
  - NORMAL: < 800
  - MODERADO: 800-1499
  - ALTO: ≥ 1500

**Trigger**: Gas ALTO → Ventilación forzada

### 4.3 KY032 (Sensor de Presencia)

```cpp
struct LecturaKY032 {
  bool presencia;        // LOW = detect, HIGH = no detect
  unsigned long timestamp;
};
```

**Lógica**: LOW = presencia (infrarrojo activo)  
**Uso**: Activar puerta automática

### 4.4 HC-SR04 (Nivel de Agua)

```cpp
struct LecturaUltrasonico {
  float distancia;       // cm (0-400)
  EstadoSensorUltrasonico estado;
  unsigned long timestamp;
};
```

**Estados posibles**:
```cpp
enum EstadoSensorUltrasonico : uint8_t {
  OK = 0,              // Lectura válida
  TIMEOUT = 1,         // Pulso ECHO > 30ms
  OUT_OF_RANGE = 2,    // > 400cm o < 0.5cm
  ERROR = 3            // Fallos acumulados ≥ 3
};
```

**Filtrado**: Media móvil de 10 muestras  
**Lógica de bomba**: Histéresis
  - Bomba ON si distancia > 6.0 cm (tanque bajo)
  - Bomba OFF si distancia ≤ 3.0 cm (tanque lleno)
  - En error: Mantener último estado conocido

---

## 5. Gestión de Actuadores

### 5.1 Relés (K1-K4)

```cpp
class Actuador {
  void activar();     // GPIO = LOW (relé activo)
  void desactivar();  // GPIO = HIGH (relé inactivo)
  bool getEstado();   // true = activo
};
```

| Relé | Función | Lógica |
|------|---------|--------|
| K1 | Calefacción/Bombillos IR | ON si T < 27°C y sin gases altos |
| K2 | Ventilador | ON si K1 activo O hay ventilación |
| K3 | Extractor | ON si T ≥ 32°C O Hum > 65% O gases altos |
| K4 | Bomba agua | ON si nivel > 6cm (histéresis) |

### 5.2 Servo (Puerta)

```cpp
class ControlServo {
  void escribirServoCuidado(uint8_t angulo);
  // Ángulos:
  // 0° = Abierto
  // 93° = Neutral (reposo)
  // 180° = Cerrado
};
```

**Duración giro**: 300ms  
**Tiempo abierto**: 2000ms  
**Alimentación**: 5V (regulador externo recomendado)

### 5.3 Motor L293D (Persiana y Alimento)

```
Canal A (EN1, IN1, IN2):
  ├─ IN1=HIGH, IN2=LOW  → Motor gira apertura
  ├─ IN1=LOW, IN2=HIGH  → Motor gira cierre
  └─ EN1=PWM           → Control de velocidad

Canal B (EN2, IN3, IN4):
  ├─ IN3=HIGH, IN4=LOW  → Motor gira alimentador
  └─ EN2=PWM(128)      → 50% velocidad
```

---

## 6. Filtro de Media Móvil (MovingAverage)

### 6.1 Implementación

```cpp
template <typename T, uint16_t SIZE = 10>
class MovingAverage {
  T add(T value);      // Agregar valor, retorna promedio
  T getAverage();      // Obtener promedio actual
  bool isFilled();     // Buffer completamente lleno
};
```

### 6.2 Buffer Circular

```
Agregar valores: [v1, v2, v3, ..., v10]
Índice: 0
Suma: v1+v2+...+v10
Promedio: Suma/10

Agregar v11:
  • Suma -= v1
  • Suma += v11
  • Índice = 1
  • Promedio: Suma/10
```

### 6.3 Aplicación

| Sensor | Buffer | Latencia | Efecto |
|--------|--------|----------|--------|
| MQ135 | 10 | ~20ms | Estabiliza lectura gaseos |
| HC-SR04 | 10 | ~20ms | Elimina picos de distancia |

---

## 7. Watchdog Timer (WDT)

### 7.1 Configuración

```cpp
esp_task_wdt_init(WDT_TIMEOUT_S, true);
  // timeout = 10 segundos
  // panic = true (genera core dump)

esp_task_wdt_add(NULL);  // Agregar tarea actual a WDT

// En cada ciclo:
esp_task_wdt_reset();    // Reset del contador
```

### 7.2 Escenario de Fallo

```
Ciclo 1: Reset WDT ✓
Ciclo 2: [DEADLOCK EN TAREA] ✗
Ciclo 3: WDT cuenta = 1s
...
Ciclo 12: WDT cuenta = 10s → TIMEOUT
→ Genera panic
→ Imprime stack trace
→ Reinicia ESP32 (watchdog reboot)
```

### 7.3 Prevención de Deadlock

- **vTaskDelay()** en cada ciclo: Permite yield a scheduler
- **esp_task_wdt_reset()** antes de operaciones largas
- **No usar delay()** bloqueante — siempre vTaskDelay()

---

## 8. Estrategia de Error (Fail-Safe)

### 8.1 Estados de Error Sensor

```
Lectura OK
  ↓
Lectura ERR → Contador = 1
  ↓
Lectura ERR → Contador = 2
  ↓
Lectura ERR → Contador = 3 → enError() = true
              ↓
         Sistema → ERROR
              ↓
         failSafe()
              ↓
    [Desactivar todo salvo bomba]
```

### 8.2 Acciones en Fail-Safe

```cpp
void failSafe() {
  k1_.desactivar();      // Calefacción OFF
  k2_.desactivar();      // Ventilador OFF
  k3_.desactivar();      // Extractor OFF
  k4_.activar();         // Bomba ON (drenaje emergencia)
  
  controlServo.cerrarEmergencia();
  alimentador.detener();
  persiana.detener();
  
  LOG_ERROR("FAIL-SAFE ACTIVADO");
}
```

**Rationale**:
- Evitar sobrecalentamiento al desactivar calefacción
- Activar bomba por si hay exceso de agua
- Detener mecanismos móviles para seguridad

---

## 9. Distribución FreeRTOS

### 9.1 Core 0 (Sensores/Actuadores)

```
tareaGalpon() — Prioridad 2
  • Lectura de sensores (cada 2000ms)
  • Actualización de máquinas de estado
  • Control de actuadores
  • Monitoreo de errores
  • Reset de Watchdog Timer

Stack: 16KB
Período: ~10ms yield (vTaskDelay)
```

### 9.2 Core 1 (WiFi)

```
tareaWiFi() — Prioridad 1
  • Conexión a WiFi
  • Reconexión automática
  • Mantenimiento de conexión
  • Futuro: MQTT, OTA

Stack: 4KB
Período: 1000ms yield
```

### 9.3 Loop principal()

```
loop() — Prioridad 0 (Idle)
  • Vacío — solo vTaskDelay(1000)
  • Los cores usan el scheduler FreeRTOS
```

---

## 10. Protocolo de Comunicación (Futuro)

### 10.1 MQTT (v8.0)

```
Topic estructura:
  galpon/sensores/temperatura
  galpon/sensores/humedad
  galpon/sensores/gases
  galpon/sensores/agua
  galpon/actuadores/k1/comando
  galpon/actuadores/k1/estado
  galpon/eventos/error
  galpon/eventos/fallo_sensor
```

### 10.2 API REST (v9.0)

```
GET  /api/sensores
GET  /api/actuadores
POST /api/actuadores/k1
GET  /api/estado
GET  /api/logs
```

---

## 11. Límites y Capacidades

| Característica | Límite | Nota |
|---|---|---|
| **Número de sensores** | 16 max | Limitado por GPIO + ADC |
| **Número de actuadores** | 20 max | Limitado por GPIO, relay en cascada |
| **Frecuencia de control** | 100Hz max | Limited by millis() precision |
| **Tamaño stack** | 520KB total | 256KB + heap |
| **Conexiones WiFi simultáneas** | 1 | ESP32 STA + futuro softAP |
| **MQTT msgs/sec** | ~50 | Depende de conexión |

---

## 12. Análisis de Seguridad

### 12.1 Puntos Críticos

| Punto | Riesgo | Mitigación |
|-------|--------|-----------|
| **Fallo DHT22** | Pérdida de sensado | Contador de fallos + Fail-safe |
| **HC-SR04 timeout** | Bomba no se activa | Histéresis + último estado |
| **Servo bloqueado** | Puerta atrapada | Timeout 300ms + manual override |
| **WiFi desconectado** | Sin monitoreo remoto | Core 0 sigue funcionando |
| **Watchdog timeout** | Tarea bloqueada | Reinicio automático |
| **Stack overflow** | Corrupción de memoria | Tamaño aumentado a 16KB |

### 12.2 Validación de Datos

```cpp
// Ejemplo: Validación de distancia
if (distancia < 0.5 || distancia > 400) {
  estado = OUT_OF_RANGE;
  return;  // Rechazar lectura
}

// Validación de temperatura
if (isnan(temperatura)) {
  contador_fallos++;
  if (contador_fallos >= MAX_FALLOS) {
    enError = true;
  }
}
```

---

## 13. Performance y Optimización

### 13.1 Timing Crítico

```
Ciclo tareaGalpon() ≈ 10ms (vTaskDelay)
Lectura DHT22 ≈ 2.25ms
Lectura ADC ≈ 0.1ms
Cálculo media móvil ≈ 0.5ms
Total ≈ 2.85ms → Margen: 7.15ms ✓
```

### 13.2 Consumo de Memoria


```
Variables globales ≈ 500 bytes
Stack tareaGalpon: 16KB
Stack tareaWiFi: 4KB
Heap dinámico: Bajo (sin malloc)
Total: ~20.5KB (de 520KB disponibles)
Uso: ~4%
```

### 13.3 Optimizaciones Realizadas

✅ Uso de `vTaskDelay()` en lugar de `delay()`  
✅ Enumeraciones `enum class` tipadas  
✅ Media móvil con buffer circular (sin reallocs)  
✅ Máquinas de estado (evitar lógica anidada)  
✅ Registro centralizado en `config.h`  

---

## 14. Roadmap de Versiones

| Versión | Descripción |
|---------|---|
| **7.0** | Refactorización modular (actual) |
| **7.1** | Testing con hardware real |
| **8.0** | MQTT + Home Assistant |
| **9.0** | API REST + Dashboard web |
| **9.5** | OTA firmware updates |
| **10.0** | Sensor de peso + análisis de consumo |

---

## 15. Referencias y Estándares

- **IEC 61508** — Safety Integrity Levels (SIL)
- **MISRA C** — Motor Industry Software Reliability Association
- **TRL 5** — Technology Readiness Level (Prototipo Industrial)
- **ESP32 TRM** — Technical Reference Manual Espressif
- **FreeRTOS** — Real-time OS Kernel

---

**Arquitecto**: Andrés Luna  
**Versión**: 7.0  
**Fecha**: Agosto 2026  
**Clasificación**: Industrial (TRL 5)
