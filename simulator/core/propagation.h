#pragma once
#include "types.h"
#include "uca_geometry.h"
#include <vector>
#include <array>

namespace hydro {

    // ============================================================
    //  Модель распространения: раскладывает МОНО-сигнал источника
    //  на 8 каналов UCA с учётом задержек и добавляет шум приёмника.
    //
    //  Использует arrivalOffsets() из UcaGeometry, то есть
    //  ГЕНЕРАЦИОННОЕ соглашение о знаке:
    //    dt_i = -R*cos(theta - phi_i)/c
    //  Сигнал на i-м элементе: s_i(t) = s_source(t - dt_i)
    //
    //  Дробные задержки — линейная интерполяция.
    //
    //  Шум приёмника — гауссов белый, независимый на каждом канале.
    // ============================================================

    class Propagation {
    public:
        // Параметры распространения.
        struct Params {
            float azimuthDeg    = 0.0f;      // азимут источника, градусы
            float soundSpeed    = 1500.0f;   // м/с (вода)
            float receiverSnrDb = -40.0f;    // SNR приёмника относительно сигнала
            uint32_t noiseSeed  = 42;        // seed для генератора шума
        };

        Propagation() = default;
        explicit Propagation(const Params& p) : p_(p) {}

        void setParams(const Params& p) { p_ = p; }
        const Params& params() const { return p_; }

        // Раскладывает моно-сигнал на 8 каналов.
        //   monoSource — входной моно-сигнал
        //   numSamples — число отсчётов
        //   fs         — частота дискретизации
        //   uca        — геометрия решётки
        //   out        — [NUM_CHANNELS][numSamples] выходной 8-канальный сигнал
        void propagate(const std::vector<float>& monoSource,
                       int numSamples,
                       float fs,
                       const UcaGeometry& uca,
                       std::array<std::vector<float>, NUM_CHANNELS>& out);

    private:
        Params p_;
    };

} // namespace hydro