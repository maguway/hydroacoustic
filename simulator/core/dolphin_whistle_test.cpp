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
    //  Сгенерировать поток переливов.
    //
    //  Три фазы:
    //    1) начало  — 3 перелива: два «спуска сверху вниз»
    //                 + короткий подъём;
    //    2) середина — 10–20 хаотичных переливов встык;
    //    3) конец   — 3 перелива: два спуска + длинный
    //                 затихающий.
    //
    //  Каждый перелив — свой диапазон и своё направление.
    //  Иногда накладываются (overlapFrac).
    // ============================================================
    std::vector<DolphinWhistleTest::Transition>
    DolphinWhistleTest::generateTransitions(int numSamples)
    {
        std::vector<Transition> out;

        std::uniform_real_distribution<float> freqDist(p_.fMin, p_.fMax);
        std::uniform_real_distribution<float> durDist(p_.transDurMin,
                                                      p_.transDurMax);
        std::uniform_real_distribution<float> phaseDist(0.0f, TWO_PI_F);
        std::uniform_real_distribution<float> ampDist(0.7f, 1.3f);
        std::uniform_int_distribution<int> shapeDist(0, 4);

        const float fs = p_.fs;
        const int totalSamples = numSamples;

        int pos = 0;
        int made = 0;
        const int target = std::uniform_int_distribution<int>(
            p_.numTransitionsMin, p_.numTransitionsMax)(rng_);

        // Фаза 1: 3 перелива.
        // Перелив 1: сверху вниз.
        // Перелив 2: сверху вниз (чуть иначе).
        // Перелив 3: короткий подъём.
        auto makeOne = [&](int shapeHint, float durScale)
        {
            Transition t;
            float dur = durDist(rng_) * durScale;
            t.lengthSamples = static_cast<int>(dur * fs);
            if (pos + t.lengthSamples > totalSamples)
                t.lengthSamples = totalSamples - pos;
            if (t.lengthSamples < 200) return false;

            t.sampleStart = pos;
            t.shape = shapeHint;
            t.phase = phaseDist(rng_);
            t.amp = ampDist(rng_);

            const float fa = freqDist(rng_);
            const float fb = freqDist(rng_);
            const float fLow = std::min(fa, fb);
            const float fHigh = std::max(fa, fb);

            switch (shapeHint)
            {
            case 0: // up
                t.f0 = fLow; t.f1 = fHigh; t.fMid = 0.0f;
                break;
            case 1: // down
                t.f0 = fHigh; t.f1 = fLow; t.fMid = 0.0f;
                break;
            case 2: // U
                t.f0 = fHigh; t.f1 = fHigh; t.fMid = fLow;
                break;
            case 3: // capU (∩)
                t.f0 = fLow; t.f1 = fLow; t.fMid = fHigh;
                break;
            case 4: // broken: сначала вверх, потом вниз
                t.f0 = fLow;
                t.fMid = fHigh;
                t.f1 = (fLow + fHigh) * 0.5f;
                break;
            default:
                t.f0 = fLow; t.f1 = fHigh; t.fMid = 0.0f;
                break;
            }

            out.push_back(t);

            // Следующий старт — встык или с наложением.
            const int overlap = static_cast<int>(
                static_cast<float>(t.lengthSamples) * p_.overlapFrac);
            pos += t.lengthSamples - overlap;
            if (pos < 0) pos = 0;
            ++made;
            return true;
        };

        // Фаза 1.
        // «Спуск сверху вниз» — shape=1, но с высокого к среднему.
        {
            Transition t;
            float dur = durDist(rng_) * 1.5f;
            t.lengthSamples = static_cast<int>(dur * fs);
            if (t.lengthSamples > totalSamples) t.lengthSamples = totalSamples;
            t.sampleStart = 0;
            t.shape = 1;
            t.f0 = p_.fMax * 0.9f;
            t.f1 = p_.fMax * 0.15f;
            t.fMid = 0.0f;
            t.phase = phaseDist(rng_);
            t.amp = 1.0f;
            out.push_back(t);
            pos = static_cast<int>(t.lengthSamples * (1.0f - p_.overlapFrac));
        }
        // Второй спуск.
        if (pos < totalSamples)
        {
            Transition t;
            float dur = durDist(rng_) * 1.2f;
            t.lengthSamples = static_cast<int>(dur * fs);
            if (pos + t.lengthSamples > totalSamples)
                t.lengthSamples = totalSamples - pos;
            t.sampleStart = pos;
            t.shape = 1;
            t.f0 = p_.fMax * 0.7f;
            t.f1 = p_.fMin * 1.5f;
            t.phase = phaseDist(rng_);
            t.amp = 0.9f;
            out.push_back(t);
            pos += static_cast<int>(t.lengthSamples * (1.0f - p_.overlapFrac));
        }
        // Короткий подъём.
        if (pos < totalSamples) makeOne(0, 0.4f);

        // Фаза 2: хаотичные переливы, пока не дойдём до последней
        // трети времени.
        const int phase3Start = static_cast<int>(totalSamples * 0.7f);
        int guard = 0;
        while (pos < phase3Start && made < target && guard < 200)
        {
            // Иногда намеренно ставим «сложный» shape (4).
            int shape = shapeDist(rng_);
            if (rng_() % 3 == 0) shape = 4;
            if (!makeOne(shape, 1.0f)) break;
            ++guard;
        }

        // Фаза 3: снова два спуска + длинный затихающий.
        // Спуск 1.
        if (pos < totalSamples)
        {
            Transition t;
            float dur = durDist(rng_) * 1.3f;
            t.lengthSamples = static_cast<int>(dur * fs);
            if (pos + t.lengthSamples > totalSamples)
                t.lengthSamples = totalSamples - pos;
            t.sampleStart = pos;
            t.shape = 1;
            t.f0 = p_.fMax * 0.8f;
            t.f1 = p_.fMin * 2.0f;
            t.phase = phaseDist(rng_);
            t.amp = 0.9f;
            out.push_back(t);
            pos += static_cast<int>(t.lengthSamples * (1.0f - p_.overlapFrac));
        }
        // Спуск 2 — длинный, затихающий.
        if (pos < totalSamples)
        {
            Transition t;
            float dur = durDist(rng_) * 2.0f;
            t.lengthSamples = static_cast<int>(dur * fs);
            if (pos + t.lengthSamples > totalSamples)
                t.lengthSamples = totalSamples - pos;
            t.sampleStart = pos;
            t.shape = 1;
            t.f0 = p_.fMax * 0.5f;
            t.f1 = p_.fMin;
            t.phase = phaseDist(rng_);
            t.amp = 0.7f;
            out.push_back(t);
        }

        return out;
    }

    // ============================================================
    //  Рендер одного перелива.
    // ============================================================
    void DolphinWhistleTest::renderTransition(std::vector<float>& buffer,
                                              int numSamples,
                                              const Transition& t)
    {
        if (t.lengthSamples <= 0) return;

        const float fs = p_.fs;
        const float invFs = 1.0f / fs;
        const float durSec = static_cast<float>(t.lengthSamples) * invFs;

        for (int n = 0; n < t.lengthSamples; ++n)
        {
            const int idx = t.sampleStart + n;
            if (idx < 0 || idx >= numSamples) continue;

            const float tLocal = static_cast<float>(n) * invFs;
            const float u = tLocal / durSec;

            // Огибающая: плавный вход 15%, плавный выход 15%.
            float env = 1.0f;
            constexpr float RISE = 0.15f;
            constexpr float FALL = 0.15f;
            if (u < RISE)
                env = 0.5f * (1.0f - std::cos(PI_F * u / RISE));
            else if (u > 1.0f - FALL)
            {
                const float v = (1.0f - u) / FALL;
                env = 0.5f * (1.0f - std::cos(PI_F * v));
            }

            // Мгновенная частота по форме.
            float fInst = 0.0f;
            switch (t.shape)
            {
            case 0: // up
                fInst = t.f0 + (t.f1 - t.f0) * u;
                break;
            case 1: // down
                fInst = t.f0 + (t.f1 - t.f0) * u;
                break;
            case 2: // U: fHigh → fLow → fHigh
                // f0 == f1 == fHigh, fMid == fLow.
                fInst = t.f0 - (t.f0 - t.fMid) * std::sin(PI_F * u);
                break;
            case 3: // ∩: fLow → fHigh → fLow
                fInst = t.f0 + (t.fMid - t.f0) * std::sin(PI_F * u);
                break;
            case 4: // broken: fLow → fHigh → mid
                {
                    // Первая половина: fLow → fMid.
                    // Вторая половина: fMid → f1.
                    if (u < 0.5f)
                    {
                        const float v = u * 2.0f;
                        fInst = t.f0 + (t.fMid - t.f0) * v;
                    }
                    else
                    {
                        const float v = (u - 0.5f) * 2.0f;
                        fInst = t.fMid + (t.f1 - t.fMid) * v;
                    }
                }
                break;
            default:
                fInst = t.f0;
                break;
            }

            if (fInst < 20.0f) fInst = 20.0f;
            if (fInst > fs * 0.45f) fInst = fs * 0.45f;

            // Основной тон.
            const float sMain = std::sin(TWO_PI_F * fInst * tLocal + t.phase);

            // Низкая «тень» (тело свиста).
            const float fShadow = std::max(5.0f, fInst * 0.15f);
            const float sShadow = std::sin(TWO_PI_F * fShadow * tLocal);

            const float s = sMain + 0.35f * sShadow;

            buffer[idx] += s * env * t.amp;
        }
    }

    // ============================================================
    //  Треск — поверх всего сигнала.
    //
    //  Не белый шум, а короткие шершавые всплески:
    //  шумовой компонент, промодулированный короткой
    //  гауссовой огибающей, в полосе 2–5 кГц.
    //
    //  Идёт всё время файла, накладывается на свист.
    // ============================================================
    void DolphinWhistleTest::renderCrackle(std::vector<float>& buffer,
                                           int numSamples)
    {
        if (p_.crackleLevel <= 0.0f) return;
        if (p_.crackleRateHz <= 0.0f) return;

        const float fs = p_.fs;
        const float invFs = 1.0f / fs;
        const float dur = static_cast<float>(numSamples) * invFs;

        std::uniform_real_distribution<float> uniform01(0.0f, 1.0f);
        std::uniform_real_distribution<float> durDist(
            p_.crackleDurMinMs * 0.001f,
            p_.crackleDurMaxMs * 0.001f);

        const float fcMid = 0.5f * (p_.crackleFreqMin + p_.crackleFreqMax);
        const float a = std::exp(-TWO_PI_F * fcMid * invFs);
        float lp1 = 0.0f, lp2 = 0.0f;

        const float meanStep = 1.0f / p_.crackleRateHz;
        float t = 0.0f;
        while (t < dur)
        {
            const float jitter = 0.5f + uniform01(rng_);
            t += meanStep * jitter;

            const int idx = static_cast<int>(t * fs);
            if (idx < 0 || idx >= numSamples) continue;

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

                buffer[i] += band * env * p_.crackleLevel * 6.0f;
            }
        }
    }

    // ============================================================
    //  Низ — тело свиста.
    // ============================================================
    void DolphinWhistleTest::renderLow(std::vector<float>& buffer,
                                       int numSamples)
    {
        const float fs = p_.fs;
        const float invFs = 1.0f / fs;
        const float fTone = p_.lowFreqHz;

        for (int n = 0; n < numSamples; ++n)
        {
            const float t = static_cast<float>(n) * invFs;
            const float s = std::sin(TWO_PI_F * fTone * t);
            buffer[n] += s * p_.lowLevel;
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

        // 1. Поток переливов.
        std::vector<Transition> trans = generateTransitions(numSamples);
        for (const auto& t : trans)
            renderTransition(buffer, numSamples, t);

        // 2. Треск поверх всего.
        renderCrackle(buffer, numSamples);

        // 3. Низ — тело свиста.
        renderLow(buffer, numSamples);

        // 4. Нормировка по пику к 0.95.
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