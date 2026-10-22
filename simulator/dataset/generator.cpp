#include "generator.h"
#include "params.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <random>
#include <memory>

namespace fs = std::filesystem;

namespace hydro
{
    // ------------------------------------------------------------
    //  Заполнить OceanNoiseParams значениями по умолчанию из params.h.
    // ------------------------------------------------------------
    static void fillOceanDefaults(OceanNoiseParams& op)
    {
        op.level = OCEAN_LEVEL;
        op.rumbleWeight = OCEAN_RUMBLE_WEIGHT;
        op.midWeight = OCEAN_MID_WEIGHT;
        op.hissWeight = OCEAN_HISS_WEIGHT;
        op.rumbleSlope = OCEAN_RUMBLE_SLOPE;
        op.midSlope = OCEAN_MID_SLOPE;
        op.hissSlope = OCEAN_HISS_SLOPE;
        op.spikeRate = OCEAN_SPIKE_RATE;
        op.spikeAmp = OCEAN_SPIKE_AMP;
        op.spikeSpatialFrac = OCEAN_SPIKE_SPATIAL_FRAC;
        op.dropTauMinMs = OCEAN_DROP_TAU_MIN_MS;
        op.dropTauMaxMs = OCEAN_DROP_TAU_MAX_MS;
        op.bubbleTauMinMs = OCEAN_BUBBLE_TAU_MIN_MS;
        op.bubbleTauMaxMs = OCEAN_BUBBLE_TAU_MAX_MS;
        op.bubbleFreqStartMin = OCEAN_BUBBLE_F0_MIN;
        op.bubbleFreqStartMax = OCEAN_BUBBLE_F0_MAX;
        op.bubbleFreqEndMin = OCEAN_BUBBLE_F1_MIN;
        op.bubbleFreqEndMax = OCEAN_BUBBLE_F1_MAX;
        op.clickTauMinMs = OCEAN_CLICK_TAU_MIN_MS;
        op.clickTauMaxMs = OCEAN_CLICK_TAU_MAX_MS;
        op.echoCount = OCEAN_ECHO_COUNT;
        op.echoDelayMinMs = OCEAN_ECHO_DELAY_MIN_MS;
        op.echoDelayMaxMs = OCEAN_ECHO_DELAY_MAX_MS;
        op.echoGainMin = OCEAN_ECHO_GAIN_MIN;
        op.echoGainMax = OCEAN_ECHO_GAIN_MAX;
        op.echoAzSpreadDeg = OCEAN_ECHO_AZ_SPREAD_DEG;
        op.waveRate = OCEAN_WAVE_RATE;
        op.waveAmp = OCEAN_WAVE_AMP;
        op.waveDurationMinS = OCEAN_WAVE_DURATION_MIN_S;
        op.waveDurationMaxS = OCEAN_WAVE_DURATION_MAX_S;
        op.waveSpatialFrac = OCEAN_WAVE_SPATIAL_FRAC;
        op.modulationDepth = OCEAN_MODULATION_DEPTH;
        op.modulationHz = OCEAN_MODULATION_HZ;
    }


    template <typename Channels>
    static bool writeWavFloat32(const std::string& path,
                                const Channels& channels,
                                int sampleRate)
    {
        if (channels.empty()) return false;
        const uint16_t numChannels = static_cast<uint16_t>(channels.size());
        const size_t numSamples = channels[0].size();
        for (const auto& ch : channels)
        {
            if (ch.size() != numSamples) return false;
        }

        std::ofstream f(path, std::ios::binary);
        if (!f) return false;

        const uint32_t dataBytes = static_cast<uint32_t>(numSamples * numChannels * 4);
        const uint32_t fmtChunkSize = 16;
        const uint16_t audioFormat = 3;
        const uint16_t bitsPerSample = 32;
        const uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
        const uint16_t blockAlign = numChannels * (bitsPerSample / 8);
        const uint32_t riffSize = 4 + (8 + fmtChunkSize) + (8 + dataBytes);

        auto writeU32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
        auto writeU16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };

