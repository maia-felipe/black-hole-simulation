#include "vec3.h"
#include "ray.h"
#include "camera.h"
#include "image.h"
#include "disk.h"
#include "geodesic.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

constexpr double PI = 3.14159265358979323846;

constexpr double SKY_GAIN = 0.015;  // the disk should dominate the exposure

// Rays per pixel along each axis. The higher-order images near the photon ring are
// arbitrarily thin, so a single sample per pixel aliases badly there.
constexpr int SAMPLES_PER_AXIS = 3;

// Auto-exposure: scale the frame so this percentile of non-black luminance lands on
// EXPOSURE_TARGET before tone mapping. Using a percentile rather than the maximum
// keeps a few extremely beamed pixels at the inner edge from crushing everything else.
constexpr double EXPOSURE_PERCENTILE = 0.995;
constexpr double EXPOSURE_TARGET     = 0.9;

// Background as a function of the *outgoing* photon direction. The white->blue
// gradient carries the overall orientation; the checkerboard makes gravitational
// lensing visible (Phase 4 replaces it with a real celestial-sphere texture).
Vec3 sky_color(const Vec3& direction) {
    const Vec3   d = direction.normalize();
    const double a = 0.5 * (d.y + 1.0);  // [-1,1] -> [0,1]

    const Vec3 base = (1.0 - a) * Vec3(1.0, 1.0, 1.0) + a * Vec3(0.5, 0.7, 1.0);

    const double theta = std::acos(d.y);        // polar angle,   [0, pi]
    const double phi   = std::atan2(d.z, d.x);  // azimuth,       (-pi, pi]
    const int    cell  = static_cast<int>(std::floor(theta * 12.0 / PI)) +
                         static_cast<int>(std::floor(phi * 12.0 / PI));

    return (cell % 2 == 0) ? 0.55 * base : base;
}

Vec3 shade(const PhotonResult& photon, const Disk& disk) {
    switch (photon.fate) {
        case PhotonFate::Captured:
            return Vec3(0.0, 0.0, 0.0);

        case PhotonFate::Escaped:
            return SKY_GAIN * sky_color(photon.direction);

        case PhotonFate::HitDisk: {
            // A blackbody at temperature T seen with redshift factor g is exactly a
            // blackbody at g*T, and its bolometric intensity scales as g^4 (from the
            // invariance of I_nu/nu^3). Since sigma*(gT)^4 = g^4*sigma*T^4, the
            // shifted temperature alone carries both the colour and the brightness:
            // gravitational redshift, Doppler shift and relativistic beaming at once.
            const double t_obs = photon.redshift * disk_temperature(disk, photon.disk_r);
            const double ratio = t_obs / disk.t_peak;

            return (ratio * ratio * ratio * ratio) * blackbody_rgb(t_obs);
        }
    }
    return Vec3(0.0, 0.0, 0.0);
}

double luminance(const Vec3& c) {
    return 0.2126 * c.x + 0.7152 * c.y + 0.0722 * c.z;
}

unsigned char to_byte(double linear) {
    if (linear < 0.0) linear = 0.0;

    const double mapped  = linear / (1.0 + linear);      // Reinhard tone map
    const double encoded = std::pow(mapped, 1.0 / 2.2);  // sRGB-ish gamma

    return static_cast<unsigned char>(std::min(encoded, 1.0) * 255.999);
}

// Correctness check: bisect on the emission angle to find the capture/escape
// boundary, then report its impact parameter. Schwarzschild says this must be
// b_crit = sqrt(27) M ~ 5.196152.
void verify_critical_impact_parameter() {
    constexpr double r0 = 30.0;
    const Vec3       cam(0.0, 0.0, r0);
    const Disk       no_disk{0.0, 0.0, 0.0};

    // psi is measured from the outward radial direction, so psi = pi aims straight
    // at the hole (captured) and psi = pi/2 fires tangentially (escapes).
    double lo = 0.5 * PI;
    double hi = PI;
    for (int i = 0; i < 50; ++i) {
        const double mid = 0.5 * (lo + hi);
        const Vec3   d(std::sin(mid), 0.0, std::cos(mid));

        if (trace_photon(cam, d, no_disk).fate == PhotonFate::Captured) hi = mid;
        else                                                            lo = mid;
    }

    const double psi = 0.5 * (lo + hi);
    const double b   = r0 * std::sin(psi) / std::sqrt(1.0 - 2.0 / r0);

    std::cout << "b_crit (numeric)  = " << b << "   analytic sqrt(27) = "
              << std::sqrt(27.0) << "   rel.err = "
              << std::abs(b - std::sqrt(27.0)) / std::sqrt(27.0) << '\n';
}

}  // namespace

