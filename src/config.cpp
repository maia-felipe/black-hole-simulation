#include "config.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};

    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

}  // namespace

bool load_config(const std::string& path, Config& config) {
    std::ifstream file(path);
    if (!file) return false;

    std::string line;
    int         line_number = 0;

    while (std::getline(file, line)) {
        ++line_number;

        const auto hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);

        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            if (!trim(line).empty()) {
                std::cerr << path << ":" << line_number << ": expected 'key = value'\n";
            }
            continue;
        }

        const std::string key   = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));

        // stoi/stod throw on garbage. Report the offending line and carry on with the
        // default for that key rather than killing a render over one typo.
        try {
            bool matched = true;

            if      (key == "width")               config.width               = std::stoi(value);
            else if (key == "height")              config.height              = std::stoi(value);
            else if (key == "samples_per_axis")    config.samples_per_axis    = std::stoi(value);
            else if (key == "threads")             config.threads             = std::stoi(value);
            else if (key == "disk_r_inner")        config.disk_r_inner        = std::stod(value);
            else if (key == "disk_r_outer")        config.disk_r_outer        = std::stod(value);
            else if (key == "disk_t_peak")         config.disk_t_peak         = std::stod(value);
            else if (key == "camera_distance")     config.camera_distance     = std::stod(value);
            else if (key == "camera_elevation")    config.camera_elevation    = std::stod(value);
            else if (key == "camera_fov")          config.camera_fov          = std::stod(value);
            else if (key == "sky_texture")         config.sky_texture         = value;
            else if (key == "sky_tilt")            config.sky_tilt            = std::stod(value);
            else if (key == "sky_roll")            config.sky_roll            = std::stod(value);
            else if (key == "sky_gain")            config.sky_gain            = std::stod(value);
            else if (key == "exposure_percentile") config.exposure_percentile = std::stod(value);
            else if (key == "exposure_target")     config.exposure_target     = std::stod(value);
            else if (key == "trace_d_phi")         config.trace_d_phi         = std::stod(value);
            else if (key == "trace_phi_max")       config.trace_phi_max       = std::stod(value);
            else if (key == "output")              config.output              = value;
            else                                   matched = false;

            if (!matched) {
                std::cerr << path << ":" << line_number << ": unknown key '" << key
                          << "'\n";
            }
        } catch (const std::exception&) {
            std::cerr << path << ":" << line_number << ": bad value '" << value
                      << "' for '" << key << "', keeping the default\n";
        }
    }
    return true;
}
