#include "tracks.h"
#include "fft.h"

#include <cmath>
#include <iomanip>
#include <vector>
#include <algorithm>

namespace sd
{
    namespace
    {
        constexpr float PI_F = 3.14159265358979323846f;

        // === НАСТРОЙКИ (v4.1) ===
        constexpr double MIN_TRACK_DUR_MS = 80.0;
        constexpr double MERGE_GAP_MS = 60.0; // слияние треков
        constexpr float MERGE_F0_TOL = 0.12f;
        constexpr double HARM_MIN_DUR_MS = 150.0;

        constexpr float WHISTLE_MAX_F0_HZ = 6000.0f;

        // Группировка в фразы (v4.2 — середина).
        constexpr double PHRASE_GAP_MS = 60.0; // разрыв внутри фразы
        constexpr float HARMONIC_TOL = 0.15f; // допуск по кратности
        constexpr float CONTINUE_F0_LOW = 0.85f; // F0_B >= 0.85*F0_end_A
        constexpr float CONTINUE_F0_HIGH = 1.15f; // F0_B <= 1.15*F0_end_A
        constexpr double MIN_PHRASE_DUR_MS = 100.0;

        struct Peak
        {
            int bin;
            float freq;
            float mag;
            float ampDb;
        };

        struct TrackPoint
        {
            double tMs;
            float freq;
            float ampDb;
        };

        struct Track
        {
            int id = -1;
            std::vector<TrackPoint> points;
            double startMs = 0.0;
            double endMs = 0.0;
            float f0Start = 0.0f;
            float f0End = 0.0f;
            float f0Mean = 0.0f;
            float ampMeanDb = 0.0f;
            float harmonicity = 0.0f;
            float peakAmpDb = -100.0f;
            const char* type = "UNKNOWN";
        };

        struct Phrase
        {
            int id = 0;
            double startMs = 0.0;
            double endMs = 0.0;
            float f0Start = 0.0f;
            float f0End = 0.0f;
            float f0Min = 0.0f;
            float f0Max = 0.0f;
            float ampDb = 0.0f;
            float harmonicity = 0.0f;
            int numHarmonics = 0;
            int numTracks = 0;
            const char* type = "WHISTLE";
        };

        // Глобальный максимум магнитуды по всему файлу —
        // нужен для нормировки дБ. Устанавливается один раз
        // в writeScore, передаётся сюда.
        float g_globalMaxMag = 1.0f;

        std::vector<Peak> extractPeaks(const std::vector<float>& mag, int sr, int fftSize, float floorDb)
        {
            std::vector<Peak> peaks;
            const int half = fftSize / 2;
            const float binHz = float(sr) / float(fftSize);
            if (g_globalMaxMag < 1e-9f) return peaks;

            for (int k = 2; k < half - 1; ++k)
            {
                if (mag[k] > mag[k - 1] && mag[k] > mag[k + 1])
                {
                    const float db = 20.0f * std::log10(
                        mag[k] / g_globalMaxMag + 1e-9f);
                    if (db < floorDb) continue;
                    Peak p;
                    p.bin = k;
                    p.freq = float(k) * binHz;
                    p.mag = mag[k];
                    p.ampDb = db;
                    peaks.push_back(p);
                }
            }
            return peaks;
        }

        int findBestTrack(const Peak& p, const std::vector<Track>& tracks,
                          const std::vector<bool>& alive,
                          float tolFrac, float tolDb)
        {
            int best = -1;
            float bestScore = 1e9f;
            for (size_t i = 0; i < tracks.size(); ++i)
            {
                if (!alive[i]) continue;
                const auto& last = tracks[i].points.back();
                const float dFrac = std::fabs(p.freq - last.freq)
                    / std::max(100.0f, last.freq);
                if (dFrac > tolFrac) continue;
                const float dDb = std::fabs(p.ampDb - last.ampDb);
                if (dDb > tolDb) continue;
                const float score = dFrac + dDb * 0.01f;
                if (score < bestScore)
                {
                    bestScore = score;
                    best = int(i);
                }
            }
            return best;
        }

