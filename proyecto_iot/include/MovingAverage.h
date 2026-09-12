

#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <Arduino.h>
#include <cstring>
#include <cmath>

template <typename T, uint16_t SIZE = 10>
class MovingAverage
{
public:
  MovingAverage()
      : index_(0), sum_(0), count_(0), filled_(false), sum_sq_(0)
  {
    memset(buffer_, 0, sizeof(buffer_));
  }

  T add(T value)
  {
    // Si el buffer está lleno, restar el valor antiguo que se sobrescribe
    if (filled_)
    {
      T old_value = buffer_[index_];
      sum_ -= old_value;
      sum_sq_ -= (old_value * old_value);
    }

    buffer_[index_] = value;
    sum_ += value;
    sum_sq_ += (value * value);

    index_ = (index_ + 1) % SIZE;

    if (!filled_ && index_ == 0)
    {
      filled_ = true;
    }

    count_ = filled_ ? SIZE : index_;
    return getAverage();
  }

  T getAverage() const
  {
    if (count_ == 0)
      return 0;
    return sum_ / count_;
  }

  bool esPicoRuido(T value, float factorDesviacion = 3.0) const
  {
    // Necesitamos al menos 2 muestras para calcular desviación
    if (count_ < 2)
    {
      return false;
    }

    T promedio = getAverage();
    double desviacion = calcularDesviacionEstandar();
    double diferencia = std::abs(static_cast<double>(value - promedio));

    // Regla 3-sigma: si diferencia > 3 * desviacion, es outlier
    return diferencia > (factorDesviacion * desviacion);
  }

  double calcularDesviacionEstandar() const
  {
    if (count_ < 2)
    {
      return 0.0;
    }

    double promedio = static_cast<double>(sum_) / count_;
    double varianza = (static_cast<double>(sum_sq_) / count_) - (promedio * promedio);

    // Evitar raíz cuadrada de número negativo por errores de redondeo
    if (varianza < 0.0)
    {
      varianza = 0.0;
    }

    return std::sqrt(varianza);
  }

  void reset()
  {
    memset(buffer_, 0, sizeof(buffer_));
    index_ = 0;
    sum_ = 0;
    sum_sq_ = 0;
    count_ = 0;
    filled_ = false;
  }

  bool isFilled() const
  {
    return filled_;
  }

  uint16_t getCount() const
  {
    return count_;
  }

  static constexpr uint16_t getSize()
  {
    return SIZE;
  }

  T getMin() const
  {
    if (count_ == 0)
      return 0;
    T minval = buffer_[0];
    for (uint16_t i = 0; i < count_; i++)
    {
      if (buffer_[i] < minval)
      {
        minval = buffer_[i];
      }
    }
    return minval;
  }

  T getMax() const
  {
    if (count_ == 0)
      return 0;
    T maxval = buffer_[0];
    for (uint16_t i = 0; i < count_; i++)
    {
      if (buffer_[i] > maxval)
      {
        maxval = buffer_[i];
      }
    }
    return maxval;
  }

private:
  T buffer_[SIZE];
  uint16_t index_;
  double sum_;    // Suma acumulada
  double sum_sq_; // Suma de cuadrados (para varianza)
  uint16_t count_;
  bool filled_;
};

#endif // MOVING_AVERAGE_H
