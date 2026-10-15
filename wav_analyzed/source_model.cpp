#include "source_model.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace hydro
{
    // ------------------------------------------------------------
    //  PropellerSubmarine
    // ------------------------------------------------------------

    PropellerSubmarine::PropellerSubmarine(const PropellerParams& p)
        : p_(p), rng_(p.seed)
    {
        regeneratePhases();
    }

    void PropellerSubmarine::setParams(const PropellerParams& p)
    {
        p_ = p;
        rng_.seed(p.seed);
        regeneratePhases();
    }

    void PropellerSubmarine::reset()
    {
        rng_.seed(p_.seed);
        regeneratePhases();
    }

    void PropellerSubmarine::regeneratePhases()
    {
        const int K = std::max(1, p_.numHarmonics);
        harmonicPhases_.resize(K);
        harmonicAmplitudes_.resize(K);

        std::uniform_real_distribution<float> phaseDist(0.0f, TWO_PI_F);
        for (int k = 0; k < K; ++k)
        {
            harmonicPhases_[k] = phaseDist(rng_);
            // Убывание: A_k = 1 / (k+1)
            harmonicAmplitudes_[k] = 1.0f / static_cast<float>(k + 1);
        }
    }

    void PropellerSubmarine::generate(float* out,
                                      int numSamples,
                                      float fs,
                                      int64_t sampleOffset)
    {
        const float f_blade = p_.bladeFreqHz();
        const float f_mod = p_.modulationHz;
        const float m = p_.modulationDepth;
        const float invFs = 1.0f / fs;

        // --- 1. Вычисляем амплитуду белого шума из SNR ---
        float P_sig = 0.0f;
        for (float A : harmonicAmplitudes_) P_sig += 0.5f * A * A;
        P_sig *= (1.0f + 0.5f * m * m);

        float P_noise = 0.0f;
        if (p_.snrDb > 100.0f)
        {
            P_noise = 0.0f;
        }
        else if (p_.snrDb < -40.0f)
        {
            P_noise = P_sig * 1e4f;
        }
        else
        {
            P_noise = P_sig / std::pow(10.0f, p_.snrDb / 10.0f);
        }
        const float noiseAmp = std::sqrt(P_noise);

        // --- 2. Кавитационный шум ---
        // Белый шум, модулированный медленной огибающей.
        // Простой белый, без фильтрации — как в эталонной версии.
        const float cavAmp = p_.cavitation;

        // --- 3. Генерация отсчётов ---
        for (int n = 0; n < numSamples; ++n)
        {
            const int64_t globalN = sampleOffset + n;
            const float t = static_cast<float>(globalN) * invFs;

            // 3a. Сумма гармоник с модуляцией
            float s = 0.0f;
            for (size_t k = 0; k < harmonicAmplitudes_.size(); ++k)
            {
                const float fk = f_blade * static_cast<float>(k + 1);
                const float phase = TWO_PI_F * fk * t + harmonicPhases_[k];
                s += harmonicAmplitudes_[k] * std::sin(phase);
            }

            // Амплитудная модуляция
            const float modFactor = 1.0f + m * std::sin(TWO_PI_F * f_mod * t);
            s *= modFactor;

            // 3b. Кавитационный шум — чистый белый
            const float cav = gauss_(rng_) * cavAmp * 0.5f;

            // 3c. Белый шум приёмника
            const float wgn = gauss_(rng_) * noiseAmp;

            out[n] = s + cav + wgn;
        }
    }


    // ------------------------------------------------------------
    //  EngineSubmarine
    // ------------------------------------------------------------

    EngineSubmarine::EngineSubmarine(const EngineParams& p)
        : p_(p), rng_(p.seed)
    {
        regeneratePhases();
    }

    void EngineSubmarine::setParams(const EngineParams& p)
    {
        p_ = p;
        rng_.seed(p.seed);
        regeneratePhases();
    }

    void EngineSubmarine::reset()
    {
        rng_.seed(p_.seed);
        regeneratePhases();
    }

    void EngineSubmarine::regeneratePhases()
    {
        const int K = std::max(1, p_.numHarmonics);
        harmonicPhases_.resize(K);
        harmonicAmplitudes_.resize(K);

        std::uniform_real_distribution<float> phaseDist(0.0f, TWO_PI_F);
        for (int k = 0; k < K; ++k)
        {
            harmonicPhases_[k] = phaseDist(rng_);
            harmonicAmplitudes_[k] = 1.0f / static_cast<float>(k + 1);
        }
    }

    void EngineSubmarine::generate(float* out,
                                   int numSamples,
                                   float fs,
                                   int64_t sampleOffset)
    {
        const float f_engine = p_.engineFreqHz();
        const float invFs = 1.0f / fs;

        // --- 1. Средняя мощность сигнала (для расчёта SNR) ---
        float P_sig = 0.0f;
        for (float A : harmonicAmplitudes_) P_sig += 0.5f * A * A;
        for (int r = 0; r < p_.numResonances && r < 3; ++r)
        {
            P_sig += 0.5f * RES_AMPS[r] * RES_AMPS[r];
        }

        // --- 2. Шум по SNR ---
        float P_noise = 0.0f;
        if (p_.snrDb > 100.0f)
        {
            P_noise = 0.0f;
        }
        else if (p_.snrDb < -40.0f)
        {
            P_noise = P_sig * 1e4f;
        }
        else
        {
            P_noise = P_sig / std::pow(10.0f, p_.snrDb / 10.0f);
        }
        const float noiseAmp = std::sqrt(P_noise);

        // --- 3. Генерация отсчётов ---
        for (int n = 0; n < numSamples; ++n)
        {
            const int64_t globalN = sampleOffset + n;
            const float t = static_cast<float>(globalN) * invFs;

            // 3a. Гармоники двигателя
            float s = 0.0f;
            for (size_t k = 0; k < harmonicAmplitudes_.size(); ++k)
            {
                const float fk = f_engine * static_cast<float>(k + 1);
                const float phase = TWO_PI_F * fk * t + harmonicPhases_[k];
                s += harmonicAmplitudes_[k] * std::sin(phase);
            }

            // 3b. Резонансы корпуса
            for (int r = 0; r < p_.numResonances && r < 3; ++r)
            {
                s += RES_AMPS[r] * std::sin(TWO_PI_F * RES_FREQS[r] * t);
            }

            // 3c. Механический широкополосный шум — чистый белый
            const float mech = gauss_(rng_) * p_.mechNoiseLevel * 0.3f;

            // 3d. Белый шум приёмника
            const float wgn = gauss_(rng_) * noiseAmp;

            out[n] = s + mech + wgn;
        }
    }


    // ============================================================
    //  Dolphin — дельфин-афалина (Tursiops truncatus)
    // ============================================================

    Dolphin::Dolphin(const DolphinParams& p)
        : p_(p), rng_(p.seed)
    {
    }

    void Dolphin::setParams(const DolphinParams& p)
    {
        p_ = p;
        rng_.seed(p.seed);
    }

    void Dolphin::reset()
    {
        rng_.seed(p_.seed);
    }

    // ------------------------------------------------------------
    //  Генерация событий: свисты
    //
    //  Свист — тональный сигнал с частотной модуляцией (ЧМ).
    //  Частота плавно меняется от f0 к f1 за время whistleDuration.
    //  Форма ЧМ — линейная (для простоты), но с добавлением
    //  небольшой случайности в направлении (вверх/вниз).
    // ------------------------------------------------------------
    std::vector<Dolphin::Whistle>
    Dolphin::generateWhistles(int numSamples, float fs, uint32_t seed)
    {
        std::vector<Whistle> whistles;
        if (p_.numWhistlesPerSec <= 0) return whistles;

        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> uniform01(0.0f, 1.0f);
        std::uniform_real_distribution<float> phaseDist(0.0f, TWO_PI_F);

        const float durationSec = static_cast<float>(numSamples) / fs;

        // Средний период между свистами.
        const float meanPeriod = 1.0f / static_cast<float>(p_.numWhistlesPerSec);
        // Джиттер ±40%.
        const float jitterRange = 0.4f * meanPeriod;
        std::uniform_real_distribution<float> jitterDist(-jitterRange, +jitterRange);

        float t = 0.0f;
        while (t < durationSec)
        {
            Whistle w;

            w.sampleIndex = static_cast<int>(t * fs);
            if (w.sampleIndex >= numSamples) break;

            // Длительность свиста.
            const float durSec = p_.whistleDurationMin +
                uniform01(rng) * (p_.whistleDurationMax - p_.whistleDurationMin);
            w.lengthSamples = static_cast<int>(durSec * fs);
            if (w.sampleIndex + w.lengthSamples > numSamples)
            {
                w.lengthSamples = numSamples - w.sampleIndex;
            }
            if (w.lengthSamples <= 0) break;

            // Диапазон частот свиста.
            const float fLow = p_.whistleFreqMin;
            const float fHigh = p_.whistleFreqMax;
            const float fRange = fHigh - fLow;

            // Направление ЧМ: 0 — восходящий, 1 — нисходящий, 2 — U-образный.
            // Равновероятно.
            const int type = static_cast<int>(uniform01(rng) * 3.0f);
            if (type == 0)
            {
                // Восходящий: от fLow + немного к fHigh - немного.
                w.f0 = fLow + uniform01(rng) * fRange * 0.3f;
                w.f1 = fHigh - uniform01(rng) * fRange * 0.3f;
            }
            else if (type == 1)
            {
                // Нисходящий.
                w.f0 = fHigh - uniform01(rng) * fRange * 0.3f;
                w.f1 = fLow + uniform01(rng) * fRange * 0.3f;
            }
            else
            {
                // U-образный: сначала вниз, потом вверх — эмулируем
                // через начальную и конечную частоту, близкие друг к другу.
                // Реализуется как «сначала нисходящий, потом восходящий» —
                // но для простоты делаем нисходящий с возвратом.
                // В renderWhistle будем использовать нелинейную ЧМ.
                w.f0 = fHigh - uniform01(rng) * fRange * 0.2f;
                w.f1 = fHigh - uniform01(rng) * fRange * 0.2f;
            }

            w.phase = phaseDist(rng);

            whistles.push_back(w);

            // Следующий свист — через meanPeriod ± джиттер.
            t += meanPeriod + jitterDist(rng);
            if (t < 0.0f) t = 0.0f;
        }

        return whistles;
    }

    // ------------------------------------------------------------
    //  Рендер одного свиста в буфер.
    //
    //  Сигнал: sin(2*pi * f(t) * t + phase) * envelope(t),
    //  где f(t) меняется линейно от f0 к f1,
    //  envelope — плавное нарастание и спад (полкосинуса).
    //
    //  Гармоники: добавляем numHarmonics-1 обертонов с амплитудой
    //  1/k. Основная — 1.0.
    // ------------------------------------------------------------
    void Dolphin::renderWhistle(std::vector<float>& buffer,
                                int numSamples,
                                float fs,
                                const Whistle& w)
    {
        if (w.lengthSamples <= 0) return;

        const float invFs = 1.0f / fs;
        const float durSec = static_cast<float>(w.lengthSamples) * invFs;

        // Тип ЧМ: определяем по соотношению f0 и f1.
        // Если f0 < f1 — восходящий, если f0 > f1 — нисходящий,
        // если близки — U-образный.
        // Для U-образного сделаем f(t) = f0 - depth * sin(pi * t/dur).
        const float fDiff = w.f1 - w.f0;
        const bool isU = (std::fabs(fDiff) < 200.0f);

        const int K = std::max(1, p_.numHarmonics);

        for (int n = 0; n < w.lengthSamples; ++n)
        {
            const int idx = w.sampleIndex + n;
            if (idx < 0 || idx >= numSamples) continue;

            const float tLocal = static_cast<float>(n) * invFs;
            const float u = tLocal / durSec; // 0..1

            // Огибающая: плавный подъём и спад (полкосинус на краях).
            // Используем «окно Ханна»-подобное: 0.5*(1 - cos(2*pi*u)),
            // но для лучшей формы — комбинацию: rise 0.1, fall 0.1.
            float env = 1.0f;
            const float riseFrac = 0.1f;
            const float fallFrac = 0.1f;
            if (u < riseFrac)
            {
                env = 0.5f * (1.0f - std::cos(PI_F * u / riseFrac));
            }
            else if (u > 1.0f - fallFrac)
            {
                const float v = (1.0f - u) / fallFrac;
                env = 0.5f * (1.0f - std::cos(PI_F * v));
            }

            // Мгновенная частота.
            float fInst;
            if (isU)
            {
                // U-образная: f падает от f0 до f0 - depth, потом растёт.
                // depth = 0.4 * (fHigh - fLow), но берём из f0.
                const float depth = 0.4f * (p_.whistleFreqMax - p_.whistleFreqMin);
                fInst = w.f0 - depth * std::sin(PI_F * u);
            }
            else
            {
                // Линейная: от f0 к f1.
                fInst = w.f0 + fDiff * u;
            }

            // Основной тон + гармоники.
            float s = 0.0f;
            for (int k = 0; k < K; ++k)
            {
                const float fk = fInst * static_cast<float>(k + 1);
                // Отсечка выше Найквиста.
                if (fk >= fs * 0.5f) break;
                const float amp = 1.0f / static_cast<float>(k + 1);
                s += amp * std::sin(TWO_PI_F * fk * tLocal + w.phase);
            }

            buffer[idx] += s * env;
        }
    }

    // ------------------------------------------------------------
    //  Генерация событий: щелчки (эхолокационные, суррогатные).
    //
    //  Суррогатный щелчок — короткий импульс (50–150 мкс),
    //  заполненный широкополосным сигналом. Настоящие эхолокационные
    //  щелчки афалины — до 100+ кГц, при fs=22050 мы их не можем
    //  воспроизвести. Поэтому делаем «структурно похожий» сигнал:
    //  короткий, широкополосный, в диапазоне 1–8 кГц.
    //
    //  Щелчки идут сериями (click trains), 2–5 в серии.
    // ------------------------------------------------------------
    std::vector<Dolphin::Click>
    Dolphin::generateClicks(int numSamples, float fs, uint32_t seed)
    {
        std::vector<Click> clicks;
        if (p_.clickRate <= 0.0f || p_.clicksPerTrain <= 0) return clicks;

        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> uniform01(0.0f, 1.0f);
        std::uniform_real_distribution<float> phaseDist(0.0f, TWO_PI_F);
        std::uniform_real_distribution<float> ampJitter(0.7f, 1.3f);

        const float durationSec = static_cast<float>(numSamples) / fs;
        const float meanPeriod = 1.0f / p_.clickRate;
        const float jitterRange = 0.4f * meanPeriod;
        std::uniform_real_distribution<float> jitterDist(-jitterRange, +jitterRange);

        float t = 0.0f;
        while (t < durationSec)
        {
            // Начало серии.
            const int trainStart = static_cast<int>(t * fs);
            if (trainStart >= numSamples) break;

            // Внутри серии — clicksPerTrain щелчков через ~50 мс.
            // Интервал между щелчками в серии — не параметр (фиксированный
            // реалистичный ~50 мс), потому что в реальности он у афалины
            // именно такой при эхолокации.
            const float intraTrainInterval = 0.05f; // 50 мс

            for (int k = 0; k < p_.clicksPerTrain; ++k)
            {
                Click c;
                c.sampleIndex = trainStart +
                    static_cast<int>(k * intraTrainInterval * fs);
                if (c.sampleIndex >= numSamples) break;

                c.amplitude = ampJitter(rng);
                c.phase = phaseDist(rng);

                clicks.push_back(c);
            }

            // Следующая серия.
            t += meanPeriod + jitterDist(rng);
            if (t < 0.0f) t = 0.0f;
        }

        return clicks;
    }

    // ------------------------------------------------------------
    //  Рендер одного щелчка.
    //
    //  Сигнал: короткий импульс с гауссовой огибающей,
    //  заполненный широкополосным сигналом.
    //
    //  Широкополосность эмулируем через сумму нескольких синусоид
    //  со случайными частотами в диапазоне clickFreqMin..Max,
    //  либо через белый шум × гауссова огибающая.
    //
    //  Второй способ проще и звучит похоже.
    // ------------------------------------------------------------
    void Dolphin::renderClick(std::vector<float>& buffer,
                              int numSamples,
                              float fs,
                              const Click& c)
    {
        const float durSec = p_.clickDurationUs * 1e-6f;
        const int len = static_cast<int>(durSec * fs) + 1;
        if (len <= 0) return;

        // Гауссова огибающая: exp(-(t/tau)^2), tau = durSec / 4.
        const float tau = durSec / 4.0f;
        const float invTau2 = 1.0f / (tau * tau);
        const float invFs = 1.0f / fs;

        // Генератор белого шума — локальный, чтобы не портить основной rng_.
        // Это важно: иначе порядок вызовов rng_ стал бы непредсказуемым
        // при изменении числа щелчков.
        std::mt19937 localRng(static_cast<uint32_t>(c.sampleIndex) ^ 0x9E3779B9u);
        std::normal_distribution<float> localGauss(0.0f, 1.0f);

        for (int n = 0; n < len; ++n)
        {
            const int idx = c.sampleIndex + n;
            if (idx < 0 || idx >= numSamples) continue;

            const float tLocal = static_cast<float>(n) * invFs;
            const float env = std::exp(-(tLocal * tLocal) * invTau2);

            // Белый шум как заполнение.
            const float white = localGauss(localRng);

            buffer[idx] += c.amplitude * env * white;
        }
    }

    // ------------------------------------------------------------
    //  Генерация событий: burst pulses (жужжание).
    //
    //  Burst — серия очень быстрых щелчков (интервал ~500 мкс),
    //  сливающихся в непрерывное жужжание. Длительность 0.3–1 сек.
    //
    //  Воспринимается как «трещотка» или «жужжание».
    // ------------------------------------------------------------
    std::vector<Dolphin::Burst>
    Dolphin::generateBursts(int numSamples, float fs, uint32_t seed)
    {
        std::vector<Burst> bursts;
        if (p_.burstRate <= 0.0f) return bursts;

        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> uniform01(0.0f, 1.0f);
        std::uniform_real_distribution<float> phaseDist(0.0f, TWO_PI_F);

        const float durationSec = static_cast<float>(numSamples) / fs;
        const float meanPeriod = 1.0f / p_.burstRate;
        const float jitterRange = 0.4f * meanPeriod;
        std::uniform_real_distribution<float> jitterDist(-jitterRange, +jitterRange);

        float t = 0.0f;
        while (t < durationSec)
        {
            Burst b;
            b.sampleIndex = static_cast<int>(t * fs);
            if (b.sampleIndex >= numSamples) break;

            const float durSec = p_.burstDurationMin +
                uniform01(rng) * (p_.burstDurationMax - p_.burstDurationMin);
            b.lengthSamples = static_cast<int>(durSec * fs);
            if (b.sampleIndex + b.lengthSamples > numSamples)
            {
                b.lengthSamples = numSamples - b.sampleIndex;
            }
            if (b.lengthSamples <= 0) break;

            b.clickIntervalSamples = p_.burstClickIntervalUs * 1e-6f * fs;
            b.amplitude = 1.0f;
            b.phase = phaseDist(rng);

            bursts.push_back(b);

            t += meanPeriod + jitterDist(rng);
            if (t < 0.0f) t = 0.0f;
        }

        return bursts;
    }

    // ------------------------------------------------------------
    //  Рендер burst (жужжание).
    //
    //  Внутри burst генерируются короткие импульсы (щелчки)
    //  через clickIntervalSamples. Каждый импульс — короткий,
    //  заполнен белым шумом (как отдельный щелчок, но с меньшей
    //  длительностью).
    // ------------------------------------------------------------
    void Dolphin::renderBurst(std::vector<float>& buffer,
                              int numSamples,
                              float fs,
                              const Burst& b)
    {
        if (b.lengthSamples <= 0) return;

        const float durSec = p_.burstClickIntervalUs * 1e-6f;
        const int clickLen = static_cast<int>(durSec * fs * 0.5f) + 1;
        if (clickLen <= 0) return;

        const float tau = durSec / 4.0f;
        const float invTau2 = 1.0f / (tau * tau);
        const float invFs = 1.0f / fs;

        std::mt19937 localRng(static_cast<uint32_t>(b.sampleIndex) ^ 0x85EBCA6Bu);
        std::normal_distribution<float> localGauss(0.0f, 1.0f);

        // Идём по burst с шагом clickIntervalSamples.
        float pos = 0.0f;
        while (static_cast<int>(pos) < b.lengthSamples)
        {
            const int clickStart = b.sampleIndex + static_cast<int>(pos);
            for (int n = 0; n < clickLen; ++n)
            {
                const int idx = clickStart + n;
                if (idx < 0 || idx >= numSamples) break;
                // Ограничиваем внутри burst.
                if (idx >= b.sampleIndex + b.lengthSamples) break;

                const float tLocal = static_cast<float>(n) * invFs;
                const float env = std::exp(-(tLocal * tLocal) * invTau2);
                const float white = localGauss(localRng);

                buffer[idx] += b.amplitude * env * white;
            }

            pos += b.clickIntervalSamples;
        }
    }

    // ------------------------------------------------------------
    //  Основной generate для Dolphin.
    //
    //  Шаги:
    //    1. Генерируем события (свисты, щелчки, burst).
    //    2. Рендерим их в буфер.
    //    3. Добавляем белый шум по SNR.
    // ------------------------------------------------------------
    void Dolphin::generate(float* out,
                           int numSamples,
                           float fs,
                           int64_t sampleOffset)
    {
        // --- 1. Оценка средней мощности сигнала для расчёта SNR ---
        // Грубая оценка: сумма вклада всех трёх компонент.
        // Точная оценка была бы сложной (свисты, щелчки — события),
        // поэтому берём упрощённую: предполагаем, что средняя мощность
        // порядка единицы. Далее SNR настраиваем через порог.
        //
        // Поскольку события разрежены (2-3 свиста в секунду, редкие
        // щелчки), средняя мощность сильно ниже пиковой.
        // Это нормально — дельфин звучит как «импульсный» источник.

        // --- 2. Сброс RNG и генерация событий ---
        rng_.seed(p_.seed);

        // Отдельные под-seed'ы для каждой компоненты,
        // чтобы изменения одной не сдвигали другую.
        std::vector<Whistle> whistles = generateWhistles(numSamples, fs, p_.seed + 11u);
        std::vector<Click> clicks = generateClicks(numSamples, fs, p_.seed + 22u);
        std::vector<Burst> bursts = generateBursts(numSamples, fs, p_.seed + 33u);

        // --- 3. Буфер для накопления сигнала ---
        std::vector<float> buffer(numSamples, 0.0f);

        // --- 4. Рендер событий ---
        for (const auto& w : whistles) renderWhistle(buffer, numSamples, fs, w);
        for (const auto& c : clicks) renderClick(buffer, numSamples, fs, c);
        for (const auto& b : bursts) renderBurst(buffer, numSamples, fs, b);

        // --- 5. Расчёт RMS сигнала (для SNR) ---
        float sumSq = 0.0f;
        for (int n = 0; n < numSamples; ++n) sumSq += buffer[n] * buffer[n];
        const float rmsSignal = std::sqrt(sumSq / static_cast<float>(std::max(1, numSamples)));

        // --- 6. Амплитуда белого шума по SNR ---
        float noiseAmp = 0.0f;
        if (p_.snrDb <= 100.0f)
        {
            if (p_.snrDb < -40.0f)
            {
                noiseAmp = rmsSignal * 1e4f;
            }
            else
            {
                noiseAmp = rmsSignal * std::pow(10.0f, -p_.snrDb / 20.0f);
            }
        }

        // --- 7. Запись в out: сигнал + шум ---
        for (int n = 0; n < numSamples; ++n)
        {
            const float wgn = gauss_(rng_) * noiseAmp;
            out[n] = buffer[n] + wgn;
        }

        // sampleOffset в этой модели не используется: события
        // генерируются в локальных координатах от 0 до numSamples.
        // Это допустимо, потому что дельфин всегда генерируется
        // целиком на весь буфер сцены. Если понадобится генерация
        // по кускам — sampleOffset нужно будет учесть.
        (void)sampleOffset;
    }
} // namespace hydro
