#include "sky.h"

#include "disk.h"  // blackbody_rgb, for star colours
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

constexpr double PI = 3.14159265358979323846;

// Star grid on the (theta, phi) sphere. Cells are ~0.12 degrees across, so a star
// covers a couple of pixels at the resolutions we render at.
constexpr int    STAR_ROWS    = 1500;
constexpr int    STAR_COLS    = 3000;
constexpr double STAR_DENSITY = 0.012;  // fraction of cells holding a star
constexpr double STAR_RADIUS  = 0.45;   // in cell widths
constexpr double STAR_GAIN    = 2.2;

// Faint galactic band, tilted so it does not line up with the accretion disk.
constexpr double BAND_WIDTH = 0.17;
constexpr double BAND_GAIN  = 0.010;

// Integer avalanche hash (Wang/Murmur style): cheap, and good enough that adjacent
// cells look uncorrelated.
std::uint32_t hash_u32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

// Deterministic pseudo-random in [0,1) for a grid cell and a channel index. Being a
// pure function of the cell is what makes the starfield stable: no stored state, and
// the same sky whatever order the pixels are traced in.
double hash01(int row, int col, std::uint32_t salt) {
    const std::uint32_t h =
        hash_u32(static_cast<std::uint32_t>(row) * 73856093u ^
                 static_cast<std::uint32_t>(col) * 19349663u ^ hash_u32(salt));
    return h * (1.0 / 4294967296.0);
}

// blackbody_rgb integrates a Planck spectrum against the CIE curves, which is far too
// expensive to call per star per sample. Quantize stellar temperature into a handful
// of buckets and precompute those once.
const Vec3& star_color(std::uint32_t bucket) {
    static const std::vector<Vec3> table = [] {
        std::vector<Vec3> t;
        for (int i = 0; i < 16; ++i) {
            t.push_back(blackbody_rgb(2800.0 + i * (11000.0 - 2800.0) / 15.0));
        }
        return t;
    }();
    return table[bucket & 15u];
}

Vec3 procedural_sky(const Vec3& d, double theta, double phi) {
    const Vec3   band_pole = Vec3(0.36, 0.88, 0.31).normalize();
    const double band      = dot(d, band_pole);

    Vec3 color = (BAND_GAIN * std::exp(-band * band / (2.0 * BAND_WIDTH * BAND_WIDTH))) *
                 Vec3(1.0, 0.86, 0.66);

    // Continuous cell coordinates; the star lives at a hashed offset inside its cell.
    const double gr = theta / PI * STAR_ROWS;
    const double gc = (phi + PI) / (2.0 * PI) * STAR_COLS;

    const int r0 = static_cast<int>(std::floor(gr));
    const int c0 = static_cast<int>(std::floor(gc));

    // Longitude cells narrow towards the poles; scale so distances stay angular.
    const double lon_scale = std::max(std::sin(theta), 1e-6);

    // Sweep the 3x3 neighbourhood, so a star sitting near a cell edge still lights
    // up the pixels on the other side of that edge.
    for (int dr = -1; dr <= 1; ++dr) {
        const int row = r0 + dr;
        if (row < 0 || row >= STAR_ROWS) continue;

        for (int dc = -1; dc <= 1; ++dc) {
            const int col = ((c0 + dc) % STAR_COLS + STAR_COLS) % STAR_COLS;

            if (hash01(row, col, 0) > STAR_DENSITY) continue;  // most cells are empty

            const double star_r = row + hash01(row, col, 1);
            const double star_c = (c0 + dc) + hash01(row, col, 2);  // unwrapped, matches gc

            const double dy = gr - star_r;
            const double dx = (gc - star_c) * lon_scale;
            const double d2 = dx * dx + dy * dy;

            // Steep power law: a great many faint stars, a handful of bright ones.
            const double h   = hash01(row, col, 3);
            const double mag = h * h * h * h * h;

            const std::uint32_t bucket =
                static_cast<std::uint32_t>(hash01(row, col, 4) * 16.0);

            color = color + (STAR_GAIN * mag *
                             std::exp(-d2 / (2.0 * STAR_RADIUS * STAR_RADIUS))) *
                                star_color(bucket);
        }
    }
    return color;
}

}  // namespace

Sky::Sky(const std::string& texture_path, double tilt_deg, double roll_deg)
    : row0_(1.0, 0.0, 0.0), row1_(0.0, 1.0, 0.0), row2_(0.0, 0.0, 1.0) {
    // Orientation matrix R = Rz(roll) * Rx(tilt), stored as its three rows so that
    // sampling costs three dot products instead of a matrix type we do not otherwise
    // need. Applied to the *lookup* direction, so nothing about the physics moves.
    const double a = tilt_deg * PI / 180.0;
    const double b = roll_deg * PI / 180.0;

    const double ca = std::cos(a), sa = std::sin(a);
    const double cb = std::cos(b), sb = std::sin(b);

    row0_ = Vec3(cb, -sb * ca, sb * sa);
    row1_ = Vec3(sb, cb * ca, -cb * sa);
    row2_ = Vec3(0.0, sa, ca);

    // stbi_loadf converts 8-bit sRGB-ish input to linear floats, which is the space
    // the renderer works in. HDR files are already linear and pass through.
    int channels = 0;
    pixels_ = stbi_loadf(texture_path.c_str(), &width_, &height_, &channels, 3);

    if (pixels_ != nullptr) {
        std::cout << "sky: loaded " << texture_path << " (" << width_ << "x" << height_
                  << ")\n";
    } else {
        std::cout << "sky: no texture at " << texture_path
                  << ", using the procedural starfield\n";
    }
}

Sky::~Sky() {
    stbi_image_free(pixels_);  // safe on nullptr
}

Vec3 Sky::sample_texture(double u, double v) const {
    // Texel centres sit at half-integer coordinates.
    const double x = u * width_ - 0.5;
    const double y = v * height_ - 0.5;

    const int    x0 = static_cast<int>(std::floor(x));
    const int    y0 = static_cast<int>(std::floor(y));
    const double fx = x - x0;
    const double fy = y - y0;

    // Longitude wraps around; latitude clamps at the poles.
    const auto texel = [&](int px, int py) {
        px = ((px % width_) + width_) % width_;
        py = std::clamp(py, 0, height_ - 1);

        const float* p = pixels_ + (static_cast<std::size_t>(py) * width_ + px) * 3;
        return Vec3(p[0], p[1], p[2]);
    };

    const Vec3 top    = (1.0 - fx) * texel(x0, y0) + fx * texel(x0 + 1, y0);
    const Vec3 bottom = (1.0 - fx) * texel(x0, y0 + 1) + fx * texel(x0 + 1, y0 + 1);

    return (1.0 - fy) * top + fy * bottom;
}

Vec3 Sky::sample(const Vec3& direction) const {
    const Vec3 d = direction.normalize();
    const Vec3 s(dot(row0_, d), dot(row1_, d), dot(row2_, d));

    // Equirectangular convention: v = 0 is the +y pole (top row of the image),
    // u wraps once around the azimuth.
    const double theta = std::acos(std::clamp(s.y, -1.0, 1.0));
    const double phi   = std::atan2(s.z, s.x);

    if (pixels_ != nullptr) {
        return sample_texture((phi + PI) / (2.0 * PI), theta / PI);
    }
    return procedural_sky(s, theta, phi);
}
