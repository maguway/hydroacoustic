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

    // ============================================================
    //  Конкретные акустические сигнатуры (5 аппаратов).
    // ============================================================
    static const std::vector<VesselProfile> g_vesselProfiles = {
        // 1. Тяжёлая ПЛ: большой винт, низкие обороты, сильная кавитация.
        {"Sub_Heavy", 7, 150.0f, 0.45f, 300.0f, 3, 0.25f, 25.0f},

        // 2. Быстрая ПЛ: малый винт, высокие обороты, слабая кавитация.
        {"Sub_Fast", 5, 500.0f, 0.15f, 600.0f, 2, 0.15f, 35.0f},

        // 3. Тихая ПЛ: баланс, минимальный шум.
        {"Sub_Stealth", 9, 250.0f, 0.10f, 350.0f, 1, 0.05f, 45.0f},

        // 4. Старая (изношенная) ПЛ: много резонансов, дребезжание.
        {"Sub_Rusty", 6, 200.0f, 0.35f, 250.0f, 3, 0.40f, 20.0f},

        // 5. Средняя ПЛ: золотая середина.
        {"Sub_Medium", 7, 350.0f, 0.25f, 450.0f, 2, 0.20f, 30.0f}
    };

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
        bool useOceanNoise,
        bool includeDolphin,
        float dolphinAz) const
    {
        const fs::path classDir = fs::path(p_.outputDir) / className;
        fs::create_directories(classDir);

        const int totalSamples = static_cast<int>(p_.sampleRate) * p_.durationSec;
        int created = 0;

        std::uniform_real_distribution<float> oceanLevelDist(
            ds::OCEAN_LEVEL_MIN, ds::OCEAN_LEVEL_MAX);

        for (int i = 1; i <= p_.filesPerClass; ++i)
        {
            // Создаём источник (для OceanNoise — nullptr).
            std::unique_ptr<SourceModel> source;
            if (!useOceanNoise)
            {
                source = sourceFactory(rng);
                if (!source) continue;
            }

            // Азимут источника: -1 для океана, иначе — расчёт по индексу.
            float srcAz = AZIMUTH_OMNI;
            if (!useOceanNoise)
            {
                if (p_.uniformAzimuth)
                    srcAz = (static_cast<float>(i - 1) / p_.filesPerClass) * 360.0f;
                else
                    srcAz = p_.azimuthDeg;
            }

            SceneParams sp;
            sp.soundSpeed = SOUND_SPEED_WATER;
            sp.receiverSnrDb = p_.receiverSnrDb;
            sp.noiseSeed = static_cast<uint32_t>(rng());

            fillOceanDefaults(sp.oceanParams);
            if (useOceanNoise)
            {
                sp.oceanEnabled = true;
                sp.oceanParams.level = 0.5f + oceanLevelDist(rng) * 0.5f; // 0.5–0.8
            }
            else
            {
                sp.oceanEnabled = p_.mixWithOcean;
                sp.oceanParams.level = oceanLevelDist(rng);
            }
            sp.oceanParams.seed = static_cast<uint32_t>(rng());

            Scene scene;
            scene.setParams(sp);
            if (source)
            {
                source->setAzimuthDeg(srcAz);
                scene.addSource(std::move(source));
            }
            if (includeDolphin)
            {
                DolphinWhistleTestParams ddp;
                ddp.fStartMin = 2000.0f;
                ddp.fStartMax = 3000.0f;
                ddp.fEndMin = 800.0f;
                ddp.fEndMax = 1200.0f;
                ddp.phraseCount = 4;
                ddp.fMaxLimit = 6000.0f;
                ddp.harm1Db = -20.0f;
                ddp.harm2Db = -30.0f;
                ddp.harm3Db = -40.0f;
                ddp.crackleLevel = 4.0f;
                ddp.seed = static_cast<uint32_t>(rng());
                auto dsrc = std::make_unique<DolphinWhistleSource>(ddp);
                dsrc->setAzimuthDeg(dolphinAz);
                scene.addSource(std::move(dsrc));
            }

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
                << std::fixed << srcAz
                << "\u00b0";
            if (includeDolphin)
            {
                std::cout << "  dolphAz=" << std::setprecision(1) << dolphinAz
                    << "\u00b0";
            }
            std::cout << "  recvSNR=" << std::setprecision(1)
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

        // --- Диапазоны рандомизации для дельфина ---
        std::uniform_real_distribution<float> dolphFreqMinDist(
            ds::DOLPHIN_WHISTLE_FREQ_MIN_MIN, ds::DOLPHIN_WHISTLE_FREQ_MIN_MAX);
        std::uniform_real_distribution<float> dolphFreqMaxDist(
            ds::DOLPHIN_WHISTLE_FREQ_MAX_MIN, ds::DOLPHIN_WHISTLE_FREQ_MAX_MAX);

        int totalCreated = 0;

        // ---- 5 профилей подлодок (Sub_Heavy ... Sub_Medium) ----
        if (sel_.profiles)
        {
            for (const auto& profile : g_vesselProfiles)
            {
                std::uniform_real_distribution<float> propRpmJitter(0.95f, 1.05f);
                std::uniform_real_distribution<float> engRpmJitter(0.95f, 1.05f);
                std::uniform_real_distribution<float> cavJitter(0.9f, 1.1f);

                std::cout << "--- " << profile.name << " ---\n";

                totalCreated += generateClass(
                    profile.name,
                    [&](std::mt19937& r) -> std::unique_ptr<SourceModel>
                    {
                        PropellerParams pp;
                        pp.numBlades = profile.numBlades;
                        pp.rpm = profile.rpmProp * propRpmJitter(r);
                        pp.cavitation = profile.cavitation * cavJitter(r);
                        pp.depthM = PROP_DEPTH_M;
                        pp.numHarmonics = PROP_NUM_HARMONICS;
                        pp.modulationDepth = PROP_MODULATION_DEPTH;
                        pp.modulationHz = PROP_MODULATION_HZ;
                        pp.snrDb = profile.snrDb;
                        pp.seed = static_cast<uint32_t>(r());

                        EngineParams ep;
                        ep.rpm = profile.rpmEngine * engRpmJitter(r);
                        ep.numHarmonics = ENG_NUM_HARMONICS;
                        ep.numResonances = profile.numResonances;
                        ep.mechNoiseLevel = profile.mechNoiseLevel;
                        ep.snrDb = profile.snrDb;
                        ep.seed = static_cast<uint32_t>(r());

                        return std::make_unique<SubmarineProfileSource>(pp, ep);
                    },
                    [profile](const SourceModel* src) -> std::string
                    {
                        std::ostringstream os;
                        os << "{\n";
                        os << "    \"vessel_class\": \"" << profile.name << "\",\n";
                        os << "    \"num_blades\": " << profile.numBlades << ",\n";
                        os << "    \"rpm_prop\": " << profile.rpmProp << ",\n";
                        os << "    \"rpm_engine\": " << profile.rpmEngine << "\n";
                        os << "  }";
                        return os.str();
                    },
                    rng, false,
                    sel_.dolphinInProfiles, // includeDolphin (только если явно включено)
                    p_.dolphinAz); // dolphinAz
            }
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

                    dp.fStartMin = dolphFreqMinDist(r);
                    dp.fStartMax = dp.fStartMin + 1000.0f;
                    dp.fEndMin = dolphFreqMaxDist(r) * 0.4f;
                    dp.fEndMax = dp.fEndMin + 500.0f;

                    dp.phraseCount = 3 + (r() % 3);
                    dp.fMaxLimit = 6000.0f;

                    dp.harm1Db = -20.0f;
                    dp.harm2Db = -30.0f;
                    dp.harm3Db = -40.0f;

                    dp.crackleLevel = 4.0f;

                    dp.seed = static_cast<uint32_t>(r());

                    return std::make_unique<DolphinWhistleSource>(dp);
                },
                [](const SourceModel* src) -> std::string
                {
                    const auto* p = dynamic_cast<const DolphinWhistleSource*>(src);
                    if (!p) return "{}";
                    const auto& dp = p->params();
                    std::ostringstream os;
                    os << "{\n";
                    os << "    \"f_start_min\": " << std::fixed << std::setprecision(1) << dp.fStartMin << ",\n";
                    os << "    \"f_start_max\": " << std::setprecision(1) << dp.fStartMax << ",\n";
                    os << "    \"f_end_min\": " << std::setprecision(1) << dp.fEndMin << ",\n";
                    os << "    \"f_end_max\": " << std::setprecision(1) << dp.fEndMax << ",\n";
                    os << "    \"phrase_count\": " << dp.phraseCount << ",\n";
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
        f << "  \"_format_version\": \"2.0\",\n\n";
        f <<
            "  \"class\": \"ML class name: Sub_Heavy | Sub_Fast | Sub_Stealth | Sub_Rusty | Sub_Medium | Dolphin | OceanNoise\",\n";
        f <<
            "  \"sources\": \"array of active sources; each = { class, azimuth_deg }. OceanNoise has empty sources.\",\n";
        f << "  \"azimuth_deg\": \"source azimuth, degrees 0..359.9 (per source)\",\n";
        f << "  \"sample_rate\": \"WAV sample rate, Hz\",\n";
        f << "  \"duration_sec\": \"sample duration, seconds\",\n";
        f << "  \"num_channels\": \"WAV channel count (= 8)\",\n";
        f << "  \"receiver_snr_db\": \"receiver noise SNR, dB\",\n\n";
        f << "  \"params_SubmarineProfile\": {\n";
        f << "    \"vessel_class\": \"profile name\",\n";
        f << "    \"num_blades\": \"blade count\",\n";
        f << "    \"rpm_prop\": \"propeller shaft rpm\",\n";
        f << "    \"rpm_engine\": \"engine rpm\"\n";
        f << "  },\n\n";
        f << "  \"params_Dolphin\": {\n";
        f << "    \"f_start_min\": \"whistle start freq lower bound, Hz\",\n";
        f << "    \"f_start_max\": \"whistle start freq upper bound, Hz\",\n";
        f << "    \"f_end_min\": \"whistle end freq lower bound, Hz\",\n";
        f << "    \"f_end_max\": \"whistle end freq upper bound, Hz\",\n";
        f << "    \"phrase_count\": \"whistle phrases per sample\",\n";
        f << "    \"seed\": \"rng seed\"\n";
        f << "  },\n\n";
        f << "  \"params_OceanNoise\": {\n";
        f << "    \"_note\": \"ocean noise only, no active source\"\n";
        f << "  }\n";
        f << "}\n";
    }

    // ------------------------------------------------------------
    //  Сводный файл меток labels.json.
    //
    //  Формат: массив "files", для каждого файла — массив "sources"
    //  (даже если источник один). Это сразу готово к multi-source.
    //
    //  Для одиночных классов "sources" содержит один элемент:
    //    { "class": "Sub_Heavy", "azimuth_deg": 45.0 }
    //  Для OceanNoise — "sources": [] (нет активных источников),
    //  а сам файл — класс OceanNoise.
    // ------------------------------------------------------------
    void DatasetGenerator::writeLabelsFile() const
    {
        const fs::path path = fs::path(p_.outputDir) / "labels.json";
        std::ofstream f(path);
        if (!f) return;

        f << "{\n";
        f << "  \"version\": \"2.0\",\n";
        f << "  \"dataset_dir\": \"" << p_.outputDir << "\",\n";
        f << "  \"files_per_class\": " << p_.filesPerClass << ",\n";
        f << "  \"sample_rate\": " << static_cast<int>(p_.sampleRate) << ",\n";
        f << "  \"duration_sec\": " << p_.durationSec << ",\n";
        f << "  \"receiver_snr_db\": " << std::fixed << std::setprecision(2)
            << p_.receiverSnrDb << ",\n";
        f << "  \"files\": [\n";

        bool first = true;

        // Один класс: className, isOmni (true для OceanNoise — нет источников),
        // withDolphin (true для профилей, когда sel_.dolphin == true).
        auto scanClass = [&](const std::string& className,
                             bool isOmni,
                             bool withDolphin)
        {
            const fs::path classDir = fs::path(p_.outputDir) / className;
            if (!fs::exists(classDir)) return;

            for (int i = 1; i <= p_.filesPerClass; ++i)
            {
                const fs::path wavPath = classDir / makeFileName(i, ".wav");
                if (!fs::exists(wavPath)) continue;

                float az = 0.0f;
                if (!isOmni)
                {
                    if (p_.uniformAzimuth)
                        az = (static_cast<float>(i - 1) / p_.filesPerClass) * 360.0f;
                    else
                        az = p_.azimuthDeg;
                }

                if (!first) f << ",\n";
                first = false;

                f << "    { \"file\": \"" << className << "/"
                    << makeFileName(i, ".wav") << "\", "
                    << "\"class\": \"" << className << "\", "
                    << "\"receiver_snr_db\": " << std::setprecision(2)
                    << p_.receiverSnrDb << ", "
                    << "\"sources\": [";

                if (!isOmni)
                {
                    f << "{ \"class\": \"" << className
                        << "\", \"azimuth_deg\": " << std::setprecision(2)
                        << az << " }";
                    if (withDolphin)
                    {
                        f << ", { \"class\": \"Dolphin\", \"azimuth_deg\": "
                            << std::setprecision(2) << p_.dolphinAz << " }";
                    }
                }

                f << "] }";
            }
        };

        // 5 профилей + Dolphin + OceanNoise.
        if (sel_.profiles)
        {
            for (const auto& profile : g_vesselProfiles)
                scanClass(profile.name, false, sel_.dolphinInProfiles);
        }
        if (sel_.dolphin) scanClass("Dolphin", false, false);
        if (sel_.ocean) scanClass("OceanNoise", true, false);

        f << "\n  ]\n";
        f << "}\n";
    }


    // ============================================================
    //  Одиночная генерация 5 профилей подлодок.
    //
    //  Использует g_vesselProfiles (те же 5 сигнатур, что в пакетном
    //  режиме) и SubmarineProfileSource (адаптер винт+двигатель).
    //
    //  Небольшой джиттер вокруг профиля — как в generate():
    //    RPM винта:    ×[0.95, 1.05]
    //    RPM двигателя:×[0.95, 1.05]
    //    кавитация:    ×[0.9, 1.1]
    //  Остальные поля — ровно из профиля.
    // ============================================================
    int DatasetGenerator::generateSingleProfiles(const SceneParams& base,
                                                 int durationSec,
                                                 float azimuthDeg,
                                                 float dolphinAz,
                                                 bool includeDolphin,
                                                 bool mixOcean)
    {
        if (durationSec <= 0) durationSec = 10;

        const int totalSamples =
            static_cast<int>(p_.sampleRate) * durationSec;

        std::mt19937 rng(p_.masterSeed);

        std::uniform_real_distribution<float> propRpmJitter(0.95f, 1.05f);
        std::uniform_real_distribution<float> engRpmJitter(0.95f, 1.05f);
        std::uniform_real_distribution<float> cavJitter(0.9f, 1.1f);
        std::uniform_real_distribution<float> oceanLevelDist(
            ds::OCEAN_LEVEL_MIN, ds::OCEAN_LEVEL_MAX);

        int created = 0;

        for (const auto& profile : g_vesselProfiles)
        {
            // --- Параметры сцены (общие для всех профилей) ---
            SceneParams sp = base;
            sp.soundSpeed = SOUND_SPEED_WATER;
            sp.noiseSeed = static_cast<uint32_t>(rng());

            fillOceanDefaults(sp.oceanParams);
            sp.oceanEnabled = mixOcean;
            if (mixOcean)
            {
                sp.oceanParams.level = oceanLevelDist(rng);
            }
            sp.oceanParams.seed = static_cast<uint32_t>(rng());

            // --- Параметры винта из профиля + джиттер ---
            PropellerParams pp;
            pp.numBlades = profile.numBlades;
            pp.rpm = profile.rpmProp * propRpmJitter(rng);
            pp.cavitation = profile.cavitation * cavJitter(rng);
            pp.depthM = PROP_DEPTH_M;
            pp.numHarmonics = PROP_NUM_HARMONICS;
            pp.modulationDepth = PROP_MODULATION_DEPTH;
            pp.modulationHz = PROP_MODULATION_HZ;
            pp.snrDb = profile.snrDb;
            pp.seed = static_cast<uint32_t>(rng());

            // --- Параметры двигателя из профиля + джиттер ---
            EngineParams ep;
            ep.rpm = profile.rpmEngine * engRpmJitter(rng);
            ep.numHarmonics = ENG_NUM_HARMONICS;
            ep.numResonances = profile.numResonances;
            ep.mechNoiseLevel = profile.mechNoiseLevel;
            ep.snrDb = profile.snrDb;
            ep.seed = static_cast<uint32_t>(rng());

            Scene scene;
            scene.setParams(sp);
            {
                auto src = std::make_unique<SubmarineProfileSource>(pp, ep);
                src->setAzimuthDeg(azimuthDeg);
                scene.addSource(std::move(src));
            }
            if (includeDolphin)
            {
                DolphinWhistleTestParams ddp;
                ddp.fStartMin = 2000.0f;
                ddp.fStartMax = 3000.0f;
                ddp.fEndMin = 800.0f;
                ddp.fEndMax = 1200.0f;
                ddp.phraseCount = 4;
                ddp.fMaxLimit = 6000.0f;
                ddp.harm1Db = -20.0f;
                ddp.harm2Db = -30.0f;
                ddp.harm3Db = -40.0f;
                ddp.crackleLevel = 4.0f; // <--- громче стрёкот
                ddp.seed = static_cast<uint32_t>(rng());
                auto dsrc = std::make_unique<DolphinWhistleSource>(ddp);
                dsrc->setAzimuthDeg(dolphinAz);
                scene.addSource(std::move(dsrc));
            }

            std::array<std::vector<float>, NUM_CHANNELS> ch8;
            scene.generate(ch8, totalSamples, p_.sampleRate, uca_);

            const std::string wavName =
                "scene_" + profile.name + ".wav";
            if (!writeWavFloat32(wavName, ch8,
                                 static_cast<int>(p_.sampleRate)))
            {
                std::cerr << "  [FAIL] " << wavName << "\n";
                continue;
            }

            std::cout << "  " << wavName
                << "  (" << durationSec << " s, az="
                << std::fixed << std::setprecision(1)
                << azimuthDeg << "\u00b0)\n";

            ++created;
        }

        return created;
    }
} // namespace hydro
