#pragma once
#include <vector>
#include <cstdint>
#include <random>

// ============================================================
//  DolphinWhistleTest — модель свиста дельфина.
//
//  v9: СВИСТ + ЭХО + СТРЁКОТ (импульсный).
//
//  Структура:
//    1. Начальная тишина.
//    2. Серия фраз (нисходящие + U-образный + восходящий).
//    3. Каждая фраза:
//       - основной тон с ЧМ и вибрато;
//       - 3 гармоники (2×, 3×, 4×);
//       - огибающая с плавным входом и выходом.
//    4. Эхо каждой фразы — задержка 80 мс.
//    5. Стрёкот — серия коротких звонких импульсов в конце фразы.
//    6. Конечная тишина.
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

        // --- Тембр ---
        int numHarmonics = 3;
        float harm1Db = -12.0f;
        float harm2Db = -18.0f;
        float harm3Db = -24.0f;
        float harm4Db = -30.0f;

        // --- Вибрато ---
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

        // --- Эхо ---
        int echoCount = 1;
        float echoDelayMinMs = 80.0f;
        float echoDelayMaxMs = 80.0f;
        float echoGain1Db = -15.0f;
        float echoGain2Db = -22.0f;
        float echoGain3Db = -28.0f;
        float echoPitchFactor = 0.98f;
        float echoStretch = 1.0f;

        // --- Стрёкот: НОВАЯ АРХИТЕКТУРА (пульсирующий звонкий тембр).
        //
        //  Не серия импульсов, а непрерывный сигнал:
        //    carrier = сумма гармоник от crackleBaseHz;
        //    AM      = пульсация с cracklePulseHz;
        //    env     = огибающая по краям серии.
        //
        //  Стрёкот «накладывается» на конец фразы:
        //  начинается с crackleStartU, идёт до конца фразы.
        // ---
        float crackleProb = 1.0f;
        float crackleLevel = 1.0f; // подобрать

        // Пульсация (трещание).
        float cracklePulseHzMin = 30.0f; // Гц
        float cracklePulseHzMax = 50.0f;
        float cracklePulseDepth = 0.7f; // глубина AM (0..1)

        // Тембр (звонкость).
        float crackleBaseHz = 300.0f; // основная тембра, Гц
        int crackleNumHarm = 8; // число гармоник тембра
        float crackleHarmDecay = 0.6f; // убывание гармоник

        // Где начинается стрёкот (доля фразы).
        float crackleStartU = 0.40f;

        // Огибающая серии (плавный вход/выход).
        float crackleRiseFrac = 0.10f; // 10% на вход
        float crackleFallFrac = 0.20f; // 20% на выход

        // --- СТАРЫЕ ПАРАМЕТРЫ (не используются, оставлены для совместимости) ---
        int crackleClickMin = 15;
        int crackleClickMax = 30;
        float crackleIntervalMinMs = 10.0f;
        float crackleIntervalMaxMs = 30.0f;
        float crackleDurMinMs = 0.5f;
        float crackleDurMaxMs = 1.5f;
        float crackleFreqMin = 1000.0f;
        float crackleFreqMax = 3000.0f;
        int crackleEchoCount = 2;
        float crackleEchoDelayMs = 400.0f;
        float crackleEchoGainDb = -60.0f;

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
