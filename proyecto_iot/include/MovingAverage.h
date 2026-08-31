#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <Arduino.h>
#include <cstring>

/**
 * @class MovingAverage
 * @brief Filtro de media móvil con buffer circular.
 * 
 * Template genérico que calcula la media móvil de un flujo de datos
 * usando un buffer circular de tamaño fijo. Útil para suavizar
 * lecturas de sensores ruidosos (MQ135, sensores ultrasónicos, etc.).
 * 
 * @tparam T Tipo de dato (int, float, double)
 * @tparam SIZE Tamaño del buffer circular
 */
template <typename T, uint16_t SIZE = 10>
class MovingAverage {
 public:
  MovingAverage() 
    : index_(0), sum_(0), count_(0), filled_(false) {
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
      sum_ -= buffer_[index_];
    }

    buffer_[index_] = value;
    sum_ += value;

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
   * @brief Reinicia el buffer y la suma.
   */
  void reset() {
    memset(buffer_, 0, sizeof(buffer_));
    index_ = 0;
    sum_ = 0;
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

 private:
  T buffer_[SIZE];
  uint16_t index_;
  double sum_;
  uint16_t count_;
  bool filled_;
};

#endif // MOVING_AVERAGE_H
