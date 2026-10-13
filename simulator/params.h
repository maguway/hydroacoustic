#pragma once
#include <cstdint>
#include "types.h"

// ============================================================
//  ЕДИНЫЙ ИСТОЧНИК ПАРАМЕТРОВ.
//
//  ВСЕ ЧИСЛОВЫЕ ЗНАЧЕНИЯ — ЗДЕСЬ.
//  Структуры данных (PropellerParams, EngineParams, OceanNoiseParams)
//  находятся в своих заголовках. Они описывают СХЕМУ полей,
//  а конкретные значения берутся отсюда.
//
//  СЕКЦИЯ 1 — фиксированные значения (эталон для одиночных WAV).
//  СЕКЦИЯ 2 (namespace ds) — диапазоны для датасета.
// ============================================================

namespace hydro
{
    // ============================================================
    //  СЕКЦИЯ 1. ФИКСИРОВАННЫЕ ЗНАЧЕНИЯ (эталон)
    // ============================================================

    // ---------- ОБЩИЕ ПАРАМЕТРЫ СЦЕНЫ ----------

    constexpr int SCENE_DURATION_SEC = 5;
    constexpr float SCENE_AZIMUTH_DEG = 45.0f;

    // ---------- ШУМ ПРИЁМНИКА (глобальный, внутри Propagation) ----------
    // Чем БОЛЬШЕ число — тем ТИШЕ шум.
    //   +120 → шум 0 | +40 → почти чисто | +10 → заметный
    //   0 → шум = сигнал | -10 → сигнал на грани | -40 → только шум
    constexpr float SCENE_RECEIVER_SNR_DB = 30.0f;

    // ============================================================
    //  ПАРАМЕТРЫ ИСТОЧНИКА «ВИНТ ПЛ» (PropellerSubmarine)
    // ============================================================

    constexpr int PROP_NUM_BLADES = 7;
    constexpr float PROP_RPM = 400.0f;
    constexpr float PROP_DEPTH_M = 100.0f;
    constexpr int PROP_NUM_HARMONICS = 6;
    constexpr float PROP_MODULATION_DEPTH = 0.2f;
    constexpr float PROP_MODULATION_HZ = 0.3f;
    constexpr float PROP_CAVITATION = 0.3f;
    constexpr float PROP_SNR_DB = 30.0f;
    constexpr uint32_t PROP_SEED = 12345;

    // ============================================================
    //  ПАРАМЕТРЫ ИСТОЧНИКА «ДВИГАТЕЛЬ ПЛ» (EngineSubmarine)
    // ============================================================

    constexpr float ENG_RPM = 300.0f;
    constexpr int ENG_NUM_HARMONICS = 4;
    constexpr int ENG_NUM_RESONANCES = 3;
    constexpr float ENG_MECH_NOISE_LEVEL = 0.2f;
    constexpr float ENG_SNR_DB = 30.0f;
    constexpr uint32_t ENG_SEED = 54321;


    // ============================================================
    //  ПАРАМЕТРЫ ДЕЛЬФИНА-АФАЛИНЫ (Dolphin, Tursiops truncatus)
    // ============================================================

    // --- Свисты (коммуникация) ---
    constexpr int DOLPHIN_WHISTLES_PER_SEC = 3;
    constexpr float DOLPHIN_WHISTLE_DUR_MIN = 0.4f;
    constexpr float DOLPHIN_WHISTLE_DUR_MAX = 0.8f;
    constexpr float DOLPHIN_WHISTLE_FREQ_MIN = 2000.0f;
    constexpr float DOLPHIN_WHISTLE_FREQ_MAX = 9000.0f;
    constexpr float DOLPHIN_WHISTLE_SWEEP = 0.6f;
    constexpr int DOLPHIN_NUM_HARMONICS = 2;

    // --- Щелчки (эхолокация, суррогатные) ---
    constexpr float DOLPHIN_CLICK_RATE = 1.0f; // серий в секунду
    constexpr float DOLPHIN_CLICK_DUR_US = 100.0f; // мкс
    constexpr float DOLPHIN_CLICK_FREQ_MIN = 1000.0f;
    constexpr float DOLPHIN_CLICK_FREQ_MAX = 8000.0f;
    constexpr int DOLPHIN_CLICKS_PER_TRAIN = 3;

