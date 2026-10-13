#pragma once
#include "types.h"
#include "uca_geometry.h"
#include "source_model.h"
#include "ocean_noise.h"
#include "propagation.h"
#include <array>
#include <memory>
#include <vector>

namespace hydro
{
    struct SceneParams
    {
        float azimuthDeg = 0.0f;
        float distanceM = 100.0f;
        float soundSpeed = SOUND_SPEED_WATER;
        float receiverSnrDb = 30.0f;
        uint32_t noiseSeed = 42;

        // --- Шум океана (отдельная компонента) ---
        bool oceanEnabled = false;
        OceanNoiseParams oceanParams;
    };

    class Scene
    {
    public:
        Scene() = default;

        // Добавить пространственный источник (винт, двигатель, ...).
        void addSource(std::unique_ptr<SourceModel> src);

        // Очистить все источники.
        void clearSources();

        // Включить/выключить шум океана.
        void setOceanEnabled(bool enabled) { params_.oceanEnabled = enabled; }
        void setOceanParams(const OceanNoiseParams& p) { params_.oceanParams = p; }

        // Общие параметры.
        void setParams(const SceneParams& p) { params_ = p; }
        const SceneParams& params() const { return params_; }

        // Есть ли хотя бы одна компонента.
        bool hasAnyComponent() const
        {
            return !sources_.empty() || params_.oceanEnabled;
        }

        void generate(std::array<std::vector<float>, NUM_CHANNELS>& out,
                      int numSamples,
                      float fs,
                      const UcaGeometry& uca);

    private:
        std::vector<std::unique_ptr<SourceModel>> sources_;
        SceneParams params_;
    };
} // namespace hydro
