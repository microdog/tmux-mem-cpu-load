# Codebase Map

## Overview

`tmux-mem-cpu-load` builds a single C++ executable that prints one tmux-ready status line containing system metrics. At runtime, the binary collects platform-specific CPU and memory data, optionally samples network traffic, formats each segment with plain tmux colors or Powerline separators, and writes the final string to stdout.

The build is intentionally simple:

- `CMakeLists.txt` selects the platform-specific source set.
- `common/version.h.in` generates `version.h` at configure time.
- `make test` runs CTest coverage for shared network logic, Linux network parsing, and CLI behavior.

## Top-Level Layout

| Path | Purpose |
| --- | --- |
| `CMakeLists.txt` | Build entry point, platform detection, executable definition, CTest registration. |
| `common/` | Shared headers, formatting code, CLI entry point, generated/shared helpers. |
| `linux/` | Linux metric collectors, including the `/proc`-backed network reader and parser seam. |
| `osx/` | macOS CPU and memory collectors using Mach and `sysctl`. |
| `freebsd/` | FreeBSD CPU and memory collectors using `sysctl`. |
| `openbsd/` | OpenBSD CPU and memory collectors, including compatibility workaround logic. |
| `netbsd/` | NetBSD CPU and memory collectors using `sysctl` and UVM stats. |
| `windows/` | Windows CPU, memory, network, and load-average support. |
| `builder/` | Dockcross-based helper scripts for cross-platform builds. |
| `tmux-mem-cpu-load.plugin.tmux` | TPM-oriented plugin bootstrap that tries to build the binary in place. |
| `tmux-mem-cpu-load.plugin.zsh` | Antigen/Zsh install helper that builds and installs to `/usr/local/bin`. |
| `README.rst` | User-facing usage, install, and configuration guide. |
| `CONTRIBUTING.rst` | Project workflow and style rules. |
| `seg-adj1.png`, `seg-adj2.png` | Reference screenshots for Powerline segment blending. |

## Runtime Flow

1. `common/main.cc` parses CLI flags with `getopt_long`.
2. `common/main.cc` samples memory immediately via `mem_status`.
3. If network display is enabled, `common/main.cc` launches network sampling concurrently with CPU sampling.
4. `common/network_sampler.cc` resolves interface selectors, reads two snapshots through the platform reader, aggregates the selected set, and classifies the result as hidden, ok, or error.
5. `common/main.cc` builds output in fixed order: network → memory → CPU → load.
6. Shared formatters in `common/` apply units, graphs, tmux color escapes, and Powerline separators.
7. The program prints one line to stdout for tmux to consume.

## Shared Core (`common/`)

### Entrypoint and orchestration

- `common/main.cc`
  - Defines `main`, `print_help`, and `cpu_string`.
  - Owns CLI parsing for colors, graph style, memory mode, CPU mode, averages count, and network options.
  - Coordinates sampler calls and concatenates all output segments.
  - Contains the current segment-order contract used by the rest of the codebase.

### Shared interfaces

- `common/cpu.h`
  - Declares `cpu_percentage()` and `get_cpu_count()`.
  - Defines `CPU_MODE` and platform-specific CPU state constants.
- `common/memory.h`
  - Declares `mem_status()` and `mem_string()`.
  - Defines `MemoryStatus` and `MEMORY_MODE`.
- `common/load.h`
  - Declares `load_string()`.
- `common/network.h`
  - Declares `NetworkOptions`, `NetworkSnapshot`, `NetworkStatus`,
    selector helpers, and the top-level sampling API.

### Output formatting helpers

- `common/memory.cc`
  - Converts `MemoryStatus` into one of three display modes.
  - Applies tmux color blocks and optional left-segment blending.
- `common/load.cc`
  - Reads load averages with `getloadavg()`.
  - Colors the segment relative to CPU count and supports right-edge blending.
- `common/network.cc`
  - Formats byte rates into `B/s`, `KB/s`, `MB/s`, or `GB/s`.
  - Supports both/download/upload/dynamic network modes.
  - Pads strings to reduce tmux status-width jitter.
- `common/network_sampler.cc`
  - Owns selector parsing, default filtering, snapshot matching, and rate
    aggregation.
- `common/graph.cc`
  - Generates the classic ASCII bar graph and the compact vertical graph glyph.
- `common/powerline.cc`
  - Emits tmux escape sequences and Powerline separator glyphs.
  - Handles fg/bg inversion for seamless segment transitions.

### Shared utility headers

- `common/conversions.h`
  - Tiny unit-conversion template used by memory collectors and formatters.
- `common/error.h`
  - Simple fatal-error helper used mainly by BSD collectors.
- `common/getsysctl.h`
  - Small `sysctlbyname` wrapper for BSD implementations.
- `common/luts.h`
  - Checked-in tmux color lookup tables used by CPU, memory, load, and network formatters.
- `common/generate-luts.py`
  - Regenerates `luts.h` from Matplotlib colormaps.
- `common/version.h.in`
  - CMake template for generated version macros.

## Platform-Specific Metric Collection

### Linux

- `linux/cpu.cc`
  - Samples `/proc/stat` twice with a sleep between reads, then computes total CPU usage.
- `linux/memory.cc`
  - Parses `/proc/meminfo` and approximates “used” memory by subtracting reclaimable/cache-like fields.
