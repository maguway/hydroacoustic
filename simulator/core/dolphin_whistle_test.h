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


        // --- Стрёкот: ШУМОВАЯ АРХИТЕКТУРА.
        //
        //  Не тон. Шум. BPF 4–8 кГц + пульсирующая огибающая
        //  с частотой 30–60 Гц. Это — «треск».
        // ---
        // ============================================================
        //  Стрёкот — параметры.
        //  Модель: шум (сумма синусов в полосе 4–10 кГц)
        //          × пульсация (серия коротких импульсов)
        //          × огибающая серии.
        // ============================================================

        // Вероятность, что фраза закончится стрёкотом.
        //   1.0 = всегда, 0.5 = в половине фраз, 0.0 = никогда.
        float crackleProb = 1.0f;

        // Общая громкость стрёкота.
        //   Больше → громче. Меньше → тише.
        //   Начните с 1.0. Если тихо — 2.0. Если громко — 0.5.
        float crackleLevel = 2.0f;

        // Нижняя граница полосы шумового источника (Гц).
        //   Это — «низ» тембра стрёкота.
        //   Меньше → глуше, больше → выше.
        //   4000 — хорошая отправная точка.
        float crackleBpfLowHz = 2000.0f;

        // Верхняя граница полосы шумового источника (Гц).
        //   Это — «верх» тембра. Чем выше — тем звонче.
        //   Внимание: не должна превышать 11025 Гц
        //   (половина частоты дискретизации 22050).
        //   10000 — близко к максимуму, очень звонко.
        //   Если «слишком звонко» — 9000.
        float crackleBpfHighHz = 9000.0f;

        // Частота пульсации (трещания) — МИНИМУМ (Гц).
        //   Это — «как часто идут щелчки».
        //   Больше → чаще. Меньше → реже.
        //   50 — 50 щелчков в секунду.
        float cracklePulseHzMin = 50.0f;

        // Частота пульсации (трещания) — МАКСИМУМ (Гц).
        //   Каждая серия получает случайную частоту
        //   в диапазоне [Min, Max].
        //   Если хотите одинаковую частоту — сделайте Min = Max.
        float cracklePulseHzMax = 80.0f;

        // Длительность ОДНОГО импульса (щелчка) — МИНИМУМ (мс).
        //   Это — «как долго звучит каждый щелчок».
        //   Меньше → короче, резче. Больше → длиннее, мягче.
        //   1.0 мс — резко. 5.0 мс — мягко.
        float cracklePulseDurMinMs = 1.0f;

        // Длительность ОДНОГО импульса — МАКСИМУМ (мс).
        //   Каждый импульс получает случайную длительность
        //   в диапазоне [Min, Max].
        float cracklePulseDurMaxMs = 3.0f;

        // С какого момента фразы начинается стрёкот (доля 0..1).
        //   0.40 = с 40% длительности фразы до её конца.
        //   Меньше → длиннее стрёкот. Больше → короче.
        float crackleStartU = 0.40f;

        // Плавность входа стрёкота (доля 0..1).
        //   0.10 = 10% от длительности серии — на плавный вход.
        //   Больше → мягче вход. Меньше → резче.
        float crackleRiseFrac = 0.10f;

        // Плавность выхода стрёкота (доля 0..1).
        //   0.20 = 20% от длительности серии — на плавный выход.
        float crackleFallFrac = 0.20f;

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
