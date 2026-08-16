#pragma once
#include <string>

// Scene parameters. Kept in a text file rather than as constants in main.cpp so that
// framing and physics experiments stop costing a recompile.
struct Config {
    int width            = 800;
    int height           = 450;
    int samples_per_axis = 3;
    int threads          = 0;  // 0 = use hardware_concurrency()

    // Radii in units of M. The Schwarzschild ISCO is at r = 6, and the Page-Thorne
    // flux profile assumes the inner edge sits exactly there.
    double disk_r_inner = 6.0;
    double disk_r_outer = 36.0;
    double disk_t_peak  = 10500.0;  // kelvin, at the flux maximum

    double camera_distance  = 100.0;  // in units of M
    double camera_elevation = 6.0;    // degrees above the disk plane
    double camera_fov       = 24.0;   // vertical, degrees

    std::string sky_texture = "assets/eso0932a.jpg";
    double      sky_tilt    = 13.0;  // degrees; see Sky's constructor
    double      sky_roll    = 45.0;
    double      sky_gain    = 1.0;

    double exposure_percentile = 0.995;
    double exposure_target     = 0.9;

    // Geodesic integration accuracy. Lowering trace_d_phi costs time and buys sky
    // accuracy; see the Phase 6 notes in CLAUDE.md. trace_phi_max is in units of pi.
    double trace_d_phi   = 0.01;
    double trace_phi_max = 40.0;

    std::string output = "output.png";
};

// Applies every `key = value` line found in `path` on top of the defaults above.
// Returns false if the file could not be opened, in which case `config` is untouched
// and still perfectly usable. Malformed lines are reported and skipped rather than
// aborting the render.
bool load_config(const std::string& path, Config& config);
