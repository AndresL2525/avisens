/**
 * =============================================================================
 * MovingAverage.h (MODIFICADO)
 * 
 * Cambios:
 * - Agregar método esPicoRuido() para detectar outliers usando 3-sigma rule
 * - Mantener historial de desviaciones estándar
 * - Método estático para calcular desviación estándar
 * =============================================================================
 */

#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <Arduino.h>
#include <cstring>
#include <cmath>

/**
 * @class MovingAverage
 * @brief Filtro de media móvil con detección de picos de ruido (3-sigma)
 * 
 * Template genérico que calcula la media móvil de un flujo de datos
 * usando un buffer circular de tamaño fijo. Incluye detección de picos
 * que se desvían más de N desviaciones estándar de la media.
 * 
 * @tparam T Tipo de dato (int, float, double)
 * @tparam SIZE Tamaño del buffer circular
 */
template <typename T, uint16_t SIZE = 10>
class MovingAverage {
 public:
  MovingAverage() 
    : index_(0), sum_(0), count_(0), filled_(false), sum_sq_(0) {
    memset(buffer_, 0, sizeof(buffer_));
  }

  /**
   * @brief Añade un nuevo valor al buffer y actualiza la media.
   * @param value Valor a añadir
   * @return Media móvil actual
   */
  T add(T value) {
    // Si el buffer está lleno, restar el valor antiguo que se sobrescribe
    if (filled_) {
      T old_value = buffer_[index_];
      sum_ -= old_value;
      sum_sq_ -= (old_value * old_value);
    }

    buffer_[index_] = value;
    sum_ += value;
    sum_sq_ += (value * value);

    index_ = (index_ + 1) % SIZE;

    if (!filled_ && index_ == 0) {
      filled_ = true;
    }

    count_ = filled_ ? SIZE : index_;
    return getAverage();
  }

  /**
   * @brief Obtiene la media móvil actual.
   * @return Media calculada
   */
  T getAverage() const {
    if (count_ == 0) return 0;
    return sum_ / count_;
  }

  /**
   * @brief Detecta si un valor es un pico de ruido usando 3-sigma rule.
   * 
   * Un valor se considera pico de ruido si:
   * |valor - media| > factorDesviacion * desviacion_estandar
   * 
   * Por defecto usa 3-sigma (99.7% de los datos están dentro).
   * 
   * @param value Nuevo valor a evaluar
   * @param factorDesviacion Multiplicador de desviación estándar (default 3.0)
   * @return true si el valor es pico de ruido, false si es normal
   */
  bool esPicoRuido(T value, float factorDesviacion = 3.0) const {
    // Necesitamos al menos 2 muestras para calcular desviación
    if (count_ < 2) {
      return false;
    }

    T promedio = getAverage();
    double desviacion = calcularDesviacionEstandar();
    double diferencia = std::abs(static_cast<double>(value - promedio));

    // Regla 3-sigma: si diferencia > 3 * desviacion, es outlier
    return diferencia > (factorDesviacion * desviacion);
  }

  /**
   * @brief Calcula la desviación estándar de los datos en el buffer.
   * @return Desviación estándar
   */
  double calcularDesviacionEstandar() const {
    if (count_ < 2) {
      return 0.0;
    }

    double promedio = static_cast<double>(sum_) / count_;
    double varianza = (static_cast<double>(sum_sq_) / count_) - (promedio * promedio);

    // Evitar raíz cuadrada de número negativo por errores de redondeo
    if (varianza < 0.0) {
      varianza = 0.0;
    }

    return std::sqrt(varianza);
  }

  /**
   * @brief Reinicia el buffer y la suma.
   */
  void reset() {
    memset(buffer_, 0, sizeof(buffer_));
    index_ = 0;
    sum_ = 0;
    sum_sq_ = 0;
    count_ = 0;
    filled_ = false;
  }

  /**
   * @brief Indica si el buffer está completamente lleno.
   * @return true si se han recibido al menos SIZE muestras
   */
  bool isFilled() const {
    return filled_;
  }

  /**
   * @brief Obtiene el número actual de muestras en el buffer.
   * @return Número de muestras (0 a SIZE)
   */
  uint16_t getCount() const {
    return count_;
  }

  /**
   * @brief Obtiene el tamaño máximo del buffer.
   * @return SIZE
   */
  static constexpr uint16_t getSize() {
    return SIZE;
  }

  /**
   * @brief Obtiene el valor mínimo en el buffer.
   * @return Valor mínimo
   */
  T getMin() const {
    if (count_ == 0) return 0;
    T minval = buffer_[0];
    for (uint16_t i = 0; i < count_; i++) {
      if (buffer_[i] < minval) {
        minval = buffer_[i];
      }
    }
    return minval;
  }

  /**
   * @brief Obtiene el valor máximo en el buffer.
   * @return Valor máximo
   */
  T getMax() const {
    if (count_ == 0) return 0;
    T maxval = buffer_[0];
    for (uint16_t i = 0; i < count_; i++) {
      if (buffer_[i] > maxval) {
        maxval = buffer_[i];
      }
    }
    return maxval;
  }

 private:
  T buffer_[SIZE];
  uint16_t index_;
  double sum_;      // Suma acumulada
  double sum_sq_;   // Suma de cuadrados (para varianza)
  uint16_t count_;
  bool filled_;
};

#endif  // MOVING_AVERAGE_H