- `linux/network.cc`
  - Parses `/proc/net/dev`, consults `/sys/class/net/*/operstate`, and returns cumulative interface snapshots to the shared sampler.

### macOS

- `osx/cpu.cc`
  - Uses Mach host statistics to sample CPU ticks across two points in time.
- `osx/memory.cc`
  - Uses `HW_MEMSIZE`, page size, and VM statistics to compute available vs used memory.
- `osx/network.cc`
  - Uses `getifaddrs()` to expose cumulative interface counters to the shared sampler.

### FreeBSD

- `freebsd/cpu.cc`
  - Uses `kern.cp_time` via `GETSYSCTL`.
- `freebsd/memory.cc`
  - Uses VM page counters and treats active + wired pages as used memory.
- `freebsd/network.cc`
  - Uses `getifaddrs()` to expose cumulative interface counters to the shared sampler.

### OpenBSD

- `openbsd/cpu.cc`
  - Uses `sysctl` with `KERN_CPTIME`.
- `openbsd/memory.cc`
  - Uses `uvmexp` plus buffer-cache stats; includes an older OpenBSD 5.6 include-order workaround toggled by CMake.
- `openbsd/network.cc`
  - Uses `getifaddrs()` to expose cumulative interface counters to the shared sampler.

### NetBSD

- `netbsd/cpu.cc`
  - Uses `kern.cp_time` via `GETSYSCTL`.
- `netbsd/memory.cc`
  - Uses `uvmexp_sysctl` and subtracts file-backed pages from active + wired usage.
- `netbsd/network.cc`
  - Uses `getifaddrs()` to expose cumulative interface counters to the shared sampler.

### Windows

- `windows/cpu.cc`
  - Uses PDH counters for total processor time.
- `windows/memory.cc`
  - Uses `GlobalMemoryStatusEx`.
- `windows/network.cc`
  - Uses IP Helper interface tables to expose cumulative interface counters to the shared sampler.
- `windows/load.cc`
  - Provides a stub `load_string()` that returns an empty result because load averages are not implemented there.

## Build and Test Structure

### Build selection

`CMakeLists.txt` picks one platform-specific source bundle:

- Linux: `linux/memory.cc`, `linux/cpu.cc`, `linux/network.cc`, `common/load.cc`
- Darwin: `osx/memory.cc`, `osx/cpu.cc`, `osx/network.cc`, `common/load.cc`
- FreeBSD/OpenBSD/NetBSD: platform memory + CPU + `platform/network.cc` + `common/load.cc`
- Windows: `windows/memory.cc`, `windows/cpu.cc`, `windows/network.cc`, `windows/load.cc`

Common sources are always linked from:

- `common/main.cc`
- `common/memory.cc`
- `common/graph.cc`
- `common/powerline.cc`
- `common/network_sampler.cc`
- `common/network.cc`

### Test coverage

CTest cases in `CMakeLists.txt` cover:

- source-level shared network sampling via `network_logic_test`
- Linux `/proc` parsing and reader behavior via `linux_network_reader_test`
- help/usage behavior
- option parsing and validation
- network CLI smoke coverage for selector parsing and invalid mode rejection
- multiple display-mode combinations
- failure expectations for invalid arguments and removed positional syntax

GitHub Actions currently split CI coverage between:

- `.github/workflows/main.yml` cross-builds a large Linux/dockcross image matrix
- `.github/workflows/main.yml` also runs native configure/build/test jobs on Linux, macOS, Windows, FreeBSD, OpenBSD, and NetBSD so platform-specific readers are compiled on their target families

## Scripts and Integration Points

- `tmux-mem-cpu-load.plugin.tmux`
  - Checks for a local or PATH-installed binary and tries to run `cmake .` plus `cmake --build .` from the plugin directory if missing.
- `tmux-mem-cpu-load.plugin.zsh`
  - Zsh/Antigen helper that builds and installs the binary to `/usr/local/bin`.
- `builder/cmake.sh`, `builder/makefile.sh`
  - Shell entry points for dockcross-based CI or manual cross-build helpers.
- `builder/functions/cmake_fn.sh`, `builder/functions/makefile_fn.sh`
  - Pull dockcross images, generate wrapper scripts, and run CMake/Ninja or Make inside them.

## Notable Constraints and Current Hotspots

- Network selection is aggregate-by-default: when no selectors are provided, the sampler keeps only active non-loopback interfaces and hides the segment if none qualify.
- `common/main.cc` overlaps CPU and network sampling, so end-to-end latency now tracks the slower of the two sampling windows rather than their sum.
- `common/network.cc` accepts right-segment blending parameters, but the current formatter path does not use them to close the segment the way memory/load do.
- The color lookup table header is generated by script but checked into source control, so changes to color behavior may require updating both the script and the committed header.

## Suggested Starting Points for Contributors

- CLI/output changes: start in `common/main.cc`, then inspect the relevant formatter in `common/`.
- New metric formatting behavior: update the shared formatter plus any related enum/header in `common/`.
- Platform bugfixes: start in the OS directory selected by `CMakeLists.txt`.
- Build or packaging work: inspect `CMakeLists.txt`, then the tmux/plugin scripts or `builder/` helpers depending on scope.
