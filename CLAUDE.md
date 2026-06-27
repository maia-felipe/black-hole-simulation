# Black Hole Simulation — Project Context

> This file is loaded at the start of every session. It captures the scope, the
> working method, and where we left off, so any new session can continue seamlessly.

## What this project is

A **from-scratch, physically faithful Schwarzschild black hole renderer in C++**:
an accretion disk and a simple lensed background, viewed through a 3D camera. It
starts as an **offline CPU ray tracer that writes image files**, and graduates to a
**real-time interactive window** later.

## The real goal: LEARNING

The black hole is the vehicle; the destination is **the user becoming a strong
software-engineering-style C++ programmer**. The user comes from competitive
programming (single-file `g++`, strong algorithms) and wants to learn the *other*
way to use C++: build systems, multi-file architecture, numerical methods, graphics,
and performance. The user's **physics background is strong** (handles GR /
Schwarzschild metrics, geodesics, redshift, Doppler beaming with ease) — so effort
goes into the **coding craft, not deriving the physics**.

## METHODOLOGY — read this carefully (it's the core constraint)

- **Claude does NOT write the project's source code.** Not `.cpp`, not `.h`, not
  `CMakeLists.txt`. The user writes 100% of it. Claude's job is to **teach**.
- For each step, Claude: explains the **concept** + the **C++/tooling technique** +
  the **why**, points at the approach, then the user implements and Claude reviews
  and nudges.
- Style is **concept-first blended with Socratic**: explain unfamiliar/arbitrary
  syntax (e.g. CMake) thoroughly; use leading questions where struggling teaches more
  (e.g. "where would you add the newline?"). Reveal root causes by reading the user's
  files and the compiler errors *together*, rather than just handing a fix.
- **Claude MAY write:** this `CLAUDE.md`, the plan file, and Claude's own memory
  files. Claude MAY run read-only/diagnostic commands (build, run, read files, git
  status) to review the user's work. Claude must NOT author the user's code for them.
- **Conversation language: Portuguese (pt-BR).** Code, comments, and this file are in
  English. (Established mid-session at the user's request.)
- Every phase ends in a **visual milestone** (an image) — motivation + correctness check.

## Roadmap (full detail in the plan file)

Plan file: `~/.claude/plans/claude-i-d-like-to-velvety-lerdorf.md`

- **Phase 0** — Toolchain, project skeleton, first PPM image. *(in progress)*
- **Phase 1** — `Vec3` math from scratch (operators, dot/cross, normalize; tested).
- **Phase 2** — Flat-space ray tracer: pinhole camera, ray–sphere, framebuffer, PNG
  via single-header `stb_image_write`, sky gradient. → ray-traced sphere.
- **Phase 3** — Geodesic integration (RK4) in Schwarzschild spacetime. → photon ring + shadow.
- **Phase 4** — Lensed celestial-sphere background (direction→UV, bilinear sampling).
- **Phase 5** — Accretion disk (ISCO→outer, temperature/blackbody, gravitational
  redshift + Doppler beaming). → the Gargantua-style image.
- **Phase 6** — Multithreading, anti-aliasing, tone mapping, profiling, config.
- **Phase 7** *(stretch)* — Real-time window (GLFW/OpenGL or GLSL geodesics).

Choices deliberately deferred until we reach them: geodesic formulation (Cartesian
2nd-order ODE vs. orbital-plane reduction) in P3; disk emission fidelity in P5;
real-time path (CPU-upload vs. GLSL) in P7. Use libraries only for boring I/O
(`stb_image`/`stb_image_write`); everything that teaches is written from scratch.

## Environment

- macOS (Darwin), shell **zsh**.
- Compiler: **Apple clang++ 21** (`-std=c++20`).
- Build: **CMake 4.3.4** (no ninja installed → default "Unix Makefiles" generator).
- Build workflow: `cmake -S . -B build` (configure, run once / after editing
  CMakeLists.txt) then `cmake --build build` (compile+link), run `./build/<target>`.

## Conventions established so far

- Layout: `src/` (`.cpp`), `include/` (`.h`), `build/` (generated — must be gitignored),
  `CLAUDE.md` + `CMakeLists.txt` at root. (`assets/` to come for textures.)
- Headers use `#pragma once`; **declarations** in `.h`, **definitions** in `.cpp`;
  `#include "header.h"` + `target_include_directories(... include)` (never absolute paths).
- CMake: `add_executable` lists only `.cpp` (never headers); properties hung on the
  target via `target_compile_features` / `target_include_directories` with `PRIVATE`.

## Where we left off (update this each session)

- **Phase 0, partway through.** Done: toolchain verified; learned the
  compile→object→link model by hand-compiling a 2-file `greeting` program; learned
  declaration-vs-definition (root cause of bugs was an unsaved/empty header); wrote the
  first `CMakeLists.txt` (target `blackhole`, sources `src/main.cpp` + `src/greeting.cpp`);
  successfully built and ran via `cmake -S . -B build && cmake --build build`.
- The current `main.cpp`/`greeting.*` are a **throwaway learning scaffold**, not real
  project code.

### Immediate next steps
1. Create a `.gitignore` that ignores `build/`, then make the **first git commit**
   (repo currently has **no commits**). *(user writes/runs)*
2. **Phase 0 milestone:** replace the greeting logic with code that fills a buffer with
   a color gradient and writes it to a **PPM** file (plain-text image — understand every
   byte, no library). View it. Then we move to Phase 1 (`Vec3`).
