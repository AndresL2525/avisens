#ifndef CONSOLA_SERIE_H
#define CONSOLA_SERIE_H

#include <Arduino.h>
#include "config.h"

class ConsolaSerie
{
public:
  static void begin();
  static void mostrarAyuda();
  static void procesar();
};

#endif
