#include "scene.h"
#include <stdexcept>
#include <random>
#include <cmath>

namespace hydro
{
    void Scene::addSource(std::unique_ptr<SourceModel> src)
    {
        sources_.push_back(std::move(src));
    }

    void Scene::clearSources()
    {
        sources_.clear();
    }

    void Scene::generate(std::array<std::vector<float>, NUM_CHANNELS>& out,
                         int numSamples,
                         float fs,
                         const UcaGeometry& uca)
    {
        // Проверка: минимум одна компонента.
        if (!hasAnyComponent())
        {
            throw std::runtime_error("Scene::generate: no components enabled");
        }

        // 1. Обнулить выход.
        for (int ch = 0; ch < NUM_CHANNELS; ++ch)
            out[ch].assign(numSamples, 0.0f);

        // 2. Каждый источник — отдельно через Propagation со СВОИМ азимутом.
        //    Шум приёмника здесь ОТКЛЮЧЁН (receiverSnrDb > 100): он будет
        //    добавлен один раз на всю сцену, после сложения.
        std::vector<float> mono(numSamples, 0.0f);
        std::array<std::vector<float>, NUM_CHANNELS> tmp8;

        for (auto& src : sources_)
        {
            std::fill(mono.begin(), mono.end(), 0.0f);
            src->reset();
            src->generate(mono.data(), numSamples, fs, 0);

            Propagation::Params pp;
            pp.azimuthDeg = src->azimuthDeg(); // <--- азимут ИСТОЧНИКА
            pp.soundSpeed = params_.soundSpeed;
            pp.receiverSnrDb = 1000.0f; // шум отключён здесь
            pp.noiseSeed = params_.noiseSeed;

            Propagation prop(pp);
            prop.propagate(mono, numSamples, fs, uca, tmp8);

            for (int ch = 0; ch < NUM_CHANNELS; ++ch)
                for (int n = 0; n < numSamples; ++n)
                    out[ch][n] += tmp8[ch][n];
        }

        // 3. Шум приёмника — ОДИН раз на всю сцену.
        //    Амплитуда — от RMS текущего микса (как раньше в Propagation).
        if (params_.receiverSnrDb <= 100.0f)
        {
            float rmsSignal = 0.0f;
            for (int ch = 0; ch < NUM_CHANNELS; ++ch)
                for (int n = 0; n < numSamples; ++n)
                    rmsSignal += out[ch][n] * out[ch][n];
            rmsSignal = std::sqrt(rmsSignal /
                static_cast<float>(NUM_CHANNELS * numSamples));

            const float noiseRms = rmsSignal *
                std::pow(10.0f, -params_.receiverSnrDb / 20.0f);

            std::mt19937 rngNoise(params_.noiseSeed);
            std::normal_distribution<float> gauss(0.0f, 1.0f);

            for (int ch = 0; ch < NUM_CHANNELS; ++ch)
                for (int n = 0; n < numSamples; ++n)
                    out[ch][n] += gauss(rngNoise) * noiseRms;
        }

        // 4. Шум океана.
        if (params_.oceanEnabled && params_.oceanParams.level > 0.0f)
        {
            OceanNoise ocean(params_.oceanParams);
            ocean.addTo(out, numSamples, fs, uca, params_.soundSpeed);
        }

        // 5. Нормализация по пику: гарантируем, что пик не превышает 0.95.
        {
            float maxAbs = 0.0f;
            for (int ch = 0; ch < NUM_CHANNELS; ++ch)
                for (int n = 0; n < numSamples; ++n)
                {
                    const float a = std::fabs(out[ch][n]);
                    if (a > maxAbs) maxAbs = a;
                }

            if (maxAbs > 0.95f)
            {
                const float gain = 0.95f / maxAbs;
                for (int ch = 0; ch < NUM_CHANNELS; ++ch)
                    for (int n = 0; n < numSamples; ++n)
                        out[ch][n] *= gain;
            }
        }
    }
} // namespace hydro
