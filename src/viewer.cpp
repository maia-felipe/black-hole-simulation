// Interactive viewer: the same CPU tracer as the offline renderer, driven by an orbit
// camera and a progressive-refinement ladder, presented through a single OpenGL
// texture. No physics lives on the GPU -- it only blits the frame we traced.

#include "camera.h"
#include "config.h"
#include "disk.h"
#include "geodesic.h"
#include "image.h"
#include "renderer.h"
#include "sky.h"
#include "vec3.h"

#define GL_SILENCE_DEPRECATION  // macOS froze OpenGL at 4.1; that is fine here
#include <OpenGL/gl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr double PI = 3.14159265358979323846;

// Progressive refinement ladder, cheapest first. While the camera is moving we stay on
// level 0; once it stops we climb one rung per frame. `scale` divides the framebuffer
// resolution, so on a Retina display level 0 is genuinely small.
struct QualityLevel {
    int    scale;
    int    samples_per_axis;
    double d_phi;
    double phi_max_pi;
};

constexpr QualityLevel LEVELS[] = {
    {4, 1, 0.10, 8.0},
    {3, 1, 0.05, 12.0},
    {2, 1, 0.02, 20.0},
    {1, 1, 0.01, 40.0},
};
constexpr int NUM_LEVELS = static_cast<int>(sizeof(LEVELS) / sizeof(LEVELS[0]));

// Exposure is derived from each frame's own luminance histogram, so recomputing it per
// frame makes the whole image pulse as the camera moves. Damp it towards the target
// instead: the eye accepts an exposure that settles, but not one that flickers.
constexpr double EXPOSURE_SMOOTHING = 0.15;

struct Orbit {
    double azimuth   = 0.0;   // radians
    double elevation = 0.0;   // radians above the disk plane
    double distance  = 100.0;

    Vec3 eye() const {
        return Vec3(distance * std::cos(elevation) * std::sin(azimuth),
                    distance * std::sin(elevation),
                    distance * std::cos(elevation) * std::cos(azimuth));
    }
};

// All mutable UI state, hung off the GLFW window so the callbacks can reach it.
struct ViewerState {
    Orbit  orbit;
    bool   dragging      = false;
    double last_x        = 0.0;
    double last_y        = 0.0;
    bool   camera_moved  = true;
    bool   save_requested = false;
};

void cursor_position_callback(GLFWwindow* window, double x, double y) {
    auto* state = static_cast<ViewerState*>(glfwGetWindowUserPointer(window));
    if (!state->dragging) {
        state->last_x = x;
        state->last_y = y;
        return;
    }

    const double dx = x - state->last_x;
    const double dy = y - state->last_y;
    state->last_x   = x;
    state->last_y   = y;

    state->orbit.azimuth -= dx * 0.005;
    state->orbit.elevation =
        std::clamp(state->orbit.elevation + dy * 0.005, -1.5, 1.5);

    state->camera_moved = true;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* state = static_cast<ViewerState*>(glfwGetWindowUserPointer(window));
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    state->dragging = (action == GLFW_PRESS);
    if (state->dragging) glfwGetCursorPos(window, &state->last_x, &state->last_y);
}

void scroll_callback(GLFWwindow* window, double /*dx*/, double dy) {
    auto* state = static_cast<ViewerState*>(glfwGetWindowUserPointer(window));

    // Multiplicative zoom: a scroll notch should feel the same at every distance.
    state->orbit.distance =
        std::clamp(state->orbit.distance * std::exp(-dy * 0.08), 8.0, 600.0);
    state->camera_moved = true;
}

void key_callback(GLFWwindow* window, int key, int /*sc*/, int action, int /*mods*/) {
    if (action != GLFW_PRESS) return;
    auto* state = static_cast<ViewerState*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (key == GLFW_KEY_S)      state->save_requested = true;
}

GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "shader compile failed:\n" << log << '\n';
    }
    return shader;
}

