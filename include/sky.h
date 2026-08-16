#pragma once
#include <string>

#include "vec3.h"

// The celestial sphere behind the black hole.
//
// Samples an equirectangular texture when one loads from disk, and otherwise falls
// back to a deterministic procedural starfield, so a render never depends on having
// an asset available.
class Sky {
public:
    // `tilt_deg` / `roll_deg` orient the map relative to the scene. Star maps put the
    // galactic plane on the texture's horizontal midline, which maps straight onto the
    // accretion disk plane -- so with no rotation the Milky Way hides behind the disk.
    // A black hole's spin axis has no reason to align with its galaxy's pole anyway.
    explicit Sky(const std::string& texture_path, double tilt_deg = 0.0,
                 double roll_deg = 0.0);
    ~Sky();

    // Owns a raw buffer allocated by stb_image, so the compiler-generated copies
    // would double-free it. There is no reason to copy a Sky, so forbid it outright
    // rather than write a deep copy nobody needs.
    Sky(const Sky&)            = delete;
    Sky& operator=(const Sky&) = delete;

    bool has_texture() const { return pixels_ != nullptr; }

    // Radiance arriving from `direction` (need not be normalized).
    Vec3 sample(const Vec3& direction) const;

private:
    Vec3 sample_texture(double u, double v) const;

    float* pixels_ = nullptr;  // 3 linear floats per texel, owned
    int    width_  = 0;
    int    height_ = 0;

    Vec3 row0_, row1_, row2_;  // rows of the orientation matrix
};