        float trackHarmonicity(const Track& tr)
        {
            if (tr.points.size() < 5) return 0.0f;
            float f0 = 0.0f;
            for (const auto& p : tr.points) f0 += p.freq;
            f0 /= float(tr.points.size());
            if (f0 < 50.0f) return 0.0f;
            float devSum = 0.0f;
            for (const auto& p : tr.points)
                devSum += std::fabs(p.freq - f0) / f0;
            float h = 1.0f - (devSum / float(tr.points.size())) * 2.0f;
            if (h < 0.0f) h = 0.0f;
            if (h > 1.0f) h = 1.0f;
            return h;
        }

        std::vector<Track> mergeTracks(const std::vector<Track>& in)
        {
            std::vector<Track> out;
            std::vector<bool> used(in.size(), false);
            std::vector<int> order(in.size());
            for (size_t i = 0; i < in.size(); ++i) order[i] = int(i);
            std::sort(order.begin(), order.end(),
                      [&](int a, int b) { return in[a].startMs < in[b].startMs; });

            for (int idxA : order)
            {
                if (used[idxA]) continue;
                Track merged = in[idxA];
                used[idxA] = true;
                bool changed = true;
                while (changed)
                {
                    changed = false;
                    for (int idxB : order)
                    {
                        if (used[idxB]) continue;
                        const auto& tB = in[idxB];
                        const double gap = tB.startMs - merged.endMs;
                        if (gap < -5.0 || gap > MERGE_GAP_MS) continue;
                        const float fEnd = merged.points.back().freq;
                        const float fStart = tB.points.front().freq;
                        const float dFrac = std::fabs(fEnd - fStart)
                            / std::max(100.0f, fEnd);
                        if (dFrac > MERGE_F0_TOL) continue;
                        for (const auto& p : tB.points) merged.points.push_back(p);
                        merged.endMs = tB.endMs;
                        used[idxB] = true;
                        changed = true;
                        break;
                    }
                }
                out.push_back(std::move(merged));
            }
            return out;
        }

        void finalizeTrack(Track& tr)
        {
            if (tr.points.empty()) return;
            tr.startMs = tr.points.front().tMs;
            tr.endMs = tr.points.back().tMs;
            tr.f0Start = tr.points.front().freq;
            tr.f0End = tr.points.back().freq;

            float sumF = 0.0f, sumA = 0.0f;
            for (const auto& p : tr.points)
            {
                sumF += p.freq;
                sumA += p.ampDb;
            }
            tr.f0Mean = sumF / float(tr.points.size());
            tr.ampMeanDb = sumA / float(tr.points.size());
            tr.peakAmpDb = -100.0f;
            for (const auto& p : tr.points)
                if (p.ampDb > tr.peakAmpDb) tr.peakAmpDb = p.ampDb;

            const double dur = tr.endMs - tr.startMs;
            if (dur >= HARM_MIN_DUR_MS) tr.harmonicity = trackHarmonicity(tr);
            else tr.harmonicity = -1.0f;

            const float fMax = std::max(tr.f0Start, tr.f0End);
            const float fMin = std::min(tr.f0Start, tr.f0End);

            // Жёсткие диапазоны:
            //  <500 Гц        — низкочастотный шум
            //  500–6000 Гц    — зона свиста
            //  >6000 Гц       — стрёкот
            if (fMin > 6000.0f)
            {
                tr.type = "STR";
            }
            else if (fMax < 500.0f)
            {
                tr.type = "NOISE";
            }
            else
            {
                // В зоне свиста. Уточняем тип.
                if (tr.harmonicity >= 0.0f && tr.harmonicity < 0.20f
                    && dur > 200.0)
                    tr.type = "NOISE";
                else if (fMax - fMin > 0.05f * fMax)
                    tr.type = "GLISS";
                else
                    tr.type = "TON";
            }
        }

