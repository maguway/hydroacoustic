#include "dolphin_whistle_test.h"
#include "math_constants.h"

#include <cmath>
#include <algorithm>

namespace hydro
{
    DolphinWhistleTest::DolphinWhistleTest(const DolphinWhistleTestParams& p)
        : p_(p), rng_(p.seed)
    {
    }

    void DolphinWhistleTest::setParams(const DolphinWhistleTestParams& p)
    {
        p_ = p;
        rng_.seed(p.seed);
    }

    // ============================================================
    //  Свист — фразы с уходом в низ в конце.
    //
    //  Каждая фраза:
    //    - старт 6–9 кГц;
    //    - середина 3–6 кГц;
    //    - финиш 0.8–1.5 кГц (уход в низ = «эхо»).
    //  Края фразы плавные: вход 15%, выход 35%.
    //  Между фразами — пауза 200–500 мс.
    //
    //  Стрёкот привязан к концу фразы (см. renderCrackle).
    // ============================================================
    void DolphinWhistleTest::renderWhistle(std::vector<float>& buffer,
                                           int numSamples)
    {
        const float fs = p_.fs;
        const float invFs = 1.0f / fs;
        const float dur = static_cast<float>(numSamples) * invFs;

        float phase = 0.0f;
        float t = 0.0f;

        std::uniform_real_distribution<float> phraseDurDist(0.6f, 1.3f);
        std::uniform_real_distribution<float> pauseDist(0.20f, 0.50f);
        std::uniform_real_distribution<float> startFreqDist(6000.0f, 9000.0f);
        std::uniform_real_distribution<float> midFreqDist(3000.0f, 6000.0f);
        std::uniform_real_distribution<float> endFreqDist(800.0f, 1500.0f);

        while (t < dur)
        {
            const float phraseDur = phraseDurDist(rng_);
            const float phraseEnd = std::min(t + phraseDur, dur);
            const float phraseLen = phraseEnd - t;

            const float fStart = startFreqDist(rng_);
            const float fMid = midFreqDist(rng_);
            const float fEnd = endFreqDist(rng_);

            const int nStart = static_cast<int>(t * fs);
            const int nEnd = static_cast<int>(phraseEnd * fs);

            for (int n = nStart; n < nEnd && n < numSamples; ++n)
            {
                const float tLocal = static_cast<float>(n) * invFs;
                const float u = (tLocal - t) / phraseLen;

                // Три контрольные точки: fStart → fMid → fEnd.
                // Кусочно-косинусная интерполяция.
                float fInst;
                if (u < 0.5f)
                {
                    const float v = u * 2.0f;
                    const float s = 0.5f * (1.0f - std::cos(PI_F * v));
                    fInst = fStart + (fMid - fStart) * s;
                }
                else
                {
                    const float v = (u - 0.5f) * 2.0f;
                    const float s = 0.5f * (1.0f - std::cos(PI_F * v));
                    fInst = fMid + (fEnd - fMid) * s;
                }

                // Огибающая: вход 15%, выход 35%.
                float env = 1.0f;
                if (u < 0.15f)
                    env = 0.5f * (1.0f - std::cos(PI_F * u / 0.15f));
                else if (u > 0.65f)
                {
                    const float v = (1.0f - u) / 0.35f;
                    env = 0.5f * (1.0f - std::cos(PI_F * v));
                }

                phase += TWO_PI_F * fInst * invFs;
                if (phase > TWO_PI_F) phase -= TWO_PI_F;

                buffer[n] += std::sin(phase) * env;
            }

            t = phraseEnd + pauseDist(rng_);
        }
    }

