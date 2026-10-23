#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <vector>

// ============================================================
//  Общие константы проекта hydroacoustic
//  Всё, что НЕ зависит от GUI и НЕ зависит от LibTorch.
// ============================================================

namespace hydro
{
    // ---------- Решётка ----------
    constexpr int NUM_CHANNELS = 8; // 8 гидрофонов (UCA)
    constexpr float UCA_RADIUS_M = 0.5f; // радиус круговой решётки, м

    // ---------- Дискретизация ----------
    constexpr int SAMPLE_RATE = 22050; // Гц (Найквист 11025 Гц)
    constexpr int FFT_SIZE = 2048; // для анализа
    constexpr int FFT_SIZE_MIN = 256;
    constexpr int FFT_SIZE_MAX = 8192;

    // ---------- Физика среды ----------
    constexpr float SOUND_SPEED_WATER = 1500.0f; // м/с
    constexpr float SOUND_SPEED_AIR = 330.0f; // м/с

    // Азимут не определён / источник все направлений (океан).
    constexpr float AZIMUTH_OMNI = -1.0f;

    // ---------- Диапазоны сигнатур ----------
    // Ниже — «разумные» границы для гидроакустики.
    // Используются генераторами как пределы варьирования.
    namespace band
    {
        constexpr float AUDIO_LOW_HZ = 10.0f;
        constexpr float AUDIO_HIGH_HZ = 9000.0f; // < Найквиста (11025 Гц)
        constexpr float BLADE_LOW_HZ = 2.0f; // лопастная частота — минимум
        constexpr float BLADE_HIGH_HZ = 40.0f; // лопастная частота — максимум
        constexpr float ENGINE_LOW_HZ = 5.0f;
        constexpr float ENGINE_HIGH_HZ = 200.0f;
    }

    // ---------- Идентификаторы сред ----------
    enum class Medium
    {
        Water = 0,
        Air = 1,
    };

    inline float soundSpeed(Medium m)
    {
        return (m == Medium::Water) ? SOUND_SPEED_WATER : SOUND_SPEED_AIR;
    }

    // ---------- Идентификаторы ML-классов ----------
    // ВАЖНО: это метки для обучения, а не C++-классы генераторов.
    enum class SourceClass : int
    {
        Unknown = -1,
        PropellerSub = 0, // винт подводной лодки
        EngineSub = 1, // двигатель подводной лодки
        OceanNoise = 2, // шум океана (нуль-класс / фон)
        Dolphin = 3, // дельфин-афалина (Tursiops truncatus)
        // --- следующие добавим позже ---
        // SurfaceShip   = 4,
        // Submersible   = 5,
        // Whale         = 6,
        // Orca          = 7,
    };

    inline const char* sourceClassName(SourceClass c)
    {
        switch (c)
        {
        case SourceClass::PropellerSub: return "PropellerSub";
        case SourceClass::EngineSub: return "EngineSub";
        case SourceClass::OceanNoise: return "OceanNoise";
        case SourceClass::Dolphin: return "Dolphin";
        default: return "Unknown";
        }
    }

    // ---------- Общие структуры ----------
    struct Azimuth
    {
        // Азимут храним в градусах 0..360, но внутри удобно в радианах.
        float deg = 0.0f;
    };
} // namespace hydro
