#include "propagation.h"
#include "math_constants.h"
#include <cmath>
#include <random>

namespace hydro
{
    // Линейная интерполяция: значение сигнала в точке t (дробный индекс).
    // idx может быть отрицательным или >= numSamples — возвращаем 0.
    static inline float sampleLinear(const std::vector<float>& x,
                                     float idx,
                                     int numSamples)
    {
        if (idx < 0.0f || idx >= static_cast<float>(numSamples - 1))
        {
            // За границей — нули (сигнал вне окна)
            return 0.0f;
        }
        const int i0 = static_cast<int>(idx);
        const float frac = idx - static_cast<float>(i0);
        const float s0 = x[i0];
        const float s1 = x[i0 + 1];
        return s0 + frac * (s1 - s0);
    }

    void Propagation::propagate(const std::vector<float>& monoSource,
                                int numSamples,
                                float fs,
                                const UcaGeometry& uca,
                                std::array<std::vector<float>, NUM_CHANNELS>& out)
    {
        const int N = uca.numElements();
        if (N != NUM_CHANNELS)
        {
            // На всякий случай — наша UCA должна быть 8-элементной
            // (в будущем можно будет обобщить).
            return;
        }

        // 0. Омни-источник (океан, AZIMUTH_OMNI): без задержек,
        //    поровну на все каналы, с сохранением RMS.
        if (p_.azimuthDeg == AZIMUTH_OMNI)
        {
            const float perCh = 1.0f / std::sqrt(static_cast<float>(N));

            // Уровень шума приёмника — как в обычной ветке.
            float rmsSignal = 0.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                rmsSignal += monoSource[i] * monoSource[i];
            }
            rmsSignal = std::sqrt(rmsSignal / std::max(1, numSamples));

            float noiseRms = 0.0f;
            if (p_.receiverSnrDb <= 100.0f)
            {
                noiseRms = rmsSignal * std::pow(10.0f, -p_.receiverSnrDb / 20.0f);
            }

            std::mt19937 rng(p_.noiseSeed);
            std::normal_distribution<float> gauss(0.0f, 1.0f);

            for (int ch = 0; ch < N; ++ch)
            {
                out[ch].assign(numSamples, 0.0f);
                for (int n = 0; n < numSamples; ++n)
                {
                    const float noise = gauss(rng) * noiseRms;
                    out[ch][n] = monoSource[n] * perCh + noise;
                }
            }
            return;
        }

        // 1. Задержки (генерационное соглашение — arrivalOffsets).
        const auto dt = uca.arrivalOffsets(p_.azimuthDeg, p_.soundSpeed);

        // 2. Уровень шума приёмника (относительно RMS сигнала).
        float rmsSignal = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            rmsSignal += monoSource[i] * monoSource[i];
        }
        rmsSignal = std::sqrt(rmsSignal / std::max(1, numSamples));

        float noiseRms = 0.0f;
        if (p_.receiverSnrDb > 100.0f)
        {
            noiseRms = 0.0f;
        }
        else
        {
            noiseRms = rmsSignal * std::pow(10.0f, -p_.receiverSnrDb / 20.0f);
        }

        // 3. Раскладка по каналам.
        std::mt19937 rng(p_.noiseSeed);
        std::normal_distribution<float> gauss(0.0f, 1.0f);

        for (int ch = 0; ch < N; ++ch)
        {
            out[ch].assign(numSamples, 0.0f);

            // Задержка в отсчётах: dt[ch] секунд → dt[ch]*fs отсчётов.
            const float delaySamples = dt[ch] * fs;

            for (int n = 0; n < numSamples; ++n)
            {
                // s_i(t_n) = s_source(t_n - dt_i)
                //           = monoSource[n - delaySamples]
                const float srcIdx = static_cast<float>(n) - delaySamples;
                const float s = sampleLinear(monoSource, srcIdx, numSamples);

                // Шум приёмника (независимый на каждом канале)
                const float noise = gauss(rng) * noiseRms;

                out[ch][n] = s + noise;
            }
        }
    }
} // namespace hydro
