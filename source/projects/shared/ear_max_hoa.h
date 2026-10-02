/// @file
/// @brief   Real spherical harmonics in the conventions of the EAR / ITU-R
///          BS.2076 (ACN channel order; N3D, SN3D and FuMa normalisation;
///          ADM azimuth anticlockwise, elevation up), as used by the EAR's
///          HOA decoder design. libear has the same functions but does not
///          export them, so they are reimplemented here for the encoder.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include <algorithm>
#include <cmath>
#include <string>

namespace earmax::hoa {

constexpr int k_max_order = 8;

/// Ambisonics Channel Number of order n and degree m.
inline int to_acn(int n, int m)
{
    return n * n + n + m;
}

/// Order n and degree m of an Ambisonics Channel Number.
inline void from_acn(int acn, int& n, int& m)
{
    n = static_cast<int>(std::floor(std::sqrt(static_cast<double>(std::max(0, acn)))));
    m = acn - n * n - n;
}

/// Number of components of a full ambisonic set of the given order.
inline size_t component_count(int order)
{
    return static_cast<size_t>((order + 1) * (order + 1));
}

inline double factorial(int n)
{
    double f = 1.0;
    for (int i = 2; i <= n; ++i) {
        f *= i;
    }
    return f;
}

/// Associated Legendre function P_n^m(x) for m >= 0, omitting the (-1)^m
/// Condon-Shortley phase (as the EAR does).
inline double associated_legendre(int n, int m, double x)
{
    double pmm = 1.0;
    if (m > 0) {
        const double somx2 = std::sqrt(std::max(0.0, 1.0 - x * x));
        double odd = 1.0;
        for (int i = 1; i <= m; ++i) {
            pmm *= odd * somx2;
            odd += 2.0;
        }
    }
    if (n == m) {
        return pmm;
    }
    double pmmp1 = x * (2.0 * m + 1.0) * pmm;
    if (n == m + 1) {
        return pmmp1;
    }
    double pnm = 0.0;
    for (int l = m + 2; l <= n; ++l) {
        pnm = ((2.0 * l - 1.0) * x * pmmp1 - (l + m - 1.0) * pmm) / static_cast<double>(l - m);
        pmm = pmmp1;
        pmmp1 = pnm;
    }
    return pnm;
}

enum class normalization { SN3D, N3D, FuMa };

/// Parse a normalisation name; returns false for an unknown name.
inline bool normalization_from_name(const std::string& name, normalization& out)
{
    if (name == "SN3D") {
        out = normalization::SN3D;
    }
    else if (name == "N3D") {
        out = normalization::N3D;
    }
    else if (name == "FuMa") {
        out = normalization::FuMa;
    }
    else {
        return false;
    }
    return true;
}

inline const char* normalization_name(normalization norm)
{
    switch (norm) {
        case normalization::N3D: return "N3D";
        case normalization::FuMa: return "FuMa";
        default: return "SN3D";
    }
}

/// Highest order a normalisation is defined for (FuMa stops at 3).
inline int max_order(normalization norm)
{
    return norm == normalization::FuMa ? 3 : k_max_order;
}

inline double norm_sn3d(int n, int abs_m)
{
    return std::sqrt(factorial(n - abs_m) / factorial(n + abs_m));
}

inline double norm_n3d(int n, int abs_m)
{
    return std::sqrt((2.0 * n + 1.0) * factorial(n - abs_m) / factorial(n + abs_m));
}

/// FuMa: SN3D scaled by the BS.2076 conversion factors, defined up to order 3.
inline double norm_fuma(int n, int abs_m)
{
    double factor = 1.0;
    if (n == 0) {
        factor = 1.0 / std::sqrt(2.0);
    }
    else if (n == 2 && abs_m >= 1) {
        factor = 2.0 / std::sqrt(3.0);
    }
    else if (n == 3 && abs_m == 1) {
        factor = std::sqrt(45.0 / 32.0);
    }
    else if (n == 3 && abs_m == 2) {
        factor = 3.0 / std::sqrt(5.0);
    }
    else if (n == 3 && abs_m == 3) {
        factor = std::sqrt(8.0 / 5.0);
    }
    return norm_sn3d(n, abs_m) * factor;
}

inline double norm(normalization kind, int n, int abs_m)
{
    switch (kind) {
        case normalization::N3D: return norm_n3d(n, abs_m);
        case normalization::FuMa: return norm_fuma(n, abs_m);
        default: return norm_sn3d(n, abs_m);
    }
}

/// Real spherical harmonic Y_n^m for a direction given as ADM azimuth and
/// elevation in radians (azimuth anticlockwise from the front, elevation
/// up from the horizontal plane), BS.2076 section 10.1.
inline double sph_harm(int n, int m, double azimuth, double elevation, normalization kind)
{
    double scale = 1.0;
    if (m > 0) {
        scale = std::sqrt(2.0) * std::cos(m * azimuth);
    }
    else if (m < 0) {
        scale = -std::sqrt(2.0) * std::sin(m * azimuth);
    }
    return norm(kind, n, std::abs(m)) * associated_legendre(n, std::abs(m), std::sin(elevation)) * scale;
}

} // namespace earmax::hoa
