#pragma once
#include "vec3.h"
#include "ray.h"

// Pinhole camera: an image plane ("viewport") placed one unit in front of the eye.
// get_ray maps normalized viewport coordinates (s, t) in [0,1]^2 to a world-space ray.
struct Camera {
    Vec3 origin;      // the eye
    Vec3 lower_left;  // world position of the viewport corner at (s=0, t=0)
    Vec3 horizontal;  // full-width vector of the viewport
    Vec3 vertical;    // full-height vector of the viewport

    Camera(Vec3 look_from, Vec3 look_at, Vec3 vup, double vfov_deg, double aspect);

    Ray get_ray(double s, double t) const;
};
