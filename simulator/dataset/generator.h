#pragma once
#include "uca_geometry.h"
#include "scene.h"
#include "source_model.h"

#include <string>
#include <cstdint>
#include <functional>
#include <random>
#include <memory>

namespace hydro
{
    // ============================================================
    //  Генератор синтетического датасета.
    //
    //  Создаёт N сэмплов на каждый ВЫБРАННЫЙ класс:
    //    dataset/<ClassName>/<NNNN>.wav  — 8-канальный WAV, 32-bit float
    //    dataset/<ClassName>/<NNNN>.json — метаданные
    //
    //  Плюс один раз пишет dataset/_schema.json.
    //
    //  Директории относительные — считаются от рабочей директории запуска.
    // ============================================================

    struct DatasetParams
    {
        int filesPerClass = 10; // файлов на класс
        int durationSec = 2; // длительность сэмпла
        float sampleRate = 10000.0f; // fs
        std::string outputDir = "dataset"; // куда писать
        uint32_t masterSeed = 20260924; // seed рандомизации

        // --- Общие параметры сцены (одинаковые для всех файлов) ---
        float receiverSnrDb = 30.0f; // SNR приёмника

        // --- Азимут ---
        bool uniformAzimuth = true; // true = равномерно 0..359
        // false = фиксированный azimuthDeg
        float azimuthDeg = 45.0f; // используется если uniformAzimuth = false

        // --- НОВОЕ ---
        bool mixWithOcean = true; // Добавлять ли звук океана к чистым классам
    };

    // Что генерировать. Минимум один класс должен быть включён.
    struct DatasetSelection
    {
        bool propeller = true;
        bool engine = true;
        bool dolphin = true;
        bool ocean = true;
    };

    class DatasetGenerator
    {
    public:
        DatasetGenerator(const UcaGeometry& uca,
                         const DatasetParams& params,
                         const DatasetSelection& selection);

        // Сгенерировать выбранные классы.
        // Возвращает число успешно созданных WAV-файлов.
        int generate();

    private:
        const UcaGeometry& uca_;
        DatasetParams p_;
        DatasetSelection sel_;

        // --- Один класс ---
        // className       — имя класса ("PropellerSub", "EngineSub", "Dolphin", "OceanNoise")
        // sourceFactory   — лямбда, создающая источник (или nullptr для OceanNoise)
        // jsonExtraFields — функция, формирующая params для JSON
        // rng             — общий RNG
        // useOceanNoise   — если true, генерируется только шум океана (без сигнала)
        int generateClass(
            const std::string& className,
            const std::function<std::unique_ptr<SourceModel>(std::mt19937&)>& sourceFactory,
            const std::function<std::string(const SourceModel*)>& jsonExtraFields,
            std::mt19937& rng,
            bool useOceanNoise) const;

        void writeSchemaFile() const;
        void writeLabelsFile() const;
        int generateCombinedClass(std::mt19937& rng) const;
        static std::string makeFileName(int index, const std::string& ext);
    };
} // namespace hydro