        // ------------------------------------------------------------
        //  Группировка в фразы (агрессивная, v4.1).
        //
        //  Трек B присоединяется к фразе, если:
        //    (A) F0_B кратна F0_A (0.5x, 1x, 2x, 3x) с допуском HARMONIC_TOL
        //        — это гармоника;
        //    ИЛИ
        //    (B) B начинается в пределах PHRASE_GAP_MS от конца фразы,
        //        и F0_B лежит в [0.7, 1.4] × F0_конец_фразы
        //        — это продолжение того же свиста.
        // ------------------------------------------------------------
        std::vector<Phrase> groupIntoPhrases(const std::vector<Track>& tracks)
        {
            std::vector<Phrase> phrases;
            std::vector<bool> used(tracks.size(), false);

            std::vector<int> order(tracks.size());
            for (size_t i = 0; i < tracks.size(); ++i) order[i] = int(i);
            std::sort(order.begin(), order.end(),
                      [&](int a, int b) { return tracks[a].startMs < tracks[b].startMs; });

            for (int idxA : order)
            {
                if (used[idxA]) continue;
                used[idxA] = true;

                Phrase ph;
                ph.id = int(phrases.size()) + 1;
                ph.startMs = tracks[idxA].startMs;
                ph.endMs = tracks[idxA].endMs;
                ph.f0Start = tracks[idxA].f0Start;
                ph.f0End = tracks[idxA].f0End;
                ph.f0Min = std::min(tracks[idxA].f0Start, tracks[idxA].f0End);
                ph.f0Max = std::max(tracks[idxA].f0Start, tracks[idxA].f0End);
                ph.ampDb = tracks[idxA].peakAmpDb;
                ph.numHarmonics = 1;
                ph.numTracks = 1;
                ph.type = tracks[idxA].type;

                // Накопители гармоничности: усредняем по трекам с известной h.
                float harmSum = 0.0f;
                int harmCnt = 0;
                if (tracks[idxA].harmonicity >= 0.0f)
                {
                    harmSum += tracks[idxA].harmonicity;
                    ++harmCnt;
                }

                // Лимиты: фраза не длиннее MAX_PHRASE_DUR_MS,
                // не больше MAX_PHRASE_TRACKS треков,
                // суммарный дрейф F0 не больше MAX_F0_DRIFT.
                constexpr double MAX_PHRASE_DUR_MS = 1200.0;
                constexpr int MAX_PHRASE_TRACKS = 8;
                constexpr float MAX_F0_DRIFT = 0.80f; // ±80%

                const float f0Anchor = tracks[idxA].f0Start; // якорь F0

                bool changed = true;
                while (changed)
                {
                    changed = false;
                    for (int idxB : order)
                    {
                        if (used[idxB]) continue;
                        if (ph.numTracks >= MAX_PHRASE_TRACKS) break;

                        const auto& tB = tracks[idxB];

                        // Лимит по времени.
                        const double newEnd = std::max(ph.endMs, tB.endMs);
                        const double newStart = std::min(ph.startMs, tB.startMs);
                        if (newEnd - newStart > MAX_PHRASE_DUR_MS) continue;

                        // Лимит по дрейфу F0 от якоря.
                        const float f0Min = std::min(ph.f0Min,
                                                     std::min(tB.f0Start, tB.f0End));
                        const float f0Max = std::max(ph.f0Max,
                                                     std::max(tB.f0Start, tB.f0End));
                        const float drift = std::max(
                            std::fabs(f0Min - f0Anchor) / f0Anchor,
                            std::fabs(f0Max - f0Anchor) / f0Anchor);
                        if (drift > MAX_F0_DRIFT) continue;

                        // --- Условие (A): гармоника ---
                        bool isHarmonic = false;
                        {
                            const double overlapStart = std::max(ph.startMs, tB.startMs);
                            const double overlapEnd = std::min(ph.endMs, tB.endMs);
                            const double overlapMs = overlapEnd - overlapStart;
                            if (overlapMs > 30.0)
                            {
                                const float f0A = 0.5f * (ph.f0Min + ph.f0Max);
                                const float f0B = 0.5f * (tB.f0Start + tB.f0End);
                                const float ratio = f0B / std::max(50.0f, f0A);
                                const float nearest = std::round(ratio);
                                if (nearest >= 2.0f && nearest <= 4.0f)
                                {
                                    const float err = std::fabs(ratio - nearest);
                                    if (err <= HARMONIC_TOL) isHarmonic = true;
                                }
                            }
                        }

                        // --- Условие (B): продолжение ---
                        bool isContinuation = false;
                        {
                            const double gap = tB.startMs - ph.endMs;
                            if (gap >= -5.0 && gap <= PHRASE_GAP_MS)
                            {
                                const float f0EndA = ph.f0End;
                                const float f0StartB = tB.f0Start;
                                const float ratio = f0StartB / std::max(50.0f, f0EndA);
                                if (ratio >= CONTINUE_F0_LOW && ratio <= CONTINUE_F0_HIGH)
                                    isContinuation = true;
                            }
                        }

                        if (!isHarmonic && !isContinuation) continue;

                        // Сливаем.
                        if (tB.startMs < ph.startMs) ph.startMs = tB.startMs;
                        if (tB.endMs > ph.endMs) ph.endMs = tB.endMs;
                        ph.f0Min = std::min(ph.f0Min, std::min(tB.f0Start, tB.f0End));
                        ph.f0Max = std::max(ph.f0Max, std::max(tB.f0Start, tB.f0End));
                        if (tB.peakAmpDb > ph.ampDb) ph.ampDb = tB.peakAmpDb;

                        if (isContinuation && !isHarmonic)
                            ph.f0End = tB.f0End;

                        if (isHarmonic) ++ph.numHarmonics;
                        ++ph.numTracks;
                        if (tB.harmonicity >= 0.0f)
                        {
                            harmSum += tB.harmonicity;
                            ++harmCnt;
                        }
                        used[idxB] = true;
                        changed = true;
                    }
                }
            }

            // Убираем слишком короткие фразы.
            std::vector<Phrase> kept;
            for (auto& p : phrases)
            {
                if (p.endMs - p.startMs < MIN_PHRASE_DUR_MS) continue;
                kept.push_back(std::move(p));
            }
            return kept;
        }
    }

