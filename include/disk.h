#pragma once
#include "vec3.h"

// Geometrically thin, optically thick accretion disk lying in the y = 0 plane and
// rotating about +y. Radii are in units of M (Schwarzschild ISCO is at r = 6).
struct Disk {
    double r_inner;  // no-torque inner boundary
    double r_outer;
    double t_peak;   // effective temperature at the flux maximum, in kelvin
};

// Effective temperature of the relativistic (Page-Thorne / Novikov-Thorne) thin disk
// with a no-torque inner boundary, normalized so T = t_peak at the flux maximum.
// Assumes r_inner is the Schwarzschild ISCO, r = 6M.
double disk_temperature(const Disk& disk, double r);

// Linear sRGB colour of a Planck blackbody at temperature T, normalized to unit
// luminance (Y = 1). Brightness is carried separately by the Stefan-Boltzmann T^4
// factor, so the two multiply back to the correct radiance.
Vec3 blackbody_rgb(double T);
