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
    //  Стрёкот — серия коротких звонких импульсов.
    //
    //  Каждый импульс:
    //    - основной тон 5000–9000 Гц;
    //    - 2×, 3×, 4× гармоники (звонкость);
    //    - короткая огибающая 1–3 мс;
    //    - эхо на 50 мс, −12 дБ.
    // ============================================================
    void DolphinWhistleTest::renderCrackleForPhrase(
        std::vector<float>& buffer, int numSamples, const Phrase& ph)
    {
        if (!ph.hasCrackle) return;
        if (p_.crackleLevel <= 0.0f) return;

        const float fs = p_.fs;
        const float invFs = 1.0f / fs;

        const float crackleStartU = 0.40f;
        const int crackleStart = ph.sampleStart
            + static_cast<int>(ph.lengthSamples * crackleStartU);
        const int crackleEnd = ph.sampleStart + ph.lengthSamples;
        std::uniform_real_distribution<float> uniform01(0.0f, 1.0f);

        std::uniform_int_distribution<int> nClicksDist(
            p_.crackleClickMin, p_.crackleClickMax);
        std::uniform_real_distribution<float> intervalDist(
            p_.crackleIntervalMinMs * 0.001f,
            p_.crackleIntervalMaxMs * 0.001f);
        std::uniform_real_distribution<float> durDist(
            p_.crackleDurMinMs * 0.001f,
            p_.crackleDurMaxMs * 0.001f);
        std::uniform_real_distribution<float> freqDist(
            p_.crackleFreqMin, p_.crackleFreqMax);
        std::uniform_real_distribution<float> phaseDist(0.0f, TWO_PI_F);
        std::uniform_real_distribution<float> noiseLevelDist(0.05f, 0.15f);

        const int nClicks = nClicksDist(rng_);
        const float echoGain = std::pow(10.0f,
                                        p_.crackleEchoGainDb / 20.0f);
        const int echoDelaySamples = static_cast<int>(
            p_.crackleEchoDelayMs * 0.001f * fs);

        float t = static_cast<float>(crackleStart) * invFs;
        const float tEnd = static_cast<float>(crackleEnd) * invFs;

        // --- Частота «едет» по серии ---
        const float f0Start = freqDist(rng_);
        const float f0End = f0Start * (1.0f + p_.crackleSweepFrac * uniform01(rng_));
        const float f0Step = (f0End - f0Start)
            / float(std::max(1, nClicks));

        for (int c = 0; c < nClicks && t < tEnd; ++c)
        {
            const int idx = static_cast<int>(t * fs);
            if (idx < 0 || idx >= numSamples) break;

            const float pulseDur = durDist(rng_);
            const int len = static_cast<int>(pulseDur * fs) + 1;
            const float f0 = f0Start + f0Step * float(c);
            const float phase0 = phaseDist(rng_);
            const float noiseLevel = noiseLevelDist(rng_);

            for (int k = 0; k < len; ++k)
            {
                const int i = idx + k;
                if (i < 0 || i >= numSamples) break;

                const float tL = static_cast<float>(k) * invFs;
                const float u = tL / pulseDur;

                float env = 1.0f;
                if (u < 0.15f)
                    env = u / 0.15f;
                else
                    env = std::exp(-(u - 0.15f) * 8.0f);

                float sig = std::sin(TWO_PI_F * f0 * tL + phase0);
                if (2.0f * f0 < fs * 0.45f)
                    sig += 0.9f * std::sin(TWO_PI_F * 2.0f * f0 * tL
                        + phase0 * 1.3f);
                if (3.0f * f0 < fs * 0.45f)
                    sig += 0.7f * std::sin(TWO_PI_F * 3.0f * f0 * tL
                        + phase0 * 0.7f);
                if (4.0f * f0 < fs * 0.45f)
                    sig += 0.5f * std::sin(TWO_PI_F * 4.0f * f0 * tL
                        + phase0 * 0.4f);

                const float noise = gauss_(rng_) * noiseLevel;

                const float s = (sig + noise) * env * p_.crackleLevel;

                buffer[i] += s;

                const int ie = i + echoDelaySamples;
                if (ie >= 0 && ie < numSamples)
                    buffer[ie] += s * echoGain;
            }

            t += intervalDist(rng_);
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
