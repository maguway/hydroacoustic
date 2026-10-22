#pragma once
#include "types.h"
#include "math_constants.h"
#include "dolphin_whistle_test.h"
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace hydro
{
    // ============================================================
    //  Модель источника: базовый интерфейс.
    //
    //  Источник генерирует МОНО-СИГНАЛ, который ВКЛЮЧАЕТ:
    //    - сигнал (гармоники),
    //    - встроенный шум (кавитация для винта, мех.шум для двигателя),
    //    - внутренний шум (по SNR).
    //
    //  Всё вместе идёт через propagation — с ОДНИМ азимутом источника.
    //
    //  Глобальные шумы (океан, приёмник) добавляются в Scene — ОТДЕЛЬНО.
    // ============================================================

    class SourceModel
    {
    public:
        virtual ~SourceModel() = default;

        virtual void generate(float* out,
                              int numSamples,
                              float fs,
                              int64_t sampleOffset) = 0;

        virtual std::string name() const = 0;
        virtual void reset() = 0;
    };

    // ============================================================
    //  Винт ПЛ.
    //  Сигнал: гармоники k*f_blade (A_k = 1/k) + амплитудная модуляция.
    //  Встроенный шум: кавитация (широкополосный) + белый (по SNR).
    // ============================================================

    struct PropellerParams
    {
        int numBlades = 7; // число лопастей
        float rpm = 68.0f; // обороты вала, об/мин
        float depthM = 100.0f; // глубина (задел)
        int numHarmonics = 6; // число гармоник
        float modulationDepth = 0.2f; // глубина AM модуляции
        float modulationHz = 0.3f; // частота модуляции, Гц

        // --- Встроенный шум ---
        float cavitation = 0.3f; // 0..1, широкополосный шум кавитации
        float snrDb = 30.0f; // дБ, внутренний белый шум (120 = выкл)

        uint32_t seed = 12345;

        float bladeFreqHz() const
        {
            return static_cast<float>(numBlades) * rpm / 60.0f;
        }
    };

    class PropellerSubmarine : public SourceModel
    {
    public:
        explicit PropellerSubmarine(const PropellerParams& p = {});

        void generate(float* out, int numSamples, float fs, int64_t sampleOffset) override;

        std::string name() const override { return "PropellerSub"; }
        void reset() override;

        const PropellerParams& params() const { return p_; }
        void setParams(const PropellerParams& p);

    private:
        PropellerParams p_;
        std::mt19937 rng_;
        std::normal_distribution<float> gauss_{0.0f, 1.0f};
        std::vector<float> harmonicPhases_;
        std::vector<float> harmonicAmplitudes_;

        void regeneratePhases();
    };

    // ============================================================
    //  Двигатель ПЛ.
    //  Сигнал: гармоники k*f_engine + резонансы корпуса (120/240/480 Гц).
    //  Встроенный шум: механический (широкополосный) + белый (по SNR).
    // ============================================================

    struct EngineParams
    {
        float rpm = 300.0f; // обороты двигателя, об/мин
        int numHarmonics = 4; // число гармоник
        int numResonances = 3; // число резонансов (0..3)

        // --- Встроенный шум ---
        float mechNoiseLevel = 0.2f; // 0..1, широкополосный мех.шум
        float snrDb = 30.0f; // дБ, внутренний белый шум

        uint32_t seed = 54321;

        float engineFreqHz() const { return rpm / 60.0f; }
    };

    class EngineSubmarine : public SourceModel
    {
    public:
        explicit EngineSubmarine(const EngineParams& p = {});

        void generate(float* out, int numSamples, float fs, int64_t sampleOffset) override;

        std::string name() const override { return "EngineSub"; }
        void reset() override;

        const EngineParams& params() const { return p_; }
        void setParams(const EngineParams& p);

    private:
        EngineParams p_;
        std::mt19937 rng_;
        std::normal_distribution<float> gauss_{0.0f, 1.0f};
        std::vector<float> harmonicPhases_;
        std::vector<float> harmonicAmplitudes_;

        // Фиксированные частоты и амплитуды резонансов корпуса.
        static constexpr int RES_FREQS[3] = {120, 240, 480};
        static constexpr float RES_AMPS[3] = {0.15f, 0.10f, 0.07f};

        void regeneratePhases();
    };

    // ============================================================
    //  Дельфин-афалина (Tursiops truncatus).
    //
    //  Сигнал состоит из трёх компонент:
    //    1. Свисты (whistles) — коммуникация, 2–10 кГц, ЧМ,
    //       длительность 0.4–0.8 сек, 2–4 в секунду.
    //    2. Щелчки (clicks) — эхолокация, короткие импульсы,
    //       редкими сериями (2–5 в серии, серия раз в 1–3 сек).
    //    3. Burst pulses — быстрые серии щелчков, сливающихся
    //       в «жужжание». Реакция возбуждения/агрессии.
    //
    //  Встроенный шум: белый по SNR.
    //  Амплитудной модуляции нет — она дельфину не свойственна.
    //
    //  Ограничение fs: при SAMPLE_RATE = 22050 (Найквист 11025 Гц)
    //  свисты укладываются почти полностью (верхние 11–20 кГц
    //  обрезаются — это слабая часть свиста). Настоящие
    //  эхолокационные щелчки (до 100+ кГц) не укладываются —
    //  моделируются суррогатные щелчки в диапазоне 1–8 кГц.
    // ============================================================

    struct DolphinParams
    {
        // --- 1. Свисты ---
        int numWhistlesPerSec = 3; // 2..5
        float whistleDurationMin = 0.4f; // сек
        float whistleDurationMax = 0.8f;
        float whistleFreqMin = 2000.0f; // Гц, нижняя граница ЧМ
        float whistleFreqMax = 9000.0f; // Гц, верхняя граница ЧМ
        float whistleSweepDepth = 0.6f; // 0..1, глубина ЧМ
        int numHarmonics = 2; // гармоники свиста (1 или 2)

        // --- 2. Щелчки ---
        float clickRate = 1.0f; // серий в секунду
        float clickDurationUs = 100.0f; // длительность щелчка, мкс
        float clickFreqMin = 1000.0f; // Гц
        float clickFreqMax = 8000.0f;
        int clicksPerTrain = 3; // щелчков в серии

        // --- 3. Burst pulses (жужжание) ---
        float burstRate = 0.3f; // вспышек в секунду
        float burstDurationMin = 0.3f; // сек
        float burstDurationMax = 1.0f;
        float burstClickIntervalUs = 500.0f; // интервал между щелчками, мкс
        float burstFreqMin = 2000.0f; // Гц
        float burstFreqMax = 8000.0f;

        // --- Общие ---
        float snrDb = 30.0f;
        uint32_t seed = 77777;
    };

    class Dolphin : public SourceModel
    {
    public:
        explicit Dolphin(const DolphinParams& p = {});

        void generate(float* out, int numSamples, float fs, int64_t sampleOffset) override;

        std::string name() const override { return "Dolphin"; }
        void reset() override;

        const DolphinParams& params() const { return p_; }
        void setParams(const DolphinParams& p);

    private:
        DolphinParams p_;
        std::mt19937 rng_;
        std::normal_distribution<float> gauss_{0.0f, 1.0f};

        // --- Внутренние структуры для событий ---
        struct Whistle
        {
            int sampleIndex = 0; // начало
            int lengthSamples = 0; // длина
            float f0 = 3000.0f; // начальная частота, Гц
            float f1 = 6000.0f; // конечная частота, Гц
            float phase = 0.0f; // начальная фаза
        };

        struct Click
        {
            int sampleIndex = 0; // позиция
            float amplitude = 1.0f; // амплитуда
            float phase = 0.0f; // начальная фаза
        };

        struct Burst
        {
            int sampleIndex = 0; // начало
            int lengthSamples = 0; // длина
            float clickIntervalSamples = 0.0f; // интервал между щелчками, в отсчётах
            float amplitude = 1.0f; // амплитуда
            float phase = 0.0f; // начальная фаза
        };

        // --- Генерация событий ---
        std::vector<Whistle> generateWhistles(int numSamples, float fs, uint32_t seed);
        std::vector<Click> generateClicks(int numSamples, float fs, uint32_t seed);
        std::vector<Burst> generateBursts(int numSamples, float fs, uint32_t seed);

        // --- Рендер событий в буфер ---
        void renderWhistle(std::vector<float>& buffer, int numSamples, float fs,
                           const Whistle& w);
        void renderClick(std::vector<float>& buffer, int numSamples, float fs,
                         const Click& c);
        void renderBurst(std::vector<float>& buffer, int numSamples, float fs,
                         const Burst& b);
    };


    // ============================================================
    //  Адаптер: превращает DolphinWhistleTest в SourceModel.
    //  Нужен, чтобы генератор датасета (Scene) мог использовать
    //  новую модель свиста, не меняя архитектуру.
    // ============================================================
    class DolphinWhistleSource : public SourceModel
    {
    public:
        explicit DolphinWhistleSource(const DolphinWhistleTestParams& p = {});

        void generate(float* out, int numSamples, float fs, int64_t sampleOffset) override;
        std::string name() const override { return "DolphinWhistleSource"; }
        void reset() override;

        const DolphinWhistleTestParams& params() const { return p_; }
        void setParams(const DolphinWhistleTestParams& p);

    private:
        DolphinWhistleTestParams p_;
        DolphinWhistleTest engine_;
    };
} // namespace hydro
