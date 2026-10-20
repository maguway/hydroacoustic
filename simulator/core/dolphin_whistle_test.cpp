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
    //  Рендер одной фразы.
    //
    //  Основная F0 плавно едет по форме (нисходящая / восходящая / U).
    //  В хвосте (u > tailStartFrac) F0 уходит в низ (до fEnd × tailEndFrac).
    //  На каждом отсчёте — 3 гармоники (2×, 3×, 4×) с базовыми
    //  уровнями harm1Db/harm2Db/harm3Db.
    // ============================================================
    void DolphinWhistleTest::renderPhrase(std::vector<float>& buffer,
                                          int numSamples,
                                          Phrase& ph)
    {
        if (ph.lengthSamples <= 0) return;

        const float fs = p_.fs;
        const float invFs = 1.0f / fs;
        const float durSec = static_cast<float>(ph.lengthSamples) * invFs;

        const float h1 = std::pow(10.0f, p_.harm1Db / 20.0f);
        const float h2 = std::pow(10.0f, p_.harm2Db / 20.0f);
        const float h3 = std::pow(10.0f, p_.harm3Db / 20.0f);

        float phaseMain = ph.phase;
        float phaseH[3] = {
            ph.phase * 0.7f, ph.phase * 0.3f, ph.phase * 0.5f
        };

        ph.fInstTrace.assign(ph.lengthSamples, 0.0f);

        for (int n = 0; n < ph.lengthSamples; ++n)
        {
            const int idx = ph.sampleStart + n;
            if (idx < 0 || idx >= numSamples) continue;

            const float tLocal = static_cast<float>(n) * invFs;
            const float u = tLocal / durSec;

            // --- Основная F0 по форме ---
            float fBase;
            const float s = 0.5f * (1.0f - std::cos(PI_F * u));

            if (ph.shape == PhraseShape::Descending)
                fBase = ph.fStart + (ph.fEnd - ph.fStart) * s;
            else if (ph.shape == PhraseShape::Ascending)
                fBase = ph.fEnd + (ph.fStart - ph.fEnd) * s;
            else // UShape
            {
                const float mid = 0.5f * (ph.fStart + ph.fEnd);
                if (u < 0.5f)
                {
                    const float v = u * 2.0f;
                    const float sv = 0.5f * (1.0f - std::cos(PI_F * v));
                    fBase = ph.fStart + (mid - ph.fStart) * sv;
                }
                else
                {
                    const float v = (u - 0.5f) * 2.0f;
                    const float sv = 0.5f * (1.0f - std::cos(PI_F * v));
                    fBase = mid + (ph.fStart - mid) * sv;
                }
            }

            // --- Хвост: уход в низ ---
            float fInst = fBase;
            const bool inTail = (u > p_.tailStartFrac);
            if (inTail)
            {
                const float v = (u - p_.tailStartFrac)
                    / (1.0f - p_.tailStartFrac);
                const float sv = 0.5f * (1.0f - std::cos(PI_F * v));
                const float fTail = ph.fEnd * p_.tailEndFrac;
                fInst = fBase + (fTail - fBase) * sv;
            }

            // --- Вибрато ---
            const float vibMain = p_.vibDepthHz
                * std::sin(TWO_PI_F * p_.vibRateHz * tLocal);
            fInst += vibMain;

            ph.fInstTrace[n] = fInst;

            // --- Огибающая ---
            float env = 1.0f;
            if (u < p_.riseFrac)
                env = 0.5f * (1.0f - std::cos(PI_F * u / p_.riseFrac));
            else if (u > 1.0f - p_.fallFrac)
            {
                const float v = (1.0f - u) / p_.fallFrac;
                env = 0.5f * (1.0f - std::cos(PI_F * v));
            }

            // --- AM ---
            const float am = 1.0f
                - p_.amDepth * 0.5f
                + p_.amDepth * 0.5f
                * std::sin(TWO_PI_F * p_.amRateHz * tLocal);

            // --- Основной тон ---
            phaseMain += TWO_PI_F * fInst * invFs;
            if (phaseMain > TWO_PI_F) phaseMain -= TWO_PI_F;
            float sig = std::sin(phaseMain);

            // Гармоники 2×, 3×, 4× — простые, тихие.
            // Стрёкот делается отдельно, в renderCrackleForPhrase.
            const float gains[3] = {h1, h2, h3};
            for (int k = 0; k < 3; ++k)
            {
                const float fH = fInst * float(k + 2);
                if (fH >= fs * 0.45f) break;

                phaseH[k] += TWO_PI_F * fH * invFs;
                if (phaseH[k] > TWO_PI_F) phaseH[k] -= TWO_PI_F;

                sig += gains[k] * std::sin(phaseH[k]);
            }

            buffer[idx] += sig * env * am;
        }
    }

    // ============================================================
    //  Эхо одной фразы.
    //  Повторяет F0-траекторию, но тише, чуть ниже и растянуто.
    // ============================================================
    void DolphinWhistleTest::renderEchoForPhrase(std::vector<float>& buffer,
                                                 int numSamples,
                                                 const Phrase& ph)
    {
        if (ph.fInstTrace.empty()) return;

        const float fs = p_.fs;
        const float invFs = 1.0f / fs;

        const int nEcho = std::max(1, p_.echoCount);
        const float gains[3] = {
            std::pow(10.0f, p_.echoGain1Db / 20.0f),
            std::pow(10.0f, p_.echoGain2Db / 20.0f),
            std::pow(10.0f, p_.echoGain3Db / 20.0f)
        };

        for (int e = 0; e < nEcho && e < 1; ++e)
        {
            const float delay = p_.echoDelayMinMs * 0.001f;
            const int delaySamples = static_cast<int>(delay * fs);
            const float gain = gains[e];
            const float pitch = p_.echoPitchFactor;
            const float stretch = p_.echoStretch;

            const int echoLen = static_cast<int>(ph.lengthSamples * stretch);
            const float invEchoDur = 1.0f
                / (static_cast<float>(echoLen) * invFs);

            float phaseMain = ph.phase * 0.5f;
            float phaseH[2] = {ph.phase * 0.3f, ph.phase * 0.8f};

            for (int n = 0; n < echoLen; ++n)
            {
                const int idx = ph.sampleStart + delaySamples + n;
                if (idx < 0 || idx >= numSamples) continue;

                const float tLocal = static_cast<float>(n) * invFs;
                const float u = tLocal * invEchoDur;

                const float srcU = u / stretch;
                if (srcU < 0.0f || srcU >= 1.0f) continue;
                const int srcIdx = static_cast<int>(srcU
                    * static_cast<float>(ph.lengthSamples));
                if (srcIdx < 0
                    || srcIdx >= int(ph.fInstTrace.size()))
                    continue;

                const float fInst = ph.fInstTrace[srcIdx] * pitch;
                const float env = std::exp(-u * 1.5f);

                phaseMain += TWO_PI_F * fInst * invFs;
                if (phaseMain > TWO_PI_F) phaseMain -= TWO_PI_F;
                float sig = std::sin(phaseMain);

                for (int k = 0; k < 2 && k < p_.numHarmonics; ++k)
                {
                    const float fH = fInst * float(k + 2);
                    if (fH >= fs * 0.45f) break;
                    phaseH[k] += TWO_PI_F * fH * invFs;
                    if (phaseH[k] > TWO_PI_F) phaseH[k] -= TWO_PI_F;
                    const float a = 0.4f / float(k + 1);
                    sig += a * std::sin(phaseH[k]);
                }

                buffer[idx] += sig * env * gain;
            }
        }
    }

    // ============================================================
    //  Стрёкот — НОВАЯ АРХИТЕКТУРА.
    //
    //  Пульсирующий звонкий тембр в конце фразы.
    //
    //  carrier(t) = сумма гармоник от crackleBaseHz:
    //                 f0, 2*f0, 3*f0, ... до crackleNumHarm
    //                 с убыванием амплитуд (crackleHarmDecay^k)
    //
    //  AM(t) = 1 - depth/2 + depth/2 * sin(2*pi*pulseHz*t)
    //          где pulseHz — случайно в [cracklePulseHzMin, cracklePulseHzMax]
    //
    //  env(t) — плавный вход (crackleRiseFrac) и выход (crackleFallFrac)
    //          по краям серии.
    //
    //  Серия начинается с crackleStartU фразы и идёт до конца фразы.
    //  Накладывается на свист — не заменяет его.
    // ============================================================
    // ============================================================
    //  Стрёкот — ШУМОВАЯ АРХИТЕКТУРА.
    //
    //  Не тон. Шум, пропущенный через BPF 4–8 кГц.
    //  Амплитуда пульсирует серией коротких гауссовых импульсов
    //  с частотой 30–60 Гц. Каждый импульс — 1–3 мс.
    //
    //  Это и есть «треск».
    // ============================================================
    void DolphinWhistleTest::renderCrackleForPhrase(
        std::vector<float>& buffer, int numSamples, const Phrase& ph)
    {
        if (!ph.hasCrackle) return;
        if (p_.crackleLevel <= 0.0f) return;

        const float fs = p_.fs;
        const float invFs = 1.0f / fs;

        // Начало стрёкота — с crackleStartU фразы, до конца.
        const int crackleStart = ph.sampleStart
            + static_cast<int>(ph.lengthSamples * p_.crackleStartU);
        const int crackleEnd = ph.sampleStart + ph.lengthSamples;

        const int totalLen = crackleEnd - crackleStart;
        if (totalLen <= 0) return;

        const float totalDurSec = static_cast<float>(totalLen) * invFs;

        // --- Параметры серии (случайные) ---
        std::uniform_real_distribution<float> pulseDist(
            p_.cracklePulseHzMin, p_.cracklePulseHzMax);
        std::uniform_real_distribution<float> durDist(
            p_.cracklePulseDurMinMs * 0.001f,
            p_.cracklePulseDurMaxMs * 0.001f);

        const float pulseHz = pulseDist(rng_);
        const float meanPeriod = 1.0f / pulseHz;

        // BPF: два однополюсника (разностный BP).
        const float fcMid = 0.5f * (p_.crackleBpfLowHz
            + p_.crackleBpfHighHz);
        const float a = std::exp(-TWO_PI_F * fcMid * invFs);
        float lp1 = 0.0f, lp2 = 0.0f;

        // --- Огибающая пульсации: серия гауссовых импульсов ---
        // Заранее генерируем моменты импульсов.
        std::vector<float> pulseCenters;
        {
            float t = 0.0f;
            while (t < totalDurSec)
            {
                pulseCenters.push_back(t);
                t += meanPeriod;
            }
        }

        // --- Рендер ---
        for (int n = 0; n < totalLen; ++n)
        {
            const int idx = crackleStart + n;
            if (idx < 0 || idx >= numSamples) continue;

            const float tLocal = static_cast<float>(n) * invFs;
            const float u = tLocal / totalDurSec;

            // --- Огибающая серии (плавный вход/выход) ---
            float env = 1.0f;
            if (u < p_.crackleRiseFrac)
                env = 0.5f * (1.0f - std::cos(PI_F * u
                    / p_.crackleRiseFrac));
            else if (u > 1.0f - p_.crackleFallFrac)
            {
                const float v = (1.0f - u) / p_.crackleFallFrac;
                env = 0.5f * (1.0f - std::cos(PI_F * v));
            }

            // --- Пульсирующая огибающая (серия гауссовых импульсов) ---
            // Для каждого импульса — гауссова огибающая.
            float pulseEnv = 0.0f;
            for (float pc : pulseCenters)
            {
                const float dt = tLocal - pc;
                if (dt < -0.005f || dt > 0.005f) continue;
                const float dur = 0.001f; // 1 мс — оценочно
                const float tau = dur / 2.0f;
                const float invTau2 = 1.0f / (tau * tau + 1e-9f);
                const float g = std::exp(-(dt * dt) * invTau2);
                if (g > pulseEnv) pulseEnv = g;
            }

            if (pulseEnv < 1e-4f) continue;

            // --- Шумовой источник ---
            const float white = gauss_(rng_);
            lp1 = a * lp1 + (1.0f - a) * white;
            lp2 = a * lp2 + (1.0f - a) * lp1;
            const float band = lp1 - lp2; // BPF-выход

            // --- Итог ---
            buffer[idx] += band * pulseEnv * env * p_.crackleLevel;
        }
    }

    // ============================================================
    //  Шум моря — очень тихий, HP-фильтр (без НЧ-гула).
    // ============================================================
    void DolphinWhistleTest::renderSeaNoise(std::vector<float>& buffer,
                                            int numSamples)
    {
        if (p_.seaNoiseLevel <= 0.0f) return;

        const float fs = p_.fs;
        const float invFs = 1.0f / fs;
        const float fc = p_.seaNoiseHP;
        const float a = std::exp(-TWO_PI_F * fc * invFs);
        float lp = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            const float t = static_cast<float>(n) * invFs;

            const float white = gauss_(rng_);
            lp = a * lp + (1.0f - a) * white;
            const float hp = white - lp;

            const float am = 1.0f
                - p_.seaNoiseAmDepth * 0.5f
                + p_.seaNoiseAmDepth * 0.5f
                * std::sin(TWO_PI_F * p_.seaNoiseAmHz * t);

            buffer[n] += hp * am * p_.seaNoiseLevel;
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

        const float fs = p_.fs;
        const float totalDurSec = static_cast<float>(numSamples) / fs;

        std::uniform_real_distribution<float> phraseDurDist(
            p_.phraseDurMin, p_.phraseDurMax);
        std::uniform_real_distribution<float> pauseDist(
            p_.pauseMinMs * 0.001f, p_.pauseMaxMs * 0.001f);
        std::uniform_real_distribution<float> fStartDist(
            p_.fStartMin, p_.fStartMax);
        std::uniform_real_distribution<float> fEndDist(
            p_.fEndMin, p_.fEndMax);
        std::uniform_real_distribution<float> phaseDist(0.0f, TWO_PI_F);
        std::uniform_real_distribution<float> startSilenceDist(
            p_.startSilenceMin, p_.startSilenceMax);
        std::uniform_real_distribution<float> endSilenceDist(
            p_.endSilenceMin, p_.endSilenceMax);
        std::uniform_real_distribution<float> uniform01(0.0f, 1.0f);

        const float tStart = startSilenceDist(rng_);
        const float tEnd = totalDurSec - endSilenceDist(rng_);

        std::vector<Phrase> phrases;

        // --- Основные фразы ---
        float t = tStart;
        while (t < tEnd && int(phrases.size()) < p_.phraseCount)
        {
            const float phraseDur = phraseDurDist(rng_);
            if (t + phraseDur > tEnd) break;

            Phrase ph;
            ph.sampleStart = static_cast<int>(t * fs);
            ph.lengthSamples = static_cast<int>(phraseDur * fs);
            ph.fStart = fStartDist(rng_);
            ph.fEnd = fEndDist(rng_);
            ph.phase = phaseDist(rng_);
            ph.hasCrackle = (uniform01(rng_) < p_.crackleProb);
            ph.shape = PhraseShape::Descending;

            phrases.push_back(ph);
            t += phraseDur + pauseDist(rng_);
        }

        // --- Альтернативные фразы (накладываются) ---
        for (int k = 0; k < p_.altPhraseCount; ++k)
        {
            const float phraseDur = phraseDurDist(rng_);
            const float tRange = tEnd - tStart - phraseDur;
            if (tRange <= 0.0f) break;
            const float tPos = tStart + uniform01(rng_) * tRange;

            Phrase ph;
            ph.sampleStart = static_cast<int>(tPos * fs);
            ph.lengthSamples = static_cast<int>(phraseDur * fs);
            ph.fStart = fStartDist(rng_);
            ph.fEnd = fEndDist(rng_);
            ph.phase = phaseDist(rng_);
            ph.hasCrackle = (uniform01(rng_) < p_.crackleProb);
            ph.shape = (k % 2 == 0)
                           ? PhraseShape::UShape
                           : PhraseShape::Ascending;

            phrases.push_back(ph);
        }

        renderSeaNoise(buffer, numSamples);

        for (auto& ph : phrases)
        {
            renderPhrase(buffer, numSamples, ph);
            renderEchoForPhrase(buffer, numSamples, ph);
            renderCrackleForPhrase(buffer, numSamples, ph);
        }

        // --- Нормализация по пику к 0.95 ---
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
