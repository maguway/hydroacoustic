#pragma once
#include <vector>
#include <cstdint>
#include <random>

// ============================================================
//  DolphinWhistleTest — модель свиста дельфина.
//
//  v8: БОГАТЫЙ ТЕМБР + ЭХО + РАЗМЕРЕННЫЙ СТРЁКОТ.
//
//  Структура:
//    1. Начальная тишина.
//    2. Серия фраз (нисходящие + U-образный + восходящий).
//    3. Каждая фраза:
//       - основной тон с вибрато;
//       - насыщенный спектр: целые + дробные гармоники;
//       - узкополосный шум вокруг гармоник;
//       - хвост (уход в низ) с эхом.
//    4. Стрёкот: размеренный, с эхо на каждом щелчке.
//    5. Конечная тишина.
// ============================================================

namespace hydro
{
    enum class PhraseShape
    {
        Descending,
        Ascending,
        UShape,
    };

    struct DolphinWhistleTestParams
    {
        float fs = 22050.0f;
        float durationSec = 5.0f;

        // --- Фразы ---
        int phraseCount = 4;
        int altPhraseCount = 2;
        float phraseDurMin = 0.40f;
        float phraseDurMax = 0.90f;

        // --- F0 ---
        float fStartMin = 2000.0f;
        float fStartMax = 3000.0f;
        float fEndMin = 800.0f;
        float fEndMax = 1200.0f;

        // --- Хвост ---
        float tailStartFrac = 0.50f;
        float tailEndFrac = 0.35f;

        // --- Огибающая ---
        float riseFrac = 0.15f;
        float fallFrac = 0.25f;

        // --- Тембр (гармоники громче — как в эталоне) ---
        int numHarmonics = 3;
        float harm1Db = -6.0f; // 2× — почти как основная
        float harm2Db = -9.0f; // 3× — тоже громко
        float harm3Db = -13.0f; // 4× — заметно
        float harm4Db = -18.0f;
        float fracHarm1Db = -100.0f; // отключено
        float fracHarm2Db = -100.0f; // отключено
        float noiseAroundDb = -100.0f; // отключено
        float noiseBandHz = 60.0f;

        // --- Вибрато (мягче) ---
        float vibDepthHz = 10.0f;
        float vibRateHz = 6.0f;

        // --- AM ---
        float amDepth = 0.10f;
        float amRateHz = 3.0f;

        // --- Паузы ---
        float pauseMinMs = 200.0f;
        float pauseMaxMs = 500.0f;

        // --- Тишина ---
        float startSilenceMin = 0.3f;
        float startSilenceMax = 0.6f;
        float endSilenceMin = 0.3f;
        float endSilenceMax = 0.6f;

        // --- Эхо (одно, плотное) ---
        int echoCount = 1;
        float echoDelayMinMs = 80.0f;
        float echoDelayMaxMs = 80.0f;
        float echoGain1Db = -15.0f;
        float echoGain2Db = -22.0f;
        float echoGain3Db = -28.0f;
        float echoPitchFactor = 0.98f;
        float echoStretch = 1.0f;

        // --- Стрёкот: ОТКЛЮЧЁН. Стрёкот — это хвост свиста
        // с гармониками, а не импульсы. ---
        float crackleProb = 0.0f;
        float crackleLevel = 0.0f;
        int crackleClickMin = 0;
        int crackleClickMax = 0;
        float crackleIntervalMinMs = 50.0f;
        float crackleIntervalMaxMs = 150.0f;
        float crackleDurMinMs = 0.8f;
        float crackleDurMaxMs = 2.5f;
        float crackleFreqMin = 4000.0f;
        float crackleFreqMax = 8000.0f;
        int crackleEchoCount = 2;
        float crackleEchoDelayMs = 50.0f;
        float crackleEchoGainDb = -12.0f;

        // --- Шум моря ---
        float seaNoiseLevel = 0.004f;
        float seaNoiseHP = 2000.0f;
        float seaNoiseAmDepth = 0.3f;
        float seaNoiseAmHz = 0.15f;

        uint32_t seed = 77777;
    };

    class DolphinWhistleTest
    {
    public:
        explicit DolphinWhistleTest(const DolphinWhistleTestParams& p = {});
        void setParams(const DolphinWhistleTestParams& p);
        const DolphinWhistleTestParams& params() const { return p_; }
        void generate(float* out, int numSamples);

    private:
        DolphinWhistleTestParams p_;
        std::mt19937 rng_;
        std::normal_distribution<float> gauss_{0.0f, 1.0f};

        struct Phrase
        {
            int sampleStart = 0;
            int lengthSamples = 0;
            float fStart = 2500.0f;
            float fEnd = 1000.0f;
            float phase = 0.0f;
            bool hasCrackle = false;
            PhraseShape shape = PhraseShape::Descending;

            // Сохраняем траекторию F0 для эха.
            std::vector<float> fInstTrace;
        };

        void renderPhrase(std::vector<float>& buffer, int numSamples,
                          Phrase& ph);
        void renderEchoForPhrase(std::vector<float>& buffer, int numSamples,
                                 const Phrase& ph);
        void renderCrackleForPhrase(std::vector<float>& buffer, int numSamples,
                                    const Phrase& ph);
        void renderSeaNoise(std::vector<float>& buffer, int numSamples);
    };
} // namespace hydro
