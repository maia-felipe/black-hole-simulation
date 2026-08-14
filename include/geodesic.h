#pragma once
#include "vec3.h"
#include "disk.h"

// State of the orbital-plane reduction: u = 1/r and its derivative w.r.t. phi.
// `inline` on the operators gives them one definition across all translation units.
struct State2 {
    double u;
    double du;
};

inline State2 operator+(const State2& a, const State2& b) {
    return State2{a.u + b.u, a.du + b.du};
}

inline State2 operator*(double s, const State2& a) {
    return State2{s * a.u, s * a.du};
}

enum class PhotonFate {
    Captured,  // crossed the event horizon (or asymptotically trapped)
    Escaped,   // reached the far field; `direction` is the outgoing direction
    HitDisk,   // struck the accretion disk; `disk_r` and `redshift` describe it
};

struct PhotonResult {
    PhotonFate fate;
    Vec3       direction;  // Escaped only
    double     disk_r;     // HitDisk only: emission radius, in units of M
    double     redshift;   // HitDisk only: g = nu_observed / nu_emitted
};

// Trace a photon through Schwarzschild spacetime, starting at `origin` with the
// locally-measured direction `dir` (as seen by a static observer there).
//
// Traced backwards from the camera, so the first disk crossing is the visible one:
// the disk is optically thick, and nothing behind it contributes.
//
// Units: G = c = M = 1, so the horizon sits at r = 2 and the photon sphere at r = 3.
PhotonResult trace_photon(const Vec3& origin, const Vec3& dir, const Disk& disk);