    // --- Burst pulses (жужжание) ---
    constexpr float DOLPHIN_BURST_RATE = 0.3f; // вспышек в секунду
    constexpr float DOLPHIN_BURST_DUR_MIN = 0.3f;
    constexpr float DOLPHIN_BURST_DUR_MAX = 1.0f;
    constexpr float DOLPHIN_BURST_CLICK_US = 500.0f; // интервал между щелчками, мкс
    constexpr float DOLPHIN_BURST_FREQ_MIN = 2000.0f;
    constexpr float DOLPHIN_BURST_FREQ_MAX = 8000.0f;

    // --- Общие ---
    constexpr float DOLPHIN_SNR_DB = 30.0f;
    constexpr uint32_t DOLPHIN_SEED = 77777;


    // ============================================================
    //  ПАРАМЕТРЫ ШУМА ОКЕАНА (OceanNoise). ЭТАП 1: всплески + эхо + волны.
    // ============================================================

    constexpr float OCEAN_LEVEL = 0.5f;

    // Фон (выключен).
    // Фон (включён — сшивает волны между собой).
    constexpr float OCEAN_RUMBLE_WEIGHT = 0.15f;
    constexpr float OCEAN_MID_WEIGHT = 0.10f;
    constexpr float OCEAN_HISS_WEIGHT = 0.05f;

    // Насколько фон модулируется волнами (0 = ровный, 1 = сильно дышит).
    constexpr float OCEAN_BG_MODULATION = 0.7f;

    constexpr float OCEAN_RUMBLE_SLOPE = 0.97f;
    constexpr float OCEAN_MID_SLOPE = 0.65f;
    constexpr float OCEAN_HISS_SLOPE = 0.25f;

    // Спайки.
    // constexpr float OCEAN_SPIKE_RATE        = 3.0f;
    constexpr float OCEAN_SPIKE_RATE = 0.0f;
    constexpr float OCEAN_SPIKE_AMP = 0.5f;
    constexpr float OCEAN_SPIKE_SPATIAL_FRAC = 0.9f;

    constexpr float OCEAN_DROP_TAU_MIN_MS = 8.0f;
    constexpr float OCEAN_DROP_TAU_MAX_MS = 20.0f;

    constexpr float OCEAN_BUBBLE_TAU_MIN_MS = 40.0f;
    constexpr float OCEAN_BUBBLE_TAU_MAX_MS = 90.0f;
    constexpr float OCEAN_BUBBLE_F0_MIN = 500.0f;
    constexpr float OCEAN_BUBBLE_F0_MAX = 1200.0f;
    constexpr float OCEAN_BUBBLE_F1_MIN = 150.0f;
    constexpr float OCEAN_BUBBLE_F1_MAX = 350.0f;

    constexpr float OCEAN_CLICK_TAU_MIN_MS = 4.0f;
    constexpr float OCEAN_CLICK_TAU_MAX_MS = 10.0f;

    // Эхо.
    constexpr int OCEAN_ECHO_COUNT = 2;
    constexpr float OCEAN_ECHO_DELAY_MIN_MS = 80.0f;
    constexpr float OCEAN_ECHO_DELAY_MAX_MS = 350.0f;
    constexpr float OCEAN_ECHO_GAIN_MIN = 0.40f;
    constexpr float OCEAN_ECHO_GAIN_MAX = 0.75f;
    constexpr float OCEAN_ECHO_AZ_SPREAD_DEG = 55.0f;

    // Волны.
    constexpr float OCEAN_WAVE_RATE = 0.35f; // 1 волна раз в 2.9 секунды
    constexpr float OCEAN_WAVE_AMP = 0.25f;
    constexpr float OCEAN_WAVE_DURATION_MIN_S = 1.5f;
    constexpr float OCEAN_WAVE_DURATION_MAX_S = 2.5f;
    constexpr float OCEAN_WAVE_SPATIAL_FRAC = 0.9f;

    // Модуляция (выключена).
    constexpr float OCEAN_MODULATION_DEPTH = 0.0f;
    constexpr float OCEAN_MODULATION_HZ = 0.15f;

    constexpr uint32_t OCEAN_SEED = 777;

    // ============================================================
    //  СЕКЦИЯ 2. ДИАПАЗОНЫ ДЛЯ ДАТАСЕТА.
    //  Используются ТОЛЬКО в generator.cpp (пункт «2»).
    // ============================================================

