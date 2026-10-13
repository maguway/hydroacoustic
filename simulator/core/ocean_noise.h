#pragma once
#include "types.h"
#include "uca_geometry.h"
#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace hydro
{
    // ============================================================
    //  OceanNoiseParams — параметры шума океана.
    //
    //  ЭТАП 1: всплески + эхо + волны. Фон (rumble/mid/hiss) выключен.
    //  Модуляция выключена.
    //
    //  Три слоя:
    //    1. Всплески (spikes) — капли, пузырьки, щелчки. Короткие.
    //       Идят через propagation, с эхом.
    //    2. Волны (waves) — протяжные низкочастотные накаты, 1–3 сек.
    //       Каждая — со своим азимутом.
    //    3. Фон — пока выключен.
    // ============================================================

    struct OceanNoiseParams
    {
        // --- Общий уровень ---
        float level = 0.5f;

        // Насколько фон модулируется волнами (0 = ровный, 1 = сильно дышит).
        float bgModulation = 0.7f;

        // --- Фон (выключен) ---
        float rumbleWeight = 0.0f;
        float midWeight = 0.0f;
        float hissWeight = 0.0f;
        float rumbleSlope = 0.97f;
        float midSlope = 0.65f;
        float hissSlope = 0.25f;

        // ============================================================
        //  СПАЙКИ (короткие всплески)
        // ============================================================
        float spikeRate = 3.0f;
        float spikeAmp = 0.5f;
        float spikeSpatialFrac = 0.9f;

        // Тип 0 — «капля».
        float dropTauMinMs = 8.0f;
        float dropTauMaxMs = 20.0f;

        // Тип 1 — «пузырёк».
        float bubbleTauMinMs = 40.0f;
        float bubbleTauMaxMs = 90.0f;
        float bubbleFreqStartMin = 500.0f;
        float bubbleFreqStartMax = 1200.0f;
        float bubbleFreqEndMin = 150.0f;
        float bubbleFreqEndMax = 350.0f;

        // Тип 2 — «щелчок».
        float clickTauMinMs = 4.0f;
        float clickTauMaxMs = 10.0f;

        // Эхо.
        int echoCount = 2;
        float echoDelayMinMs = 80.0f;
        float echoDelayMaxMs = 350.0f;
        float echoGainMin = 0.40f;
        float echoGainMax = 0.75f;
        float echoAzSpreadDeg = 55.0f;

        // ============================================================
        //  ВОЛНЫ (длительные низкочастотные накаты)
        // ============================================================
        float waveRate = 0.5f; // волн в секунду (1 волна раз в 2 сек)
        float waveAmp = 0.35f; // амплитуда
        float waveDurationMinS = 1.0f; // длительность огибающей, сек
        float waveDurationMaxS = 3.0f;
        float waveSpatialFrac = 0.9f; // доля волн с азимутом


        // ============================================================
        //  СЛОИ ВОЛНЫ (для renderWave версии 2).
        //  4 полосовых слоя шума, у каждого своя огибающая.
        // ============================================================

        // Частоты среза слоёв, Гц.
        float rumbleLow = 20.0f;
        float rumbleHigh = 200.0f;
        float bodyLow = 200.0f;
        float bodyHigh = 2000.0f;
        float foamLow = 2000.0f;
        float foamHigh = 4000.0f;
        float hissLow = 4000.0f;
        float hissHigh = 4900.0f;

        // Времена нарастания, сек.
        float rumbleAttack = 0.06f;
        float bodyAttack = 0.08f;
        float foamAttack = 0.025f;
        float hissAttack = 0.015f;

        // Времена спада (1/e), сек. — длиннее, чтобы волны перекрывались.
        float rumbleDecay = 2.5f;
        float bodyDecay = 1.8f;
        float foamDecay = 0.9f;
        float hissDecay = 0.6f;

        // Задержки старта слоёв относительно начала волны, сек.
        float foamDelay = 0.07f;
        float hissDelay = 0.12f;

        // Веса слоёв в миксе (1.0 = основной уровень).
        float rumbleMix = 0.9f;
        float bodyMix = 1.0f;
        float foamMix = 0.7f;
        float hissMix = 0.6f;


        // ============================================================
        //  МОДУЛЯЦИЯ (выключена)
        // ============================================================
        float modulationDepth = 0.0f;
        float modulationHz = 0.15f;

        // --- Seed ---
        uint32_t seed = 777;
    };

    // ============================================================

    class OceanNoise
    {
    public:
        struct Spike
        {
            int sampleIndex = 0;
            float azimuthDeg = -1.0f;
            int type = 0;
            float amplitude = 0.5f;
            float tauSec = 0.005f;
            float f0 = 800.0f;
            float f1 = 200.0f;
            float phase = 0.0f;
        };

        struct Wave
        {
            int sampleIndex = 0; // начало
            int lengthSamples = 0; // длина
            float azimuthDeg = -1.0f;
            float amplitude = 0.35f;

            // --- θ-модуляция: азимут «плывёт» внутри волны ---
            float azSweepDeg = 0.0f;   // амплитуда качания, градусы
            float azSweepHz = 0.15f;   // частота качания, Гц
            float azSweepPhase = 0.0f; // начальная фаза качания
        };

        explicit OceanNoise(const OceanNoiseParams& p = {});

        void setParams(const OceanNoiseParams& p) { p_ = p; }
        const OceanNoiseParams& params() const { return p_; }

        void addTo(std::array<std::vector<float>, NUM_CHANNELS>& out,
                   int numSamples,
                   float fs,
                   const UcaGeometry& uca,
                   float soundSpeed);

    private:
        OceanNoiseParams p_;

        void generateBackgroundChannel(std::vector<float>& outChannel,
                                       int numSamples,
                                       float fs,
                                       uint32_t channelSeed);

        std::vector<Spike> generateSpikes(int numSamples, float fs, uint32_t seed);
        std::vector<Wave> generateWaves(int numSamples, float fs, uint32_t seed);

        std::vector<float> generateModulation(int numSamples, float fs, uint32_t seed);

        void renderWave(std::array<std::vector<float>, NUM_CHANNELS>& out,
                        int numSamples,
                        float fs,
                        const OceanNoise::Wave& w,
                        const OceanNoiseParams& p,
                        const UcaGeometry& uca,
                        float soundSpeed,
                        std::mt19937& rng);

        void renderSpike(std::vector<float>& buffer,
                         int numSamples,
                         float fs,
                         int startSample,
                         const OceanNoise::Spike& s,
                         std::mt19937& rng);
    };
} // namespace hydro
