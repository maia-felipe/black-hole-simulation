#include "renderer.h"

#include "ray.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>

namespace {

Vec3 shade(const PhotonResult& photon, const Disk& disk, const Sky& sky,
           double sky_gain) {
    switch (photon.fate) {
        case PhotonFate::Captured:
            return Vec3(0.0, 0.0, 0.0);

        case PhotonFate::Escaped:
            return sky_gain * sky.sample(photon.direction);

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

void render_rows(const RenderSettings& settings, const Camera& camera, const Disk& disk,
                 const Sky& sky, std::vector<Vec3>& radiance,
                 std::atomic<int>& next_row, FrameStats& out) {
    const int    samples     = settings.samples_per_axis;
    const double inv_samples = 1.0 / (samples * samples);

    // Accumulate on this thread's own stack. Writing straight into the shared stats
    // array would have neighbouring workers hammering the same cache line for no
    // reason -- false sharing, and it costs far more than the diagnostic is worth.
    FrameStats local;

    for (;;) {
        // Dynamic scheduling. Row cost varies by orders of magnitude: rows grazing the
        // photon ring spiral for thousands of integration steps while open-sky rows
        // finish almost immediately. Handing out fixed blocks up front would leave
        // most threads idle waiting on one unlucky worker.
        const int y = next_row.fetch_add(1, std::memory_order_relaxed);
        if (y >= settings.height) break;

        for (int x = 0; x < settings.width; ++x) {
            Vec3 sum(0.0, 0.0, 0.0);

            for (int sy = 0; sy < samples; ++sy) {
                for (int sx = 0; sx < samples; ++sx) {
                    // Sample at sub-pixel centres of a regular SxS grid.
                    const double px = x + (sx + 0.5) / samples;
                    const double py = y + (sy + 0.5) / samples;

                    // Image rows run top-down; the viewport's v axis runs bottom-up.
                    const Ray r = camera.get_ray(px / (settings.width - 1),
                                                 1.0 - py / (settings.height - 1));

                    const PhotonResult photon =
                        trace_photon(r.origin, r.direction, disk, settings.quality);

                    if (photon.fate == PhotonFate::HitDisk) {
                        local.g_min = std::min(local.g_min, photon.redshift);
                        local.g_max = std::max(local.g_max, photon.redshift);
                    }
                    sum = sum + shade(photon, disk, sky, settings.sky_gain);
                }
            }
            // Each row is a disjoint slice of the buffer, and one row spans
            // width * sizeof(Vec3) bytes -- far more than a cache line -- so no two
            // workers ever touch the same line. No locking, no false sharing.
            radiance[static_cast<std::size_t>(y) * settings.width + x] =
                inv_samples * sum;
        }
    }
    out = local;
}

}  // namespace

void warm_up_tables(const Disk& disk, const Sky& sky) {
    (void)disk_temperature(disk, 0.5 * (disk.r_inner + disk.r_outer));
    (void)blackbody_rgb(5000.0);
    (void)sky.sample(Vec3(0.0, 0.0, 1.0));
}

FrameStats render_frame(const RenderSettings& settings, const Camera& camera,
                        const Disk& disk, const Sky& sky,
                        std::vector<Vec3>& radiance) {
    const std::size_t pixels =
        static_cast<std::size_t>(settings.width) * settings.height;

    // Vec3 has no default constructor, so assign rather than resize().
    if (radiance.size() != pixels) radiance.assign(pixels, Vec3(0.0, 0.0, 0.0));

    unsigned int workers =
        settings.threads > 0 ? settings.threads : std::thread::hardware_concurrency();
    if (workers == 0) workers = 1;

    std::atomic<int>         next_row{0};
    std::vector<FrameStats>  stats(workers);
    std::vector<std::thread> pool;
    pool.reserve(workers - 1);

    // Spawn workers-1 threads and let this one take a share too, so we do not leave a
    // core idle just to supervise.
    for (unsigned int i = 1; i < workers; ++i) {
        pool.emplace_back(render_rows, std::cref(settings), std::cref(camera),
                          std::cref(disk), std::cref(sky), std::ref(radiance),
                          std::ref(next_row), std::ref(stats[i]));
    }
    render_rows(settings, camera, disk, sky, radiance, next_row, stats[0]);

    for (std::thread& t : pool) t.join();

    FrameStats merged;
    for (const FrameStats& s : stats) {
        merged.g_min = std::min(merged.g_min, s.g_min);
        merged.g_max = std::max(merged.g_max, s.g_max);
    }
    return merged;
}

double auto_exposure(const std::vector<Vec3>& radiance, double percentile,
                     double target) {
    std::vector<double> lums;
    lums.reserve(radiance.size());

    for (const Vec3& c : radiance) {
        const double l = luminance(c);
        if (l > 0.0) lums.push_back(l);
    }
    if (lums.empty()) return 1.0;

    const std::size_t k = static_cast<std::size_t>(percentile * (lums.size() - 1));
    std::nth_element(lums.begin(), lums.begin() + k, lums.end());

    return lums[k] > 0.0 ? target / lums[k] : 1.0;
}

void encode_frame(const std::vector<Vec3>& radiance, double exposure,
                  std::vector<unsigned char>& rgb) {
    rgb.resize(radiance.size() * 3);

    for (std::size_t i = 0; i < radiance.size(); ++i) {
        const Vec3 c = exposure * radiance[i];

        rgb[i * 3 + 0] = to_byte(c.x);
        rgb[i * 3 + 1] = to_byte(c.y);
        rgb[i * 3 + 2] = to_byte(c.z);
    }
}
