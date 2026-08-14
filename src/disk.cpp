#include "disk.h"

#include <algorithm>
#include <cmath>

namespace {

// Piecewise-Gaussian fits to the CIE 1931 colour matching functions
// (Wyman, Sloan & Shirley, JCGT 2013) -- accurate to well under a perceptual JND,
// and far cheaper than carrying the tabulated curves.
double gauss(double x, double alpha, double mu, double s_low, double s_high) {
    const double t = (x - mu) / (x < mu ? s_low : s_high);
    return alpha * std::exp(-0.5 * t * t);
}

double cie_x(double nm) {
    return gauss(nm, 1.056, 599.8, 37.9, 31.0) +
           gauss(nm, 0.362, 442.0, 16.0, 26.7) +
           gauss(nm, -0.065, 501.1, 20.4, 26.2);
}

double cie_y(double nm) {
    return gauss(nm, 0.821, 568.8, 46.9, 40.5) +
           gauss(nm, 0.286, 530.9, 16.3, 31.1);
}

double cie_z(double nm) {
    return gauss(nm, 1.217, 437.0, 11.8, 36.0) +
           gauss(nm, 0.681, 459.0, 26.0, 13.8);
}

// Planck spectral radiance. expm1 keeps the Rayleigh-Jeans tail accurate, where
// exp(x) - 1 would lose most of its significant digits to cancellation.
double planck(double nm, double T) {
    constexpr double h  = 6.62607015e-34;
    constexpr double c  = 2.99792458e8;
    constexpr double kB = 1.380649e-23;

    const double wl = nm * 1e-9;
    return (2.0 * h * c * c) /
           (std::pow(wl, 5.0) * std::expm1(h * c / (wl * kB * T)));
}

// Page-Thorne / Novikov-Thorne relativistic thin-disk flux, for Schwarzschild with a
// no-torque inner boundary at the ISCO. In terms of x = sqrt(r/M):
//
//   F(r) ~ [ x - sqrt6 - (sqrt3/2) * ln( (x-sqrt3)/(x+sqrt3) * (sqrt2+1)^2 ) ]
//          / ( x^5 * (x^2 - 3) )
//
// This is the Newtonian Shakura-Sunyaev flux times the relativistic correction R(x),
// already multiplied out. Written that way on purpose: R(x) carries a 1/(x - sqrt6)
// that cancels against the (1 - sqrt(r_isco/r)) factor of the Newtonian flux, so the
// combined form has no 0/0 at the ISCO and no catastrophic cancellation near it.
// The constant inside the log is (sqrt6+sqrt3)/(sqrt6-sqrt3), rationalized.
//
// Unlike the Newtonian profile this is *not* self-similar in r/r_inner -- the sqrt3
// and sqrt6 are the Schwarzschild ISCO baked in, so it assumes r_inner = 6M.
double flux_shape(double r) {
    constexpr double SQRT3 = 1.7320508075688772;
    constexpr double SQRT6 = 2.4494897427831781;
    constexpr double K     = 5.8284271247461903;  // (sqrt(2) + 1)^2

    const double x = std::sqrt(r);
    if (x <= SQRT6) return 0.0;

    const double lg = std::log((x - SQRT3) / (x + SQRT3) * K);

    return (x - SQRT6 - 0.5 * SQRT3 * lg) / (x * x * x * x * x * (x * x - 3.0));
}

// The relativistic profile's maximum has no tidy closed form (the Newtonian one peaks
// at (49/36) r_isco = 8.17M; this one sits near 9.4M). Scan for it once.
double flux_peak() {
    static const double value = [] {
        double best = 0.0;
        for (int i = 0; i <= 200000; ++i) {
            best = std::max(best, flux_shape(6.0 + i * (200.0 - 6.0) / 200000.0));
        }
        return best;
    }();
    return value;
}

}  // namespace

double disk_temperature(const Disk& disk, double r) {
    if (r < disk.r_inner || r > disk.r_outer) return 0.0;

    // T = (F / sigma)^(1/4); the constants fold into the t_peak normalization.
    return disk.t_peak * std::pow(flux_shape(r) / flux_peak(), 0.25);
}

Vec3 blackbody_rgb(double T) {
    if (T <= 0.0) return Vec3(0.0, 0.0, 0.0);

    double X = 0.0, Y = 0.0, Z = 0.0;
    for (double nm = 380.0; nm <= 780.0; nm += 5.0) {
        const double I = planck(nm, T);
        X += I * cie_x(nm);
        Y += I * cie_y(nm);
        Z += I * cie_z(nm);
    }
    if (Y <= 0.0) return Vec3(0.0, 0.0, 0.0);

    // Normalize to unit luminance so that T^4 alone controls brightness.
    X /= Y;
    Z /= Y;
    Y = 1.0;

    // CIE XYZ -> linear sRGB (sRGB primaries, D65 white point).
    const Vec3 rgb(3.2406 * X - 1.5372 * Y - 0.4986 * Z,
                   -0.9689 * X + 1.8758 * Y + 0.0415 * Z,
                   0.0557 * X - 0.2040 * Y + 1.0570 * Z);

    // Blackbodies at the extremes fall outside the sRGB gamut; clip to the edge.
    return Vec3(std::max(0.0, rgb.x), std::max(0.0, rgb.y), std::max(0.0, rgb.z));
}
