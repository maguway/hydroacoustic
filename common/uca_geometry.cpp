#include "uca_geometry.h"
#include "math_constants.h"
#include <cmath>

namespace hydro {

    UcaGeometry::UcaGeometry(int numElements, float radiusMeters)
        : n_(numElements), R_(radiusMeters), phi_(numElements)
    {
        for (int i = 0; i < n_; ++i) {
            phi_[i] = TWO_PI_F * i / n_;
        }
    }

    float UcaGeometry::elementAngle(int i) const {
        return phi_[i];
    }

    void UcaGeometry::elementPosition(int i, float& x, float& y) const {
        x = R_ * std::cos(phi_[i]);
        y = R_ * std::sin(phi_[i]);
    }

    std::vector<float> UcaGeometry::delays(float thetaDeg, float c) const {
        std::vector<float> tau(n_);
        const float theta = deg2rad(thetaDeg);
        const float inv_c = 1.0f / c;
        for (int i = 0; i < n_; ++i) {
            tau[i] = R_ * std::cos(theta - phi_[i]) * inv_c;
        }
        return tau;
    }

    std::vector<float> UcaGeometry::arrivalOffsets(float thetaDeg, float c) const {
        std::vector<float> dt(n_);
        const float theta = deg2rad(thetaDeg);
        const float inv_c = 1.0f / c;
        for (int i = 0; i < n_; ++i) {
            // МИНУС по сравнению с delays()
            dt[i] = -R_ * std::cos(theta - phi_[i]) * inv_c;
        }
        return dt;
    }

    std::vector<std::complex<float>>
    UcaGeometry::steeringVector(float freqHz, float thetaDeg, float c) const
    {
        std::vector<std::complex<float>> a(n_);
        const auto tau = delays(thetaDeg, c);
        const float w = TWO_PI_F * freqHz;
        for (int i = 0; i < n_; ++i) {
            const float phase = -w * tau[i];
            a[i] = std::complex<float>(std::cos(phase), std::sin(phase));
        }
        return a;
    }

    int UcaGeometry::nearestElement(float thetaDeg) const {
        const float theta = deg2rad(thetaDeg);
        int best = 0;
        float bestD = 1e9f;
        for (int i = 0; i < n_; ++i) {
            float d = std::fabs(theta - phi_[i]);
            while (d > PI_F) d = std::fabs(d - TWO_PI_F);
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

} // namespace hydro