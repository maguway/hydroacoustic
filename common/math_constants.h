#pragma once

// ============================================================
//  Математические константы.
//
//  M_PI и M_TWO_PI в стандартном C++ не определены, а в GCC/MinGW
//  доступны только как POSIX-расширение (и то не всегда).
//  Поэтому определяем явно, чтобы не зависеть от платформы.
//
//  В коде используем либо макросы M_PI / M_TWO_PI (для совместимости
//  с C-кодом, если придётся), либо constexpr PI_F / TWO_PI_F
//  (для чистого C++).
// ============================================================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_TWO_PI
#define M_TWO_PI 6.28318530717958647692
#endif

namespace hydro {

    // float-версии
    constexpr float PI_F     = 3.14159265358979323846f;
    constexpr float TWO_PI_F = 6.28318530717958647692f;

    // double-версии
    constexpr double PI_D     = 3.14159265358979323846;
    constexpr double TWO_PI_D = 6.28318530717958647692;

    // Углы
    inline float  deg2rad(float deg)  { return deg * PI_F / 180.0f; }
    inline float  rad2deg(float rad)  { return rad * 180.0f / PI_F; }
    inline double deg2rad(double deg) { return deg * PI_D / 180.0; }
    inline double rad2deg(double rad) { return rad * 180.0 / PI_D; }

} // namespace hydro