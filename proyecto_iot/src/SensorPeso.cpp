#include "SensorPeso.h"

SensorPeso::SensorPeso()
    : pinDT_(HX711_DT),
      pinSCK_(HX711_SCK),
      offsetCero_(0),
      factorEscala_(1.0f),
      tarado_(false),
      avisoTaraEmitido_(false),
      contadorFallos_(0),
      ultimoIntento_(0)
{
  ultimaLectura_ = {0.0f, 0.0f, false, 0};
}

void SensorPeso::begin()
{
  pinMode(pinDT_, INPUT);
  pinMode(pinSCK_, OUTPUT);
  digitalWrite(pinSCK_, LOW);

  delay(100); // El HX711 necesita estabilizar su alimentación antes del primer ciclo

  if (!verificarConexion())
  {
    LOG_ERROR("HX711 no detectado — Revisar conexión DT/SCK");
    contadorFallos_++;
  }
  else
  {
    LOG_DEBUG("SensorPeso HX711 inicializado");
  }
}

LecturaPeso SensorPeso::leer()
{
  LecturaPeso lectura;
  lectura.peso = 0.0f;
  lectura.voltaje = 0.0f;
  lectura.valida = false;
  lectura.timestamp = millis();

  if (!tarado_)
  {
    if (!avisoTaraEmitido_)
    {
      LOG_WARN("SensorPeso: sin tara, lecturas de peso deshabilitadas hasta recibir TARA");
      avisoTaraEmitido_ = true;
    }
    return lectura;
  }

  long rawADC = 0;
  if (!leerADC(rawADC))
  {
    contadorFallos_++;
    Serial.print("⚠ HX711 fallo #");
    Serial.println(contadorFallos_);
    return lectura;
  }

  if (rawADC >= HX711_SATURACION_POS || rawADC <= HX711_SATURACION_NEG)
  {
    LOG_WARN("HX711 saturado — Celda sobrecargada o mal conectada");
    contadorFallos_++;
    return lectura;
  }

  float peso = (rawADC - offsetCero_) * factorEscala_;

  lectura.peso = (peso < 0) ? 0 : peso;
  lectura.voltaje = rawADC * (ADC_VREF / HX711_ADC_FONDO_ESCALA);
  lectura.valida = true;

  contadorFallos_ = 0;
  ultimaLectura_ = lectura;
  return lectura;
}

LecturaPeso SensorPeso::getUltimaLectura() const
{
  return ultimaLectura_;
}

void SensorPeso::tara()
{
  LOG_DEBUG("HX711: Iniciando tara...");
  delay(500);

  long promedio = 0;
  if (!promediarLecturas(HX711_MUESTRAS_TARA, promedio))
  {
    LOG_ERROR("HX711 sin respuesta — Tara cancelada");
    tarado_ = false;
    return;
  }

  offsetCero_ = promedio;
  tarado_ = true;
  avisoTaraEmitido_ = false;

  Serial.print("Tara completada. Offset = ");
  Serial.println(offsetCero_);
}

void SensorPeso::setFactor(float factor)
{
  factorEscala_ = factor;
  Serial.print("Factor de escala: ");
  Serial.println(factor);
}

bool SensorPeso::enError() const
{
  return contadorFallos_ >= MAX_FALLOS_SENSOR;
}

void SensorPeso::reset()
{
  offsetCero_ = 0;
  factorEscala_ = 1.0f;
  tarado_ = false;
  contadorFallos_ = 0;
  ultimaLectura_ = {0.0f, 0.0f, false, 0};
  LOG_DEBUG("SensorPeso reiniciado");
}

bool SensorPeso::leerADC(long &valor)
{
  // El HX711 señala dato listo bajando DT; sin conversión disponible lo mantiene en alto
  unsigned long timeout = millis() + HX711_TIMEOUT_MS;
  while (digitalRead(pinDT_) == HIGH)
  {
    if (millis() > timeout)
    {
      LOG_WARN("HX711 timeout esperando datos");
      return false;
    }
    delayMicroseconds(1);
  }

  long resultado = 0;

  for (int i = 0; i < HX711_BITS; i++)
  {
    digitalWrite(pinSCK_, HIGH);
    delayMicroseconds(1);

    resultado <<= 1;
    if (digitalRead(pinDT_) == HIGH)
    {
      resultado |= 1;
    }

    digitalWrite(pinSCK_, LOW);
    delayMicroseconds(1);
  }

  // Pulso 25: selecciona la ganancia 128 del canal A para la conversión siguiente
  digitalWrite(pinSCK_, HIGH);
  delayMicroseconds(1);
  digitalWrite(pinSCK_, LOW);
  delayMicroseconds(1);

  // La palabra llega en complemento a dos de 24 bits: hay que extender el signo a 32
  if (resultado & 0x800000L)
  {
    resultado |= ~0xFFFFFFL;
  }

  valor = resultado;
  return true;
}

bool SensorPeso::verificarConexion()
{
  long valor = 0;
  for (int i = 0; i < 3; i++)
  {
    if (leerADC(valor))
    {
      return true;
    }
    delay(HX711_ESPERA_MUESTRA_MS);
  }
  return false;
}

bool SensorPeso::promediarLecturas(uint16_t muestras, long &promedio)
{
  long suma = 0;
  uint16_t validas = 0;
  long valor = 0;

  for (uint16_t i = 0; i < muestras; i++)
  {
    if (leerADC(valor))
    {
      suma += valor;
      validas++;
    }
    delay(HX711_ESPERA_MUESTRA_MS); // El HX711 muestrea a 10 SPS
  }

  if (validas == 0)
  {
    return false;
  }

  promedio = suma / validas;
  return true;
}
