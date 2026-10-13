#pragma once
#include "math_constants.h"
#include <cmath>

// ============================================================
//  State Variable Filter (Chamberlin).
//
//  Устойчивее biquad на краях диапазона, даёт одновременно
//  LP / BP / HP выходы, прост в реализации, легко считается.
//
//  Нормированная частота:
//      f = 2 * sin(pi * fc / fs)
//  Затухание:
//      q = 1 / Q
//
//  Коэффициенты задаются через set(). Один экземпляр = один
//  фильтр, хранит своё состояние (lp, bp).
//
//  Используется:
//    - в ocean_noise.cpp — для 4 полос волны и шума приёмника;
//    - в source_model.cpp — для полосовой окраски кавитации
//      и механического шума.
//
//  Границы:
//    fc ∈ [5 Гц, 0.45 * fs]     (иначе клампится)
//    Q  ∈ [0.5, 4.0]            (иначе клампится)
//
//  Защита от разбега: если bp или lp уходят за пределы float
//  или становятся NaN/Inf — состояние сбрасывается в 0.
// ============================================================

namespace hydro
{
    struct Svf
    {
        float f  = 0.1f;    // нормированная частота
        float q  = 1.0f;    // 1 / Q
        float lp = 0.0f;    // состояние LP-выхода
        float bp = 0.0f;    // состояние BP-выхода

        // Настройка фильтра под fs, fc и Q.
        void set(float fs, float fc, float Q)
        {
            if (fc < 5.0f)          fc = 5.0f;
            if (fc > fs * 0.45f)    fc = fs * 0.45f;
            if (Q  < 0.5f)          Q  = 0.5f;
            if (Q  > 4.0f)          Q  = 4.0f;

            f = 2.0f * std::sin(PI_F * fc / fs);
            if (f > 1.0f) f = 1.0f;
            q = 1.0f / Q;
        }

        // Прогон одного отсчёта. Возвращает BP-выход.
        // LP и BP обновляются внутри.
        float processBP(float x)
        {
            const float hp = x - lp - q * bp;
            bp += f * hp;
            lp += f * bp;

            // Защита от разбега.
            if (!std::isfinite(bp) || std::fabs(bp) > 1e6f)
            {
                bp = 0.0f;
                lp = 0.0f;
            }
            if (!std::isfinite(lp) || std::fabs(lp) > 1e6f)
            {
                lp = 0.0f;
            }

            return bp;
        }

        // Сброс состояния (для повторного использования).
        void reset()
        {
            lp = 0.0f;
            bp = 0.0f;
        }
    };

} // namespace hydro