    namespace ds
    {
        // --- Винт: сигнал ---
        constexpr int PROP_NUM_BLADES_MIN = 5;
        constexpr int PROP_NUM_BLADES_MAX = 9;
        constexpr float PROP_RPM_MIN = 150.0f;
        constexpr float PROP_RPM_MAX = 500.0f;
        constexpr int PROP_NUM_HARMONICS_MIN = 4;
        constexpr int PROP_NUM_HARMONICS_MAX = 8;
        constexpr float PROP_MOD_DEPTH_MIN = 0.1f;
        constexpr float PROP_MOD_DEPTH_MAX = 0.4f;
        constexpr float PROP_MOD_HZ_MIN = 0.1f;
        constexpr float PROP_MOD_HZ_MAX = 0.5f;

        // --- Винт: шум ---
        constexpr float PROP_CAVITATION_MIN = 0.1f;
        constexpr float PROP_CAVITATION_MAX = 0.5f;
        constexpr float PROP_SNR_DB_MIN = 20.0f;
        constexpr float PROP_SNR_DB_MAX = 40.0f;

        // --- Двигатель: сигнал ---
        constexpr float ENG_RPM_MIN = 200.0f;
        constexpr float ENG_RPM_MAX = 800.0f;
        constexpr int ENG_NUM_HARMONICS_MIN = 2;
        constexpr int ENG_NUM_HARMONICS_MAX = 6;
        constexpr int ENG_NUM_RESONANCES_MIN = 0;
        constexpr int ENG_NUM_RESONANCES_MAX = 3;

        // --- Двигатель: шум ---
        constexpr float ENG_MECH_NOISE_MIN = 0.1f;
        constexpr float ENG_MECH_NOISE_MAX = 0.4f;
        constexpr float ENG_SNR_DB_MIN = 20.0f;
        constexpr float ENG_SNR_DB_MAX = 40.0f;


        // --- Дельфин: свисты ---
        constexpr int   DOLPHIN_WHISTLES_PER_SEC_MIN = 2;
        constexpr int   DOLPHIN_WHISTLES_PER_SEC_MAX = 5;
        constexpr float DOLPHIN_WHISTLE_FREQ_MIN_MIN = 1500.0f;
        constexpr float DOLPHIN_WHISTLE_FREQ_MIN_MAX = 2500.0f;
        constexpr float DOLPHIN_WHISTLE_FREQ_MAX_MIN = 7000.0f;
        constexpr float DOLPHIN_WHISTLE_FREQ_MAX_MAX = 10000.0f;

        // --- Дельфин: щелчки ---
        constexpr float DOLPHIN_CLICK_RATE_MIN = 0.3f;
        constexpr float DOLPHIN_CLICK_RATE_MAX = 2.0f;

        // --- Дельфин: burst pulses ---
        constexpr float DOLPHIN_BURST_RATE_MIN = 0.1f;
        constexpr float DOLPHIN_BURST_RATE_MAX = 0.6f;

        // --- Дельфин: SNR ---
        constexpr float DOLPHIN_SNR_DB_MIN = 20.0f;
        constexpr float DOLPHIN_SNR_DB_MAX = 40.0f;


        // --- Сцена ---
        constexpr float SCENE_AZIMUTH_MIN = 0.0f;
        constexpr float SCENE_AZIMUTH_MAX = 359.9f;
        constexpr float SCENE_RECEIVER_SNR_MIN = 20.0f;
        constexpr float SCENE_RECEIVER_SNR_MAX = 40.0f;

        // --- Шум океана (для датасета, шире, чем фиксированный) ---
        //   От почти полного штиля до сильного шторма.
        constexpr float OCEAN_LEVEL_MIN = 0.0f;
        constexpr float OCEAN_LEVEL_MAX = 0.6f;

        //   Всплески — от «спокойно» до «дождя» (часто).
        constexpr float OCEAN_SPIKE_RATE_MIN = 0.1f;
        constexpr float OCEAN_SPIKE_RATE_MAX = 5.0f;

        //   Модуляция — глубина и частота.
        constexpr float OCEAN_MOD_DEPTH_MIN = 0.1f;
        constexpr float OCEAN_MOD_DEPTH_MAX = 0.5f;
        constexpr float OCEAN_MOD_HZ_MIN = 0.05f;
        constexpr float OCEAN_MOD_HZ_MAX = 0.5f;
    } // namespace ds
} // namespace hydro