    // ============================================================
    //  Шум моря — тихий широкополосный, наклон в НЧ.
    // ============================================================
    void DolphinWhistleTest::renderSeaNoise(std::vector<float>& buffer,
                                            int numSamples)
    {
        if (p_.seaNoiseLevel <= 0.0f) return;

        const float fs = p_.fs;
        const float invFs = 1.0f / fs;

        const float fc = 1200.0f;
        const float a = std::exp(-TWO_PI_F * fc * invFs);
        float lp = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            const float t = static_cast<float>(n) * invFs;

            const float white = gauss_(rng_);
            lp = a * lp + (1.0f - a) * white;
            const float band = 0.7f * lp + 0.3f * white;

            const float am = 1.0f
                - p_.seaNoiseAmDepth * 0.5f
                + p_.seaNoiseAmDepth * 0.5f
                * std::sin(TWO_PI_F * p_.seaNoiseAmHz * t);

            buffer[n] += band * am * p_.seaNoiseLevel;
        }
    }

    // ============================================================
    //  Стрёкот — серия щелчков в конце каждой фразы.
    //
    //  Привязка: та же логика фраз, что в renderWhistle,
    //  стрёкот идёт в последние 25% длительности фразы
    //  (то есть когда свист «уходит в низ»).
    //
    //  Громкость: сравнимая со свистом.
    //  Полоса: 8–14 кГц (УВЧ).
    //  Щелчки: короткие (0.5–2 мс), частые (интервал 5–15 мс).
    // ============================================================
    void DolphinWhistleTest::renderCrackle(std::vector<float>& buffer,
                                           int numSamples)
    {
        if (p_.crackleLevel <= 0.0f) return;

        const float fs = p_.fs;
        const float invFs = 1.0f / fs;
        const float dur = static_cast<float>(numSamples) * invFs;

        std::uniform_real_distribution<float> phraseDurDist(0.6f, 1.3f);
        std::uniform_real_distribution<float> pauseDist(0.20f, 0.50f);
        std::uniform_real_distribution<float> durDist(
            p_.crackleDurMinMs * 0.001f,
            p_.crackleDurMaxMs * 0.001f);
        std::uniform_real_distribution<float> clickIntervalDist(0.005f, 0.015f);

        const float fcMid = 0.5f * (p_.crackleFreqMin + p_.crackleFreqMax);
        const float a = std::exp(-TWO_PI_F * fcMid * invFs);
        float lp1 = 0.0f, lp2 = 0.0f;

        float t = 0.0f;
        while (t < dur)
        {
            const float phraseDur = phraseDurDist(rng_);
            const float phraseEnd = std::min(t + phraseDur, dur);
            const float phraseLen = phraseEnd - t;

            // Стрёкот — в последние 25% фразы.
            const float crackleStart = t + phraseLen * 0.75f;
            float tc = crackleStart;

            // Щелчки идут до конца фразы, но не за её пределы.
            while (tc < phraseEnd)
            {
                const int idx = static_cast<int>(tc * fs);
                if (idx < 0 || idx >= numSamples) break;

                const float pulseDur = durDist(rng_);
                const int len = static_cast<int>(pulseDur * fs) + 1;
                const float tau = pulseDur / 2.0f;
                const float invTau2 = 1.0f / (tau * tau + 1e-9f);

                for (int k = 0; k < len; ++k)
                {
                    const int i = idx + k;
                    if (i < 0 || i >= numSamples) break;

                    const float tLocal = static_cast<float>(k) * invFs;
                    const float env = std::exp(-(tLocal * tLocal) * invTau2);

                    const float white = gauss_(rng_);
                    lp1 = a * lp1 + (1.0f - a) * white;
                    lp2 = a * lp2 + (1.0f - a) * lp1;
                    const float band = lp1 - lp2;

                    buffer[i] += band * env * p_.crackleLevel;
                }

                tc += clickIntervalDist(rng_);
            }

            // Переход к следующей фразе.
            t = phraseEnd + pauseDist(rng_);
        }
    }

    // ============================================================
    //  Главный метод.
    // ============================================================
    void DolphinWhistleTest::generate(float* out, int numSamples)
    {
        if (numSamples <= 0) return;

        for (int n = 0; n < numSamples; ++n) out[n] = 0.0f;

        std::vector<float> buffer(numSamples, 0.0f);

        rng_.seed(p_.seed);

        renderSeaNoise(buffer, numSamples);
        renderWhistle(buffer, numSamples);
        renderCrackle(buffer, numSamples);

        float maxAbs = 0.0f;
        for (int n = 0; n < numSamples; ++n)
        {
            const float a = std::fabs(buffer[n]);
            if (a > maxAbs) maxAbs = a;
        }
        const float scale = (maxAbs > 1e-9f) ? (0.95f / maxAbs) : 1.0f;

        for (int n = 0; n < numSamples; ++n)
            out[n] = buffer[n] * scale;
    }
} // namespace hydro
