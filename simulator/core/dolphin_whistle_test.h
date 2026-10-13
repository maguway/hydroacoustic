#pragma once
#include <vector>
#include <cstdint>
#include <random>

// ============================================================
//  DolphinWhistleTest — модель свиста дельфина.
//
//  АРХИТЕКТУРА ЭТАПА 3: ПОТОК КОРОТКИХ ПЕРЕЛИВОВ.
//
//  Не один длинный U-перелив, а МНОГО коротких (0.1–0.4 с),
//  идущих встык. Направление каждого — своё: вверх, вниз,
//  U, ∩, с изломом. Диапазоны разные. Поверх — треск
//  (шершавые короткие всплески), вплетённый в свист.
//
//  Три фазы сигнала:
//    1) начало:  «два спуска сверху вниз» + короткий подъём;
//    2) середина: рваное хаотичное движение;
//    3) конец:   снова спуски, длинный затихающий.
//
//  Формат: моно, 22050 Гц, 32-bit float.
// ============================================================

namespace hydro
{
    struct DolphinWhistleTestParams
    {
        // --- Общие ---
        float fs = 22050.0f;
        float durationSec = 5.0f;

        // --- Свист: диапазон частот ---
        float fMin = 300.0f; // нижняя граница
        float fMax = 6000.0f; // верхняя граница

        // --- Переливы ---
        int numTransitionsMin = 18; // минимум переливов за файл
        int numTransitionsMax = 30; // максимум
        float transDurMin = 0.10f; // мин. длительность перелива, сек
        float transDurMax = 0.40f; // макс.

        // Доля наложения между переливами (0 = встык, 0.3 = наложение).
        float overlapFrac = 0.15f;

        // --- Амплитуда (дыхание) ---
        float ampBase = 0.6f; // средний уровень
        float ampDepth = 0.4f; // глубина дыхания
        float ampRateHz = 1.2f; // частота дыхания

        // --- Треск ---
        // Не белый шум, а короткие шершавые всплески.
        float crackleLevel = 0.12f; // уровень треска
        float crackleRateHz = 40.0f; // средняя частота всплесков
        float crackleDurMinMs = 0.5f; // длительность всплеска, мс
        float crackleDurMaxMs = 2.5f;
        float crackleFreqMin = 2000.0f;
        float crackleFreqMax = 5000.0f;

        // --- Низ (тело свиста) ---
        float lowFreqHz = 100.0f;
        float lowLevel = 0.15f;

        // --- Seed ---
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

        struct Transition
        {
            int sampleStart = 0;
            int lengthSamples = 0;
            float f0 = 1000.0f; // начальная частота
            float f1 = 2000.0f; // конечная частота
            float fMid = 0.0f; // промежуточная (для U/∩) — 0 если не нужно
            int shape = 0; // 0=up, 1=down, 2=U, 3=capU, 4=broken
            float phase = 0.0f;
            float amp = 1.0f;
        };

        // Сгенерировать список переливов.
        std::vector<Transition> generateTransitions(int numSamples);

        // Рендер одного перелива.
        void renderTransition(std::vector<float>& buffer,
                              int numSamples,
                              const Transition& t);

        // Треск — поверх всего.
        void renderCrackle(std::vector<float>& buffer,
                           int numSamples);

        // Низ — тело свиста.
        void renderLow(std::vector<float>& buffer,
                       int numSamples);
    };
} // namespace hydro
