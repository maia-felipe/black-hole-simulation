#include "camera.h"
#include <cmath>

namespace {
constexpr double PI = 3.14159265358979323846;
}

Camera::Camera(Vec3 look_from, Vec3 look_at, Vec3 vup, double vfov_deg, double aspect)
    : origin(look_from), lower_left(0, 0, 0), horizontal(0, 0, 0), vertical(0, 0, 0)
{
    // With the viewport one unit away, half its height is tan(fov/2).
    const double theta      = vfov_deg * PI / 180.0;
    const double viewport_h = 2.0 * std::tan(theta / 2.0);
    const double viewport_w = aspect * viewport_h;

    // Orthonormal camera basis: w points backwards (target -> eye), u right, v up.
    const Vec3 w = (look_from - look_at).normalize();
    const Vec3 u = cross(vup, w).normalize();
    const Vec3 v = cross(w, u);

    horizontal = viewport_w * u;
    vertical   = viewport_h * v;
    lower_left = origin - horizontal / 2.0 - vertical / 2.0 - w;
}

Ray Camera::get_ray(double s, double t) const {
    const Vec3 target = lower_left + s * horizontal + t * vertical;
    return Ray(origin, (target - origin).normalize());
}
