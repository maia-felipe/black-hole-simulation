#include "camera.h"
#include "config.h"
#include "disk.h"
#include "geodesic.h"
#include "image.h"
#include "renderer.h"
#include "sky.h"
#include "vec3.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

namespace {

constexpr double PI = 3.14159265358979323846;

// Correctness check: bisect on the emission angle to find the capture/escape
// boundary, then report its impact parameter. Schwarzschild says this must be
// b_crit = sqrt(27) M ~ 5.196152.
void verify_critical_impact_parameter(const TraceQuality& quality) {
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

        if (trace_photon(cam, d, no_disk, quality).fate == PhotonFate::Captured)
            hi = mid;
        else
            lo = mid;
    }

    const double psi = 0.5 * (lo + hi);
    const double b   = r0 * std::sin(psi) / std::sqrt(1.0 - 2.0 / r0);

    std::cout << "b_crit (numeric)  = " << b << "   analytic sqrt(27) = "
              << std::sqrt(27.0) << "   rel.err = "
              << std::abs(b - std::sqrt(27.0)) / std::sqrt(27.0) << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    const std::string config_path = (argc > 1) ? argv[1] : "scene.cfg";

    Config config;
    if (load_config(config_path, config)) {
        std::cout << "config: " << config_path << '\n';
    } else {
        std::cout << "config: " << config_path
                  << " not found, using built-in defaults\n";
    }

    RenderSettings settings;
    settings.width            = config.width;
    settings.height           = config.height;
    settings.samples_per_axis = config.samples_per_axis;
    settings.threads          = static_cast<unsigned int>(std::max(0, config.threads));
    settings.sky_gain         = config.sky_gain;
    settings.quality.d_phi    = config.trace_d_phi;
    settings.quality.phi_max  = config.trace_phi_max * PI;

    verify_critical_impact_parameter(settings.quality);

    const Disk disk{config.disk_r_inner, config.disk_r_outer, config.disk_t_peak};

    // Nearly edge-on: the classic view, where lensing lifts the far side of the disk
    // over the top of the hole and wraps its underside beneath.
    const double elevation = config.camera_elevation * PI / 180.0;

    const Camera cam(Vec3(0.0, config.camera_distance * std::sin(elevation),
                          config.camera_distance * std::cos(elevation)),
                     Vec3(0.0, 0.0, 0.0),  // look_at: the singularity
                     Vec3(0.0, 1.0, 0.0),  // up: the disk's rotation axis
                     config.camera_fov,
                     static_cast<double>(config.width) / config.height);

    const Sky sky(config.sky_texture, config.sky_tilt, config.sky_roll);
    warm_up_tables(disk, sky);

    std::cout << "rendering " << settings.width << "x" << settings.height << " at "
              << settings.samples_per_axis << "x" << settings.samples_per_axis
              << " samples\n";

    std::vector<Vec3> radiance;

    const auto       started = std::chrono::steady_clock::now();
    const FrameStats stats   = render_frame(settings, cam, disk, sky, radiance);
    const double     seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
            .count();

    const double exposure =
        auto_exposure(radiance, config.exposure_percentile, config.exposure_target);

    img framebuffer(config.width, config.height);
    encode_frame(radiance, exposure, framebuffer.color);
    framebuffer.save(config.output);

    std::cout << "render time: " << seconds << " s\n"
              << "redshift factor g over the disk: " << stats.g_min << " .. "
              << stats.g_max << "   (1 = unshifted)\n"
              << "auto-exposure scale: " << exposure << '\n'
              << "wrote " << config.output << '\n';
}