int main() {
    verify_critical_impact_parameter();

    constexpr int    width  = 800;
    constexpr int    height = 450;
    constexpr double aspect = static_cast<double>(width) / height;

    // Schwarzschild ISCO is at r = 6M; outside it the thin disk radiates.
    // t_peak is a free parameter set by M and the accretion rate; this value puts
    // the emission in the visible band.
    // The outer radius matters visually: T ~ r^(-3/4) asymptotically, so the disk
    // only cools from white-hot to red over roughly a factor of five in radius.
    const Disk disk{6.0, 36.0, 10500.0};

    // Nearly edge-on: the classic view, where lensing lifts the far side of the disk
    // over the top of the hole and wraps its underside beneath.
    constexpr double cam_r    = 100.0;
    constexpr double cam_elev = 6.0 * PI / 180.0;

    const Camera cam(Vec3(0.0, cam_r * std::sin(cam_elev), cam_r * std::cos(cam_elev)),
                     Vec3(0.0, 0.0, 0.0),  // look_at: the singularity
                     Vec3(0.0, 1.0, 0.0),  // up: the disk's rotation axis
                     24.0,                 // vertical field of view, degrees
                     aspect);

    std::vector<Vec3> radiance;
    radiance.reserve(static_cast<std::size_t>(width) * height);

    double g_min = 1e30, g_max = -1e30;

    constexpr double inv_samples = 1.0 / (SAMPLES_PER_AXIS * SAMPLES_PER_AXIS);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Vec3 sum(0.0, 0.0, 0.0);

            for (int sy = 0; sy < SAMPLES_PER_AXIS; ++sy) {
                for (int sx = 0; sx < SAMPLES_PER_AXIS; ++sx) {
                    // Sample at sub-pixel centres of a regular SxS grid.
                    const double px = x + (sx + 0.5) / SAMPLES_PER_AXIS;
                    const double py = y + (sy + 0.5) / SAMPLES_PER_AXIS;

                    const double s = px / (width - 1);
                    const double t = py / (height - 1);

                    // Image rows run top-down; the viewport's v axis runs bottom-up.
                    const Ray          r      = cam.get_ray(s, 1.0 - t);
                    const PhotonResult photon = trace_photon(r.origin, r.direction, disk);

                    if (photon.fate == PhotonFate::HitDisk) {
                        g_min = std::min(g_min, photon.redshift);
                        g_max = std::max(g_max, photon.redshift);
                    }
                    sum = sum + shade(photon, disk);
                }
            }
            radiance.push_back(inv_samples * sum);
        }
        std::cout << "\rScanlines remaining: " << (height - 1 - y) << "  " << std::flush;
    }

    // Auto-exposure from the luminance distribution of the frame itself.
    std::vector<double> lums;
    lums.reserve(radiance.size());
    for (const Vec3& c : radiance) {
        const double l = luminance(c);
        if (l > 0.0) lums.push_back(l);
    }

    double exposure = 1.0;
    if (!lums.empty()) {
        const std::size_t k =
            static_cast<std::size_t>(EXPOSURE_PERCENTILE * (lums.size() - 1));
        std::nth_element(lums.begin(), lums.begin() + k, lums.end());
        if (lums[k] > 0.0) exposure = EXPOSURE_TARGET / lums[k];
    }

    img framebuffer(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Vec3 c = exposure * radiance[static_cast<std::size_t>(y) * width + x];
            framebuffer.set_pixel(x, y, to_byte(c.x), to_byte(c.y), to_byte(c.z));
        }
    }
    framebuffer.save("output.png");

    std::cout << "\rredshift factor g over the disk: " << g_min << " .. " << g_max
              << "   (1 = unshifted)\n"
              << "auto-exposure scale: " << exposure << '\n'
              << "wrote output.png\n";
}
