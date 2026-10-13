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

        // 1. Суммировать моно-сигналы всех пространственных источников.
        const bool hasSpatial = !sources_.empty();
        std::vector<float> mono(numSamples, 0.0f);

        if (hasSpatial)
        {
            std::vector<float> tmp(numSamples, 0.0f);
            for (auto& src : sources_)
            {
                // Обнуляем tmp ПЕРЕД генерацией каждого источника.
                // SourceModel::generate() ЗАПИСЫВАЕТ значения,
                // а не добавляет их, поэтому старые данные нужно стереть.
                std::fill(tmp.begin(), tmp.end(), 0.0f);

                src->reset();
                src->generate(tmp.data(), numSamples, fs, 0);
                for (int n = 0; n < numSamples; ++n)
                {
                    mono[n] += tmp[n];
                }
            }
        }

        // 2. Propagation → 8 каналов (или тишина).
        if (hasSpatial)
        {
            Propagation::Params pp;
            pp.azimuthDeg = params_.azimuthDeg;
            pp.soundSpeed = params_.soundSpeed;
            pp.receiverSnrDb = params_.receiverSnrDb;
            pp.noiseSeed = params_.noiseSeed;

            Propagation prop(pp);
            prop.propagate(mono, numSamples, fs, uca, out);
        }
        else
        {
            for (int ch = 0; ch < NUM_CHANNELS; ++ch)
            {
                out[ch].assign(numSamples, 0.0f);
            }
        }

        // 3. Шум океана.
        if (params_.oceanEnabled && params_.oceanParams.level > 0.0f)
        {
            OceanNoise ocean(params_.oceanParams);
            ocean.addTo(out, numSamples, fs, uca, params_.soundSpeed);
        }

        // ---- 4. Нормализация: гарантируем, что пик не превышает 0.95 ----
        // Это защищает от клиппинга при записи в WAV. Не меняет форму
        // сигнала — только уровень. Для каждого файла независимо.
        {
            float maxAbs = 0.0f;
            for (int ch = 0; ch < NUM_CHANNELS; ++ch)
            {
                for (int n = 0; n < numSamples; ++n)
                {
                    const float a = std::fabs(out[ch][n]);
                    if (a > maxAbs) maxAbs = a;
                }
            }

            if (maxAbs > 0.95f)
            {
                const float gain = 0.95f / maxAbs;
                for (int ch = 0; ch < NUM_CHANNELS; ++ch)
                {
                    for (int n = 0; n < numSamples; ++n)
                    {
                        out[ch][n] *= gain;
                    }
                }
            }
        }
    }
} // namespace hydro