// A single oversized triangle covers the screen with no vertex buffer at all: the
// positions come from gl_VertexID. Cheaper than a quad and with no diagonal seam.
const char* VERTEX_SOURCE = R"(#version 150 core
out vec2 uv;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    uv = vec2(p.x, 1.0 - p.y);   // image rows run top-down, GL texture rows bottom-up
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

const char* FRAGMENT_SOURCE = R"(#version 150 core
in vec2 uv;
uniform sampler2D frame;
out vec4 colour;
void main() { colour = texture(frame, uv); }
)";

}  // namespace

int main(int argc, char** argv) {
    // --selftest renders one frame, presents it, reads the pixels back off the GPU and
    // writes them to a PNG, then exits. That exercises the whole presentation path --
    // texture upload, shader, and the vertical flip -- which "it launched without
    // crashing" does not: a blank window would pass that.
    std::string config_path = "scene.cfg";
    bool        selftest    = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--selftest") selftest = true;
        else                     config_path = arg;
    }

    Config config;
    if (!load_config(config_path, config)) {
        std::cout << "config: " << config_path
                  << " not found, using built-in defaults\n";
    }

    if (!glfwInit()) {
        std::cerr << "failed to initialise GLFW\n";
        return 1;
    }

    // macOS only exposes modern OpenGL through a forward-compatible core profile.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    if (selftest) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window =
        glfwCreateWindow(config.width, config.height, "Schwarzschild", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "failed to create a window\n";
        glfwTerminate();
        return 1;
    }

    ViewerState state;
    state.orbit.elevation = config.camera_elevation * PI / 180.0;
    state.orbit.distance  = config.camera_distance;

    glfwSetWindowUserPointer(window, &state);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    const GLuint vertex   = compile_shader(GL_VERTEX_SHADER, VERTEX_SOURCE);
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SOURCE);
    const GLuint program  = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "frame"), 0);

    // A core profile refuses to draw without a bound VAO, even when the shader reads
    // no attributes at all.
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Rows of packed RGB bytes are not 4-byte aligned in general.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const Disk disk{config.disk_r_inner, config.disk_r_outer, config.disk_t_peak};
    const Sky  sky(config.sky_texture, config.sky_tilt, config.sky_roll);
    warm_up_tables(disk, sky);

    std::vector<Vec3>          radiance;
    std::vector<unsigned char> rgb;

    double smoothed_exposure = 0.0;
    int    level             = 0;
    bool   needs_render      = true;
    int    saved_frames      = 0;

    if (selftest) {
        int fb_width = 0, fb_height = 0;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);

        // Level with scale 1, so the texture maps to the framebuffer one to one and a
        // readback should reproduce the uploaded bytes exactly.
        const QualityLevel& q = LEVELS[NUM_LEVELS - 1];

        RenderSettings settings;
        settings.width            = fb_width;
        settings.height           = fb_height;
        settings.samples_per_axis = q.samples_per_axis;
        settings.threads  = static_cast<unsigned int>(std::max(0, config.threads));
        settings.sky_gain = config.sky_gain;
        settings.quality  = TraceQuality{q.d_phi, q.phi_max_pi * PI};

        const Camera camera(state.orbit.eye(), Vec3(0.0, 0.0, 0.0),
                            Vec3(0.0, 1.0, 0.0), config.camera_fov,
                            static_cast<double>(fb_width) / fb_height);

        render_frame(settings, camera, disk, sky, radiance);
        encode_frame(radiance,
                     auto_exposure(radiance, config.exposure_percentile,
                                   config.exposure_target),
                     rgb);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, fb_width, fb_height, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, rgb.data());

        glViewport(0, 0, fb_width, fb_height);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glFinish();

        std::vector<unsigned char> read_back(static_cast<std::size_t>(fb_width) *
                                             fb_height * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, fb_width, fb_height, GL_RGB, GL_UNSIGNED_BYTE,
                     read_back.data());

        // glReadPixels hands back rows bottom-up; our image rows run top-down.
        img out(fb_width, fb_height);
        const std::size_t stride = static_cast<std::size_t>(fb_width) * 3;
        for (int y = 0; y < fb_height; ++y) {
            std::copy_n(read_back.begin() + (fb_height - 1 - y) * stride, stride,
                        out.color.begin() + y * stride);
        }
        out.save("viewer_selftest.png");

        std::cout << "selftest: presented and read back " << fb_width << "x"
                  << fb_height << " -> viewer_selftest.png\n";

        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }

    std::cout << "drag to orbit, scroll to zoom, S to save a full-quality PNG, "
                 "Esc to quit\n";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (state.camera_moved) {
            state.camera_moved = false;
            level              = 0;
            needs_render       = true;
        }

        int fb_width = 0, fb_height = 0;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width == 0 || fb_height == 0) continue;

        if (needs_render) {
            const QualityLevel& q = LEVELS[level];

            RenderSettings settings;
            settings.width  = std::max(1, fb_width / q.scale);
            settings.height = std::max(1, fb_height / q.scale);
            settings.samples_per_axis = q.samples_per_axis;
            settings.threads  = static_cast<unsigned int>(std::max(0, config.threads));
            settings.sky_gain = config.sky_gain;
            settings.quality  = TraceQuality{q.d_phi, q.phi_max_pi * PI};

            const Camera camera(
                state.orbit.eye(), Vec3(0.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0),
                config.camera_fov,
                static_cast<double>(settings.width) / settings.height);

            const auto started = std::chrono::steady_clock::now();
            render_frame(settings, camera, disk, sky, radiance);
            const double seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                              started)
                    .count();

            const double target = auto_exposure(radiance, config.exposure_percentile,
                                                config.exposure_target);
            smoothed_exposure =
                smoothed_exposure <= 0.0
                    ? target
                    : smoothed_exposure + EXPOSURE_SMOOTHING * (target - smoothed_exposure);

            encode_frame(radiance, smoothed_exposure, rgb);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, settings.width, settings.height, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

            char title[160];
            std::snprintf(title, sizeof(title),
                          "Schwarzschild  |  %dx%d  L%d/%d  %.1f ms  %.0f fps  |  "
                          "r = %.0f M  %.0f deg",
                          settings.width, settings.height, level + 1, NUM_LEVELS,
                          seconds * 1000.0, 1.0 / std::max(seconds, 1e-6),
                          state.orbit.distance, state.orbit.elevation * 180.0 / PI);
            glfwSetWindowTitle(window, title);

            if (level + 1 < NUM_LEVELS) ++level;
            else                        needs_render = false;
        }

        if (state.save_requested) {
            state.save_requested = false;

            RenderSettings settings;
            settings.width            = config.width;
            settings.height           = config.height;
            settings.samples_per_axis = config.samples_per_axis;
            settings.threads = static_cast<unsigned int>(std::max(0, config.threads));
            settings.sky_gain = config.sky_gain;
            settings.quality  = TraceQuality{config.trace_d_phi,
                                             config.trace_phi_max * PI};

            const Camera camera(
                state.orbit.eye(), Vec3(0.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0),
                config.camera_fov,
                static_cast<double>(settings.width) / settings.height);

            std::cout << "saving a full-quality frame..." << std::flush;

            std::vector<Vec3> full;
            render_frame(settings, camera, disk, sky, full);

            std::vector<unsigned char> bytes;
            encode_frame(full,
                         auto_exposure(full, config.exposure_percentile,
                                       config.exposure_target),
                         bytes);

            img out(settings.width, settings.height);
            out.color = bytes;

            const std::string name = "viewer_" + std::to_string(saved_frames++) + ".png";
            out.save(name);
            std::cout << " wrote " << name << '\n';
        }

        glViewport(0, 0, fb_width, fb_height);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(window);
    }

    glDeleteTextures(1, &texture);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