        f.write("RIFF", 4);
        writeU32(riffSize);
        f.write("WAVE", 4);
        f.write("fmt ", 4);
        writeU32(fmtChunkSize);
        writeU16(audioFormat);
        writeU16(numChannels);
        writeU32(sampleRate);
        writeU32(byteRate);
        writeU16(blockAlign);
        writeU16(bitsPerSample);
        f.write("data", 4);
        writeU32(dataBytes);

        std::vector<float> frame(numChannels);
        for (size_t s = 0; s < numSamples; ++s)
        {
            for (uint16_t c = 0; c < numChannels; ++c)
            {
                frame[c] = channels[c][s];
            }
            f.write(reinterpret_cast<const char*>(frame.data()), numChannels * 4);
        }
        return true;
    }

    // ------------------------------------------------------------

    DatasetGenerator::DatasetGenerator(const UcaGeometry& uca,
                                       const DatasetParams& params,
                                       const DatasetSelection& selection)
        : uca_(uca), p_(params), sel_(selection)
    {
    }

    std::string DatasetGenerator::makeFileName(int index, const std::string& ext)
    {
        std::ostringstream oss;
        oss << std::setw(4) << std::setfill('0') << index << ext;
        return oss.str();
    }

    int DatasetGenerator::generateClass(
        const std::string& className,
        const std::function<std::unique_ptr<SourceModel>(std::mt19937&)>& sourceFactory,
        const std::function<std::string(const SourceModel*)>& jsonExtraFields,
        std::mt19937& rng,
        bool useOceanNoise) const
    {
        const fs::path classDir = fs::path(p_.outputDir) / className;
        fs::create_directories(classDir);

        const int totalSamples = static_cast<int>(p_.sampleRate) * p_.durationSec;
        int created = 0;

        // Общие диапазоны сцены.

        std::uniform_real_distribution<float> oceanLevelDist(ds::OCEAN_LEVEL_MIN, ds::OCEAN_LEVEL_MAX);

        for (int i = 1; i <= p_.filesPerClass; ++i)
        {
            // Создаём источник (для OceanNoise — nullptr).
            std::unique_ptr<SourceModel> source;
            const SourceModel* srcPtr = nullptr;
            if (!useOceanNoise)
            {
                source = sourceFactory(rng);
                if (!source) continue;
                srcPtr = source.get(); // сохраняем ДО передачи в Scene
            }

            SceneParams sp;
            if (useOceanNoise)
            {
                sp.azimuthDeg = -1.0f;
            }
            else if (p_.uniformAzimuth)
            {
                sp.azimuthDeg = (static_cast<float>(i - 1) / p_.filesPerClass) * 360.0f;
            }
            else
            {
                sp.azimuthDeg = p_.azimuthDeg;
            }
            sp.soundSpeed = SOUND_SPEED_WATER;
            sp.receiverSnrDb = p_.receiverSnrDb;
            sp.noiseSeed = static_cast<uint32_t>(rng());

            // Шум океана — включён для класса OceanNoise, и для чистых
            // классов, только если пользователь выбрал mixWithOcean.
            fillOceanDefaults(sp.oceanParams);
            if (useOceanNoise)
            {
                sp.oceanEnabled = true;
                sp.oceanParams.level = 0.5f + oceanLevelDist(rng) * 0.5f; // 0.5–0.8
            }
            else
            {
                sp.oceanEnabled = p_.mixWithOcean; // <--- УПРАВЛЯЕТСЯ НАСТРОЙКОЙ
                sp.oceanParams.level = oceanLevelDist(rng);
            }
            sp.oceanParams.seed = static_cast<uint32_t>(rng());

            Scene scene;
            scene.setParams(sp);
            if (source) scene.addSource(std::move(source));

            std::array<std::vector<float>, NUM_CHANNELS> channels8;
            scene.generate(channels8, totalSamples, p_.sampleRate, uca_);

            const std::string wavName = makeFileName(i, ".wav");
            const fs::path wavPath = classDir / wavName;
            if (!writeWavFloat32(wavPath.string(), channels8,
                                 static_cast<int>(p_.sampleRate)))
            {
                std::cerr << "  [FAIL] " << wavPath << "\n";
                continue;
            }

            std::cout << "  [" << i << "/" << p_.filesPerClass << "] "
                << className << " #" << std::setw(4) << std::setfill('0') << i
                << "  az=" << std::setw(6) << std::setprecision(1)
                << std::fixed << sp.azimuthDeg
                << "°  recvSNR=" << std::setprecision(1)
                << sp.receiverSnrDb << " dB\n";

            ++created;
        }

        return created;
    }

    int DatasetGenerator::generate()
    {
        std::cout << "\n=== Dataset generation ===\n";
        std::cout << "  files per class: " << p_.filesPerClass << "\n";
        std::cout << "  duration:        " << p_.durationSec << " s\n";
        std::cout << "  sample rate:     " << p_.sampleRate << " Hz\n";
        std::cout << "  output dir:      " << p_.outputDir << "\n\n";

        fs::create_directories(p_.outputDir);

        std::mt19937 rng(p_.masterSeed);

        // --- Диапазоны рандомизации ---
        std::uniform_int_distribution<int> bladesDist(ds::PROP_NUM_BLADES_MIN,
                                                      ds::PROP_NUM_BLADES_MAX);
        std::uniform_real_distribution<float> propRpmDist(ds::PROP_RPM_MIN,
                                                          ds::PROP_RPM_MAX);
        std::uniform_int_distribution<int> harmDist(ds::PROP_NUM_HARMONICS_MIN,
                                                    ds::PROP_NUM_HARMONICS_MAX);
        std::uniform_real_distribution<float> modDepthDist(ds::PROP_MOD_DEPTH_MIN,
                                                           ds::PROP_MOD_DEPTH_MAX);
        std::uniform_real_distribution<float> modHzDist(ds::PROP_MOD_HZ_MIN,
                                                        ds::PROP_MOD_HZ_MAX);
        std::uniform_real_distribution<float> cavDist(ds::PROP_CAVITATION_MIN,
                                                      ds::PROP_CAVITATION_MAX);
        std::uniform_real_distribution<float> propSnrDist(ds::PROP_SNR_DB_MIN,
                                                          ds::PROP_SNR_DB_MAX);

        std::uniform_real_distribution<float> engRpmDist(ds::ENG_RPM_MIN,
                                                         ds::ENG_RPM_MAX);
        std::uniform_int_distribution<int> engHarmDist(ds::ENG_NUM_HARMONICS_MIN,
                                                       ds::ENG_NUM_HARMONICS_MAX);
        std::uniform_int_distribution<int> engResDist(ds::ENG_NUM_RESONANCES_MIN,
                                                      ds::ENG_NUM_RESONANCES_MAX);
        std::uniform_real_distribution<float> engMechDist(ds::ENG_MECH_NOISE_MIN,
                                                          ds::ENG_MECH_NOISE_MAX);
        std::uniform_real_distribution<float> engSnrDist(ds::ENG_SNR_DB_MIN,
                                                         ds::ENG_SNR_DB_MAX);

        // --- Дельфин: диапазоны рандомизации ---
        std::uniform_int_distribution<int> dolphWhistlesDist(ds::DOLPHIN_WHISTLES_PER_SEC_MIN,
                                                             ds::DOLPHIN_WHISTLES_PER_SEC_MAX);
        std::uniform_real_distribution<float> dolphFreqMinDist(ds::DOLPHIN_WHISTLE_FREQ_MIN_MIN,
                                                               ds::DOLPHIN_WHISTLE_FREQ_MIN_MAX);
        std::uniform_real_distribution<float> dolphFreqMaxDist(ds::DOLPHIN_WHISTLE_FREQ_MAX_MIN,
                                                               ds::DOLPHIN_WHISTLE_FREQ_MAX_MAX);
        std::uniform_real_distribution<float> dolphClickRateDist(ds::DOLPHIN_CLICK_RATE_MIN,
                                                                 ds::DOLPHIN_CLICK_RATE_MAX);
        std::uniform_real_distribution<float> dolphBurstRateDist(ds::DOLPHIN_BURST_RATE_MIN,
                                                                 ds::DOLPHIN_BURST_RATE_MAX);
        std::uniform_real_distribution<float> dolphSnrDist(ds::DOLPHIN_SNR_DB_MIN,
                                                           ds::DOLPHIN_SNR_DB_MAX);

        int totalCreated = 0;

        // ---- PropellerSub ----
        if (sel_.propeller)
        {
            std::cout << "--- PropellerSub ---\n";
            totalCreated += generateClass(
                "PropellerSub",
                [&](std::mt19937& r) -> std::unique_ptr<SourceModel>
                {
                    PropellerParams pp;
                    pp.numBlades = bladesDist(r);
                    pp.rpm = propRpmDist(r);
                    pp.depthM = PROP_DEPTH_M;
                    pp.numHarmonics = harmDist(r);
                    pp.modulationDepth = modDepthDist(r);
                    pp.modulationHz = modHzDist(r);
                    pp.cavitation = cavDist(r);
                    pp.snrDb = propSnrDist(r);
                    pp.seed = static_cast<uint32_t>(r());
                    return std::make_unique<PropellerSubmarine>(pp);
                },
                [](const SourceModel* src) -> std::string
                {
                    const auto* p = dynamic_cast<const PropellerSubmarine*>(src);
                    if (!p) return "{}";
                    const auto& pp = p->params();
                    std::ostringstream os;
                    os << "{\n";
                    os << "    \"num_blades\": " << pp.numBlades << ",\n";
                    os << "    \"rpm\": " << std::fixed << std::setprecision(2) << pp.rpm << ",\n";
                    os << "    \"depth_m\": " << std::setprecision(1) << pp.depthM << ",\n";
                    os << "    \"cavitation\": " << std::setprecision(3) << pp.cavitation << ",\n";
                    os << "    \"snr_db\": " << std::setprecision(2) << pp.snrDb << ",\n";
                    os << "    \"seed\": " << pp.seed << "\n";
                    os << "  }";
                    return os.str();
                },
                rng, false);
        }

        // ---- EngineSub ----
        if (sel_.engine)
        {
            std::cout << "\n--- EngineSub ---\n";
            totalCreated += generateClass(
                "EngineSub",
                [&](std::mt19937& r) -> std::unique_ptr<SourceModel>
                {
                    EngineParams ep;
                    ep.rpm = engRpmDist(r);
                    ep.numHarmonics = engHarmDist(r);
                    ep.numResonances = engResDist(r);
                    ep.mechNoiseLevel = engMechDist(r);
                    ep.snrDb = engSnrDist(r);
                    ep.seed = static_cast<uint32_t>(r());
                    return std::make_unique<EngineSubmarine>(ep);
                },
                [](const SourceModel* src) -> std::string
                {
                    const auto* p = dynamic_cast<const EngineSubmarine*>(src);
                    if (!p) return "{}";
                    const auto& ep = p->params();
                    std::ostringstream os;
                    os << "{\n";
                    os << "    \"rpm\": " << std::fixed << std::setprecision(2) << ep.rpm << ",\n";
                    os << "    \"num_harmonics\": " << ep.numHarmonics << ",\n";
                    os << "    \"num_resonances\": " << ep.numResonances << ",\n";
                    os << "    \"mech_noise\": " << std::setprecision(3) << ep.mechNoiseLevel << ",\n";
                    os << "    \"snr_db\": " << std::setprecision(2) << ep.snrDb << ",\n";
                    os << "    \"seed\": " << ep.seed << "\n";
                    os << "  }";
                    return os.str();
                },
                rng, false);
        }

        // ---- Dolphin ----
        if (sel_.dolphin)
        {
            std::cout << "\n--- Dolphin ---\n";
            totalCreated += generateClass(
                "Dolphin",
                [&](std::mt19937& r) -> std::unique_ptr<SourceModel>
                {
                    DolphinWhistleTestParams dp;

                    // Рандомизация в разумных пределах (как вы просили)
                    dp.fStartMin = dolphFreqMinDist(r); // нижняя частота
                    dp.fStartMax = dp.fStartMin + 1000.0f;
                    dp.fEndMin = dolphFreqMaxDist(r) * 0.4f; // куда свистит
                    dp.fEndMax = dp.fEndMin + 500.0f;

                    // Случайное число фраз (3-5 штук на 5 секунд)
                    dp.phraseCount = 3 + (r() % 3);

                    // Ограничитель сверху (из наших вчерашних правок)
                    dp.fMaxLimit = 6000.0f;

                    // Приглушенные гармоники (из наших вчерашних правок)
                    dp.harm1Db = -20.0f;
                    dp.harm2Db = -30.0f;
                    dp.harm3Db = -40.0f;

                    dp.seed = static_cast<uint32_t>(r());

                    return std::make_unique<DolphinWhistleSource>(dp);
                },
                [](const SourceModel* src) -> std::string
                {
                    const auto* p = dynamic_cast<const Dolphin*>(src);
                    if (!p) return "{}";
                    const auto& dp = p->params();
                    std::ostringstream os;
                    os << "{\n";
                    os << "    \"num_whistles_per_sec\": " << dp.numWhistlesPerSec << ",\n";
                    os << "    \"whistle_freq_min\": " << std::fixed << std::setprecision(1) << dp.whistleFreqMin <<
                        ",\n";
                    os << "    \"whistle_freq_max\": " << std::setprecision(1) << dp.whistleFreqMax << ",\n";
                    os << "    \"click_rate\": " << std::setprecision(3) << dp.clickRate << ",\n";
                    os << "    \"burst_rate\": " << std::setprecision(3) << dp.burstRate << ",\n";
                    os << "    \"snr_db\": " << std::setprecision(2) << dp.snrDb << ",\n";
                    os << "    \"seed\": " << dp.seed << "\n";
                    os << "  }";
                    return os.str();
                },
                rng, false);
        }

        // ---- OceanNoise ----
        if (sel_.ocean)
        {
            std::cout << "\n--- OceanNoise ---\n";
            totalCreated += generateClass(
                "OceanNoise",
                [](std::mt19937&) -> std::unique_ptr<SourceModel> { return nullptr; },
                [](const SourceModel*) -> std::string
                {
                    return "{\n    \"_note\": \"ocean noise only, no active source\"\n  }";
                },
                rng, true);
        }

        // ---- Combined — сумма всех выбранных компонент ----
        // Создаётся, только если выбрано 2+ компоненты.
        int selectedCount = (sel_.propeller ? 1 : 0)
            + (sel_.engine ? 1 : 0)
            + (sel_.dolphin ? 1 : 0)
            + (sel_.ocean ? 1 : 0);
        if (selectedCount >= 2)
        {
            std::cout << "\n--- Combined ---\n";
            totalCreated += generateCombinedClass(rng);
        }

        // ---- Сводный labels.json ----
        writeLabelsFile();

        // ---- _schema.json ----
        writeSchemaFile();

        std::cout << "\nTotal created: " << totalCreated << " WAV files.\n";
        std::cout << "Output: " << p_.outputDir << "\n";
        return totalCreated;
    }

    void DatasetGenerator::writeSchemaFile() const
    {
        const fs::path path = fs::path(p_.outputDir) / "_schema.json";
        std::ofstream f(path);
        if (!f) return;

        f << "{\n";
        f << "  \"_description\": \"hydroacoustic dataset. Each sample = 8-ch float32 WAV + JSON metadata.\",\n";
        f << "  \"_format_version\": \"1.0\",\n\n";
        f << "  \"class\": \"ML class name (PropellerSub | EngineSub | Dolphin | OceanNoise | ...)\",\n";
        f << "  \"azimuth_deg\": \"source azimuth, degrees 0..359.9; -1 means no direction (OceanNoise)\",\n";
        f << "  \"sample_rate\": \"WAV sample rate, Hz\",\n";
        f << "  \"duration_sec\": \"sample duration, seconds\",\n";
        f << "  \"num_channels\": \"WAV channel count (= 8)\",\n";
        f << "  \"receiver_snr_db\": \"receiver noise SNR, dB\",\n";
        f << "  \"ocean_level\": \"ocean noise level, 0..1\",\n";
        f << "  \"params\": \"source-specific parameters, object\",\n\n";
        f << "  \"params_PropellerSub\": {\n";
        f << "    \"num_blades\": \"blade count\",\n";
        f << "    \"rpm\": \"shaft rpm\",\n";
        f << "    \"depth_m\": \"depth, meters\",\n";
        f << "    \"cavitation\": \"cavitation level, 0..1\",\n";
        f << "    \"snr_db\": \"internal SNR, dB\",\n";
        f << "    \"seed\": \"rng seed\"\n";
        f << "  },\n\n";
        f << "  \"params_EngineSub\": {\n";
        f << "    \"rpm\": \"engine rpm\",\n";
        f << "    \"num_harmonics\": \"harmonic count\",\n";
        f << "    \"num_resonances\": \"hull resonance count\",\n";
        f << "    \"mech_noise\": \"mechanical noise level, 0..1\",\n";
        f << "    \"snr_db\": \"internal SNR, dB\",\n";
        f << "    \"seed\": \"rng seed\"\n";
        f << "  },\n\n";
        f << "  \"params_Dolphin\": {\n";
        f << "    \"num_whistles_per_sec\": \"whistles per second\",\n";
        f << "    \"whistle_freq_min\": \"whistle lower frequency bound, Hz\",\n";
        f << "    \"whistle_freq_max\": \"whistle upper frequency bound, Hz\",\n";
        f << "    \"click_rate\": \"click trains per second\",\n";
        f << "    \"burst_rate\": \"burst pulses per second\",\n";
        f << "    \"snr_db\": \"internal SNR, dB\",\n";
        f << "    \"seed\": \"rng seed\"\n";
        f << "  }\n";
        f << "}\n";
    }

    // ------------------------------------------------------------
    //  Сводный файл меток labels.json.
    // ------------------------------------------------------------
    void DatasetGenerator::writeLabelsFile() const
    {
        const fs::path path = fs::path(p_.outputDir) / "labels.json";
        std::ofstream f(path);
        if (!f) return;

        f << "{\n";
        f << "  \"version\": \"1.0\",\n";
        f << "  \"dataset_dir\": \"" << p_.outputDir << "\",\n";
        f << "  \"files_per_class\": " << p_.filesPerClass << ",\n";
        f << "  \"sample_rate\": " << static_cast<int>(p_.sampleRate) << ",\n";
        f << "  \"duration_sec\": " << p_.durationSec << ",\n";
        f << "  \"receiver_snr_db\": " << std::fixed << std::setprecision(2)
            << p_.receiverSnrDb << ",\n";
        f << "  \"files\": [\n";

        bool first = true;
        auto scanClass = [&](const std::string& className, bool isOmni)
        {
            const fs::path classDir = fs::path(p_.outputDir) / className;
            if (!fs::exists(classDir)) return;

            for (int i = 1; i <= p_.filesPerClass; ++i)
            {
                const fs::path wavPath = classDir / makeFileName(i, ".wav");
                if (!fs::exists(wavPath)) continue;

                float az;
                if (isOmni)
                {
                    az = -1.0f;
                }
                else if (p_.uniformAzimuth)
                {
                    az = (static_cast<float>(i - 1) / p_.filesPerClass) * 360.0f;
                }
                else
                {
                    az = p_.azimuthDeg;
                }

                if (!first) f << ",\n";
                first = false;

                f << "    { \"file\": \"" << className << "/" << makeFileName(i, ".wav")
                    << "\", \"class\": \"" << className
                    << "\", \"azimuth_deg\": " << std::setprecision(2) << az
                    << ", \"receiver_snr_db\": " << std::setprecision(2) << p_.receiverSnrDb
                    << " }";
            }
        };

        if (sel_.propeller) scanClass("PropellerSub", false);
        if (sel_.engine) scanClass("EngineSub", false);
        if (sel_.dolphin) scanClass("Dolphin", false);
        if (sel_.ocean) scanClass("OceanNoise", true);

        // Combined — только если выбрано 2+ компоненты.
        int selectedCount = (sel_.propeller ? 1 : 0)
            + (sel_.engine ? 1 : 0)
            + (sel_.dolphin ? 1 : 0)
            + (sel_.ocean ? 1 : 0);
        if (selectedCount >= 2) scanClass("Combined", false);

        f << "\n  ]\n";
        f << "}\n";
    }

    // ------------------------------------------------------------
    //  Combined — сумма всех ВЫБРАННЫХ компонент в одном файле.
    // ------------------------------------------------------------
    int DatasetGenerator::generateCombinedClass(std::mt19937& rng) const
    {
        const fs::path classDir = fs::path(p_.outputDir) / "Combined";
        fs::create_directories(classDir);

        const int totalSamples = static_cast<int>(p_.sampleRate) * p_.durationSec;
        int created = 0;

        // Диапазоны рандомизации (те же, что и в generate()).
        std::uniform_int_distribution<int> bladesDist(ds::PROP_NUM_BLADES_MIN,
                                                      ds::PROP_NUM_BLADES_MAX);
        std::uniform_real_distribution<float> propRpmDist(ds::PROP_RPM_MIN,
                                                          ds::PROP_RPM_MAX);
        std::uniform_int_distribution<int> harmDist(ds::PROP_NUM_HARMONICS_MIN,
                                                    ds::PROP_NUM_HARMONICS_MAX);
        std::uniform_real_distribution<float> modDepthDist(ds::PROP_MOD_DEPTH_MIN,
                                                           ds::PROP_MOD_DEPTH_MAX);
        std::uniform_real_distribution<float> modHzDist(ds::PROP_MOD_HZ_MIN,
                                                        ds::PROP_MOD_HZ_MAX);
        std::uniform_real_distribution<float> cavDist(ds::PROP_CAVITATION_MIN,
                                                      ds::PROP_CAVITATION_MAX);
        std::uniform_real_distribution<float> propSnrDist(ds::PROP_SNR_DB_MIN,
                                                          ds::PROP_SNR_DB_MAX);

        std::uniform_real_distribution<float> engRpmDist(ds::ENG_RPM_MIN,
                                                         ds::ENG_RPM_MAX);
        std::uniform_int_distribution<int> engHarmDist(ds::ENG_NUM_HARMONICS_MIN,
                                                       ds::ENG_NUM_HARMONICS_MAX);
        std::uniform_int_distribution<int> engResDist(ds::ENG_NUM_RESONANCES_MIN,
                                                      ds::ENG_NUM_RESONANCES_MAX);
        std::uniform_real_distribution<float> engMechDist(ds::ENG_MECH_NOISE_MIN,
                                                          ds::ENG_MECH_NOISE_MAX);
        std::uniform_real_distribution<float> engSnrDist(ds::ENG_SNR_DB_MIN,
                                                         ds::ENG_SNR_DB_MAX);

        std::uniform_int_distribution<int> dolphWhistlesDist(ds::DOLPHIN_WHISTLES_PER_SEC_MIN,
                                                             ds::DOLPHIN_WHISTLES_PER_SEC_MAX);
        std::uniform_real_distribution<float> dolphFreqMinDist(ds::DOLPHIN_WHISTLE_FREQ_MIN_MIN,
                                                               ds::DOLPHIN_WHISTLE_FREQ_MIN_MAX);
        std::uniform_real_distribution<float> dolphFreqMaxDist(ds::DOLPHIN_WHISTLE_FREQ_MAX_MIN,
                                                               ds::DOLPHIN_WHISTLE_FREQ_MAX_MAX);
        std::uniform_real_distribution<float> dolphClickRateDist(ds::DOLPHIN_CLICK_RATE_MIN,
                                                                 ds::DOLPHIN_CLICK_RATE_MAX);
        std::uniform_real_distribution<float> dolphBurstRateDist(ds::DOLPHIN_BURST_RATE_MIN,
                                                                 ds::DOLPHIN_BURST_RATE_MAX);
        std::uniform_real_distribution<float> dolphSnrDist(ds::DOLPHIN_SNR_DB_MIN,
                                                           ds::DOLPHIN_SNR_DB_MAX);

        std::uniform_real_distribution<float> oceanLevelDist(ds::OCEAN_LEVEL_MIN,
                                                             ds::OCEAN_LEVEL_MAX);

        for (int i = 1; i <= p_.filesPerClass; ++i)
        {
            SceneParams sp;
            if (p_.uniformAzimuth)
                sp.azimuthDeg = (static_cast<float>(i - 1) / p_.filesPerClass) * 360.0f;
            else
                sp.azimuthDeg = p_.azimuthDeg;

            sp.soundSpeed = SOUND_SPEED_WATER;
            sp.receiverSnrDb = p_.receiverSnrDb;
            sp.noiseSeed = rng();

            // Океан — включаем, если выбран.
            fillOceanDefaults(sp.oceanParams);
            sp.oceanEnabled = sel_.ocean;
            if (sel_.ocean)
            {
                sp.oceanParams.level = oceanLevelDist(rng);
            }
            sp.oceanParams.seed = rng();

            Scene scene;
            scene.setParams(sp);

            // Добавляем выбранные пространственные источники.
            if (sel_.propeller)
            {
                PropellerParams pp;
                pp.numBlades = bladesDist(rng);
                pp.rpm = propRpmDist(rng);
                pp.depthM = PROP_DEPTH_M;
                pp.numHarmonics = harmDist(rng);
                pp.modulationDepth = modDepthDist(rng);
                pp.modulationHz = modHzDist(rng);
                pp.cavitation = cavDist(rng);
                pp.snrDb = propSnrDist(rng);
                pp.seed = static_cast<uint32_t>(rng());
                scene.addSource(std::make_unique<PropellerSubmarine>(pp));
            }
            if (sel_.engine)
            {
                EngineParams ep;
                ep.rpm = engRpmDist(rng);
                ep.numHarmonics = engHarmDist(rng);
                ep.numResonances = engResDist(rng);
                ep.mechNoiseLevel = engMechDist(rng);
                ep.snrDb = engSnrDist(rng);
                ep.seed = static_cast<uint32_t>(rng());
                scene.addSource(std::make_unique<EngineSubmarine>(ep));
            }
            if (sel_.dolphin)
            {
                DolphinParams dp;
                dp.numWhistlesPerSec = dolphWhistlesDist(rng);
                dp.whistleDurationMin = DOLPHIN_WHISTLE_DUR_MIN;
                dp.whistleDurationMax = DOLPHIN_WHISTLE_DUR_MAX;
                dp.whistleFreqMin = dolphFreqMinDist(rng);
                dp.whistleFreqMax = dolphFreqMaxDist(rng);
                dp.whistleSweepDepth = DOLPHIN_WHISTLE_SWEEP;
                dp.numHarmonics = DOLPHIN_NUM_HARMONICS;
                dp.clickRate = dolphClickRateDist(rng);
                dp.clickDurationUs = DOLPHIN_CLICK_DUR_US;
                dp.clickFreqMin = DOLPHIN_CLICK_FREQ_MIN;
                dp.clickFreqMax = DOLPHIN_CLICK_FREQ_MAX;
                dp.clicksPerTrain = DOLPHIN_CLICKS_PER_TRAIN;
                dp.burstRate = dolphBurstRateDist(rng);
                dp.burstDurationMin = DOLPHIN_BURST_DUR_MIN;
                dp.burstDurationMax = DOLPHIN_BURST_DUR_MAX;
                dp.burstClickIntervalUs = DOLPHIN_BURST_CLICK_US;
                dp.burstFreqMin = DOLPHIN_BURST_FREQ_MIN;
                dp.burstFreqMax = DOLPHIN_BURST_FREQ_MAX;
                dp.snrDb = dolphSnrDist(rng);
                dp.seed = static_cast<uint32_t>(rng());
                scene.addSource(std::make_unique<Dolphin>(dp));
            }

            std::array<std::vector<float>, NUM_CHANNELS> ch8;
            scene.generate(ch8, totalSamples, p_.sampleRate, uca_);

            const std::string wavName = makeFileName(i, ".wav");
            const fs::path wavPath = classDir / wavName;
            if (!writeWavFloat32(wavPath.string(), ch8,
                                 static_cast<int>(p_.sampleRate)))
            {
                std::cerr << "  [FAIL] " << wavPath << "\n";
                continue;
            }

            std::cout << "  [" << i << "/" << p_.filesPerClass << "] "
                << "Combined #" << std::setw(4) << std::setfill('0') << i
                << "  az=" << std::setw(6) << std::setprecision(1)
                << std::fixed << sp.azimuthDeg
                << "°\n";

            ++created;
        }

        return created;
    }
} // namespace hydro
