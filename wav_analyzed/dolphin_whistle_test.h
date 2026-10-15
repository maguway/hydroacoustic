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
        float fs = 22050.0f;
        float durationSec = 5.0f;

        // --- Свист: диапазон ---
        float fMin = 1500.0f;
        float fMax = 8000.0f;

        // --- Свист: ЧМ-модуляция (сумма трёх синусов) ---
        float sweepSlowHz = 0.35f; // медленная (период ~3 с)
        float sweepSlowDepth = 1500.0f;
        float sweepMidHz = 1.1f; // средняя (период ~0.9 с)
        float sweepMidDepth = 700.0f;
        float sweepFastHz = 4.5f; // быстрая (период ~0.2 с)
        float sweepFastDepth = 250.0f;

        // --- Свист: огибающая ---
        float ampBase = 0.7f;
        float ampDepth = 0.25f;
        float ampRateHz = 0.8f;

        // --- Свист: паузы ---
        int pauseCount = 3; // сколько пауз за файл
        float pauseMinMs = 80.0f;
        float pauseMaxMs = 180.0f;
        float pauseEdgeMs = 20.0f; // плавный вход/выход паузы

        // --- Шум моря (тихий, с наклоном в НЧ) ---
        float seaNoiseLevel = 0.012f;
        float seaNoiseAmDepth = 0.4f;
        float seaNoiseAmHz = 0.15f;

        // --- Стрёкот: редкие одиночные ВЧ-вспышки ---
        float crackleLevel = 0.25f;
        float crackleRateHz = 3.0f;
        float crackleDurMinMs = 1.0f;
        float crackleDurMaxMs = 3.0f;
        float crackleFreqMin = 8000.0f;
        float crackleFreqMax = 14000.0f;

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

        // Единый непрерывный свист с ЧМ-траекторией.
        void renderWhistle(std::vector<float>& buffer, int numSamples);

        // Тихий шум моря.
        void renderSeaNoise(std::vector<float>& buffer, int numSamples);

        // Редкие одиночные ВЧ-вспышки.
        void renderCrackle(std::vector<float>& buffer, int numSamples);
    };
} // namespace hydro
