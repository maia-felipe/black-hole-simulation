#include "geodesic.h"
#include "integrator.h"

#include <cmath>

namespace {

constexpr double PI = 3.14159265358979323846;

constexpr double MASS       = 1.0;
constexpr double R_HORIZON  = 2.0 * MASS;
constexpr double R_FAR      = 1000.0;
constexpr double D_PHI      = 0.01;
constexpr double PHI_MAX    = 40.0 * PI;  // give up: photon is asymptotically trapped
constexpr double RADIAL_EPS = 1e-12;

// Binet form of the null geodesic in Schwarzschild:
//     d^2u/dphi^2 + u = 3*M*u^2,   u = 1/r
// The right-hand side is regular all the way through the horizon, so no special
// handling is needed as u passes 1/2.
State2 binet(double /*phi*/, const State2& y) {
    return State2{y.du, 3.0 * MASS * y.u * y.u - y.u};
}

PhotonResult captured() {
    return PhotonResult{PhotonFate::Captured, Vec3(0.0, 0.0, 0.0), 0.0, 0.0};
}

PhotonResult escaped(const Vec3& direction) {
    return PhotonResult{PhotonFate::Escaped, direction, 0.0, 0.0};
}

}  // namespace

PhotonResult trace_photon(const Vec3& origin, const Vec3& dir, const Disk& disk) {
    const double r0 = origin.length();
    if (r0 <= R_HORIZON) return captured();

    // Spherical symmetry makes every photon path planar. Build that plane's basis:
    // e1 = outward radial at the camera, e2 = the in-plane perpendicular chosen so
    // that phi grows along the photon's direction of travel.
    const Vec3   e1      = origin / r0;
    const double cos_psi = dot(dir, e1);
    const Vec3   tang    = dir - cos_psi * e1;
    const double sin_psi = tang.length();

    if (sin_psi < RADIAL_EPS) {
        // Zero angular momentum: the photon falls straight in or flies straight out.
        return cos_psi < 0.0 ? captured() : escaped(dir);
    }
    const Vec3 e2 = tang / sin_psi;

    // Conserved impact parameter b = |L|/E, and its component about the disk's spin
    // axis (+y). Both |L| and the orbital-plane normal are conserved, so b_axis is a
    // constant of the motion and only has to be computed once.
    const Vec3   normal = cross(e1, e2);
    const double b      = r0 * sin_psi / std::sqrt(1.0 - 2.0 * MASS / r0);
    const double b_axis = b * normal.y;

    // Height above the disk plane is r(phi) * (e1.y*cos(phi) + e2.y*sin(phi)), and
    // r > 0 always -- so the plane crossings are the zeros of a pure sinusoid, known
    // in closed form. No root-finding against the ODE is needed; we only have to
    // take one partial RK4 step to land exactly on each crossing.
    const double hA = e1.y;
    const double hB = e2.y;

    // Degenerate case: the photon's own plane *is* the disk plane.
    const bool coplanar = std::hypot(hA, hB) < RADIAL_EPS;

    double phi_cross = std::atan2(-hA, hB);
    while (phi_cross <= 0.0) phi_cross += PI;

    // psi is measured in the observer's local orthonormal frame, but u and phi are
    // coordinates. The sqrt(1 - 2M/r0) factor converts between them: proper radial
    // length is dr/sqrt(1-2M/r) while proper tangential length is r*dphi.
    State2 y{1.0 / r0,
             -std::sqrt(1.0 - 2.0 * MASS / r0) * cos_psi / (r0 * sin_psi)};

    for (double phi = 0.0; phi < PHI_MAX; phi += D_PHI) {
        if (!coplanar && phi_cross < phi + D_PHI) {
            const State2 at_plane = rk4_step(y, phi, phi_cross - phi, binet);
            const double r        = at_plane.u > 0.0 ? 1.0 / at_plane.u : 0.0;

            if (r >= disk.r_inner && r <= disk.r_outer) {
                // Circular timelike geodesic in Schwarzschild: Omega = sqrt(M/r^3)
                // (the Newtonian form, exactly, in these coordinates) and
                // u^t = 1/sqrt(1 - 3M/r), which diverges at the photon sphere.
                //
                // g = nu_obs/nu_emit collapses to one expression carrying the
                // gravitational redshift (u^t) and the Doppler shift (Omega*b_axis)
                // together. It is invariant under p -> -p, so tracing the photon
                // backwards gives the same value as tracing it forwards.
                const double omega = std::sqrt(MASS / (r * r * r));
                const double u_t   = 1.0 / std::sqrt(1.0 - 3.0 * MASS / r);

                return PhotonResult{PhotonFate::HitDisk, Vec3(0.0, 0.0, 0.0), r,
                                    1.0 / (u_t * (1.0 - omega * b_axis))};
            }
            phi_cross += PI;  // missed the annulus; wait for the next crossing
        }

        y = rk4_step(y, phi, D_PHI, binet);

        if (y.u >= 1.0 / R_HORIZON) return captured();

        if (y.u <= 1.0 / R_FAR) {
            // A step can overshoot past u = 0 only for near-radial outbound rays,
            // which are essentially undeflected.
            if (y.u <= 0.0) return escaped(dir);

            const double phi_end = phi + D_PHI;
            const double r       = 1.0 / y.u;
            const double dr      = -y.du / (y.u * y.u);

            // d/dphi of  r * (cos(phi)*e1 + sin(phi)*e2). Far from the hole the
            // metric is flat, so this coordinate tangent is the physical direction.
            const Vec3 radial     = std::cos(phi_end) * e1 + std::sin(phi_end) * e2;
            const Vec3 tangential = -std::sin(phi_end) * e1 + std::cos(phi_end) * e2;

            return escaped((dr * radial + r * tangential).normalize());
        }
    }

    // Never escaped, never crossed the horizon: spiralling near the photon sphere.
    return captured();
}
