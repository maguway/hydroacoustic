#pragma once
#include "types.h"
#include <array>
#include <complex>
#include <vector>

namespace hydro {

// ============================================================
//  Геометрия круговой решётки (UCA) и steering vector.
//
//  Элементы расположены равномерно на окружности радиуса R
//  в горизонтальной плоскости.
//
//  Углы элементов:
//      phi_i = 2*pi*i / N,  i = 0..N-1
//
//  Направление прихода (азимут) отсчитывается от оси +X
//  против часовой стрелки (стандартное соглашение).
//
//  ЗНАК ЗАДЕРЖКИ — ДВА РАЗНЫХ СОГЛАШЕНИЯ:
//
//  1) delays() — для BEAMFORMING (MUSIC/Capon/EV/DAS):
//        tau_i = +R * cos(theta - phi_i) / c
//     Положительное tau — сигнал приходит к элементу ПОЗЖЕ центра.
//     Если источник в направлении theta, то элемент ch_i,
//     ближайший к theta, получает tau с МАКСИМУМОМ (положительным),
//     а противоположный — с минимумом (отрицательным).
//     Steering vector a_i = exp(-j*2*pi*f*tau_i) КОМПЕНСИРУЕТ эту
//     задержку. Используется в алгоритмах DOA.
//
//  2) arrivalOffsets() — для ГЕНЕРАЦИИ СИГНАЛА:
//        dt_i = -R * cos(theta - phi_i) / c
//     Физически: если источник в направлении theta, сигнал приходит
//     к элементу, ближайшему к theta, РАНЬШЕ, чем к центру.
//     Значит, dt_i для него ОТРИЦАТЕЛЬНО. Противоположный элемент
//     получает ПОЛОЖИТЕЛЬНОЕ dt_i (позже).
//     Так s_i(t) = s_source(t - dt_i) даёт корректные фазы.
//
//  ВАЖНО: не перепутать. Для генерации — arrivalOffsets(),
//         для DOA — delays().
// ============================================================

class UcaGeometry {
public:
    UcaGeometry(int numElements = NUM_CHANNELS,
                float radiusMeters = UCA_RADIUS_M);

    int   numElements() const { return n_; }
    float radius()      const { return R_; }

    // Угол i-го элемента (радианы, 0..2*pi)
    float elementAngle(int i) const;

    // Позиция i-го элемента в декартовых координатах (x, y)
    void  elementPosition(int i, float& x, float& y) const;

    // === 1) BEAMFORMING-соглашение ===
    //   tau_i = +R*cos(theta - phi_i) / c
    // Используется в steering vector и DOA.
    std::vector<float> delays(float thetaDeg, float c) const;

    // Steering vector:
    //   a_i = exp(-j * 2*pi*f * tau_i)
    std::vector<std::complex<float>>
        steeringVector(float freqHz, float thetaDeg, float c) const;

    // === 2) ГЕНЕРАЦИЯ-соглашение ===
    //   dt_i = -R*cos(theta - phi_i) / c
    // Используется для задержки сигнала источника на i-м элементе:
    //   s_i(t) = s_source(t - dt_i)
    std::vector<float> arrivalOffsets(float thetaDeg, float c) const;

    // Ближайший элемент к направлению thetaDeg (для визуализации/отладки)
    int nearestElement(float thetaDeg) const;

private:
    int   n_;
    float R_;
    std::vector<float> phi_;   // углы элементов, радианы
};

} // namespace hydro