    void writeScore(std::ostream& os, const WavData& wav)
    {
        const int sr = wav.sampleRate;
        const int64_t N = int64_t(wav.samples.size());
        const int win = 1024;
        const int hop = std::max(1, sr / 100);

        if (N < win || sr <= 0)
        {
            os << "# file too short or sr=0\n";
            return;
        }

        os << "# sound_digit v4.1 — партитура (агрессивная группировка)\n";
        os << "# sr=" << sr << ", duration=" << (N * 1000 / sr) << " ms\n\n";

        std::vector<std::complex<float>> buf(win);
        std::vector<float> seg(win);
        std::vector<Track> tracks;
        std::vector<bool> alive;
        std::vector<std::pair<double, double>> silences;

        const float silenceThresholdDb = -50.0f;
        bool inSilence = false;
        double silenceStart = 0.0;

        // === Глобальная нормировка: пройти по всему файлу и найти
        //     максимальную магнитуду. Используется в extractPeaks. ===
        {
            float maxMag = 0.0f;
            std::vector<std::complex<float>> bufTmp(win);
            std::vector<float> segTmp(win);
            for (int64_t s = 0; s + win <= N; s += hop)
            {
                for (int i = 0; i < win; ++i) segTmp[i] = wav.samples[s + i];
                applyHann(segTmp);
                for (int i = 0; i < win; ++i)
                    bufTmp[i] = std::complex<float>(segTmp[i], 0.0f);
                fft(bufTmp);
                const int half = win / 2;
                for (int k = 2; k < half; ++k)
                {
                    const float m = std::abs(bufTmp[k]) / (win * 0.5f);
                    if (m > maxMag) maxMag = m;
                }
            }
            g_globalMaxMag = (maxMag > 1e-9f) ? maxMag : 1.0f;
        }

        for (int64_t s = 0; s + win <= N; s += hop)
        {
            for (int i = 0; i < win; ++i) seg[i] = wav.samples[s + i];
            applyHann(seg);
            for (int i = 0; i < win; ++i)
                buf[i] = std::complex<float>(seg[i], 0.0f);
            fft(buf);

            const int half = win / 2;
            std::vector<float> mag(half);
            for (int k = 0; k < half; ++k)
                mag[k] = std::abs(buf[k]) / (win * 0.5f);

            double eTot = 0.0;
            for (int k = 2; k < half; ++k) eTot += double(mag[k]) * mag[k];
            const float levelDb = (eTot > 1e-20)
                                      ? float(10.0 * std::log10(eTot))
                                      : -100.0f;
            const double tMs = double(s) / double(sr) * 1000.0;

            if (levelDb < silenceThresholdDb)
            {
                if (!inSilence)
                {
                    inSilence = true;
                    silenceStart = tMs;
                }
                for (size_t i = 0; i < tracks.size(); ++i) alive[i] = false;
                continue;
            }
            else if (inSilence)
            {
                silences.emplace_back(silenceStart, tMs);
                inSilence = false;
            }

            auto peaks = extractPeaks(mag, sr, win, -60.0f);
            std::vector<bool> matched(tracks.size(), false);
            for (auto& p : peaks)
            {
                const int id = findBestTrack(p, tracks, alive, 0.08f, 12.0f);
                if (id >= 0)
                {
                    tracks[id].points.push_back({tMs, p.freq, p.ampDb});
                    tracks[id].endMs = tMs;
                    matched[id] = true;
                }
                else
                {
                    Track tr;
                    tr.id = int(tracks.size());
                    tr.points.push_back({tMs, p.freq, p.ampDb});
                    tr.startMs = tMs;
                    tr.endMs = tMs;
                    tracks.push_back(std::move(tr));
                    alive.push_back(true);
                    matched.push_back(true);
                }
            }
            for (size_t i = 0; i < tracks.size(); ++i)
                if (alive[i] && !matched[i]) alive[i] = false;
        }

        if (inSilence)
            silences.emplace_back(silenceStart, double(N) / double(sr) * 1000.0);

        std::vector<Track> filtered;
        for (auto& tr : tracks)
        {
            if (tr.endMs - tr.startMs < MIN_TRACK_DUR_MS) continue;
            filtered.push_back(std::move(tr));
        }

        auto merged = mergeTracks(filtered);
        for (auto& tr : merged) finalizeTrack(tr);
        std::sort(merged.begin(), merged.end(),
                  [](const Track& a, const Track& b) { return a.startMs < b.startMs; });

        auto phrases = groupIntoPhrases(merged);
        std::sort(phrases.begin(), phrases.end(),
                  [](const Phrase& a, const Phrase& b) { return a.startMs < b.startMs; });

        os << "# ОБЩАЯ КАРТИНА\n";
        os << "# фраз: " << phrases.size() << "\n";
        os << "# пауз: " << silences.size() << "\n\n";

        os << "# ФРАЗЫ (партитура)\n";
        os << "#   #  начало(ms)  конец(ms)  дл(ms)  F0нач  F0кон  F0мин  F0макс  ампл(дБ)  гарм  nHarm  тип\n";
        int idx = 1;
        for (const auto& p : phrases)
        {
            os << std::setw(5) << idx++ << "  "
                << std::setw(9) << std::fixed << std::setprecision(0) << p.startMs << "  "
                << std::setw(8) << p.endMs << "  "
                << std::setw(6) << (p.endMs - p.startMs) << "  "
                << std::setw(5) << p.f0Start << "  "
                << std::setw(5) << p.f0End << "  "
                << std::setw(5) << p.f0Min << "  "
                << std::setw(6) << p.f0Max << "  "
                << std::setw(8) << std::setprecision(1) << p.ampDb << "  ";
            if (p.harmonicity < 0.0f) os << "  —  ";
            else os << std::setw(4) << std::setprecision(2) << p.harmonicity << "  ";
            os << std::setw(4) << p.numHarmonics << "  " << p.type << "\n";
        }
        os << "\n";

        os << "# ПАУЗЫ\n";
        os << "#   #  начало(ms)  конец(ms)  дл(ms)\n";
        idx = 1;
        for (const auto& p : silences)
        {
            os << std::setw(5) << idx++ << "  "
                << std::setw(9) << std::fixed << std::setprecision(0) << p.first << "  "
                << std::setw(8) << p.second << "  "
                << std::setw(6) << (p.second - p.first) << "\n";
        }
        os << "\n";
    }
}
