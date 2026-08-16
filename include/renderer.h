#pragma once
#include <vector>

#include "camera.h"
#include "disk.h"
#include "geodesic.h"
#include "sky.h"
#include "vec3.h"

// Everything the tracer needs that is not the scene itself. Resolution and quality are
// per-call rather than per-scene so the interactive viewer can vary them frame to frame
// while the offline renderer keeps them fixed.
struct RenderSettings {
    int          width            = 800;
    int          height           = 450;
    int          samples_per_axis = 3;
    unsigned int threads          = 0;  // 0 = one per hardware thread
    double       sky_gain         = 1.0;
    TraceQuality quality{};
};

struct FrameStats {
    double g_min = 1e30;
    double g_max = -1e30;
};

// Traces a whole frame into `radiance`, resizing it to width*height. Rows are handed
// out to worker threads by an atomic counter.
FrameStats render_frame(const RenderSettings& settings, const Camera& camera,
                        const Disk& disk, const Sky& sky, std::vector<Vec3>& radiance);

// Exposure scale taken from the frame's own luminance distribution: the given quantile
// of non-black luminance is mapped onto `target`.
double auto_exposure(const std::vector<Vec3>& radiance, double percentile, double target);

// Applies the exposure, tone maps and gamma encodes into 8-bit RGB triples. `rgb` is
// resized to 3 * radiance.size().
void encode_frame(const std::vector<Vec3>& radiance, double exposure,
                  std::vector<unsigned char>& rgb);

// Builds the lazily-initialized colour tables on the calling thread. Function-local
// statics are thread-safe to initialize since C++11, but leaving it to the workers
// makes them all contend on the same first-use guard.
void warm_up_tables(const Disk& disk, const Sky& sky);
