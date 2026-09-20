# libear-max

Max externals for rendering [ADM](https://adm.ebu.io/) audio objects with
[libear](https://github.com/ebu/libear), the EBU's C++ implementation of the
ITU-R BS.2127 renderer (EAR). Built as a [Min-DevKit](https://github.com/Cycling74/min-devkit)
package, so it produces a regular Max package with externals, help patchers
and unit tests.

## Objects

| Object          | What it does |
| --------------- | ------------ |
| `ear.objects`   | Control-rate gain calculator for ADM *Objects* metadata. Outputs one list of direct gains and one of diffuse gains, one value per loudspeaker of a BS.2051 layout. |
| `ear.objects~`  | Signal-rate object renderer: one signal inlet, one signal outlet per loudspeaker. Gains are interpolated; the diffuse part runs through the BS.2127 decorrelation filters and the direct part is delay-compensated. |
| `ear.direct`    | Control-rate gain calculator for ADM *DirectSpeakers* channels: maps a channel by speaker label (e.g. `M+030`) or by nominal position and bounds onto a layout. |

All objects take a BS.2051 layout name as argument (`0+2+0`, `0+5+0`, `2+5+0`,
`4+5+0`, `4+5+1`, `3+7+0`, `4+9+0`, `9+10+3`, `0+7+0`, `4+7+0`); the default is
`0+5+0`. Send `layouts` to list the names, `channels` for the channel order of
the current layout, and `positions` for the nominal loudspeaker positions.

### ear.objects / ear.objects~ metadata

Every field of the ADM `audioBlockFormat` that libear supports is an
attribute, so it can be set from messages (`azimuth 30`), from the inspector,
or with `@azimuth 30` in the box. A list `azimuth elevation [distance]` sets the
position in one go (`x y z` when `@cartesian 1`).

| Attribute | Default | Meaning |
| --- | --- | --- |
| `layout` | `0+5+0` | BS.2051 layout (fixed at creation for `ear.objects~`) |
| `azimuth` `elevation` `distance` | `0 0 1` | polar position (degrees, anticlockwise is positive) |
| `x` `y` `z` | `0 1 0` | cartesian position, used when `cartesian` is on |
| `cartesian` | `0` | select cartesian coordinates and divergence |
| `width` `height` `depth` | `0 0 0` | object extent |
| `gain` | `1` | linear gain |
| `diffuse` | `0` | diffuseness, 0..1 |
| `channellock` `channellock_distance` | `0 0` | snap to the nearest loudspeaker (0 distance = unlimited) |
| `divergence` `divergence_range` | `0 45` | object divergence and its range |
| `screenref` | `0` | screen scaling with the default reference screen |
| `autocalc` (`ear.objects` only) | `1` | output gains on every change; otherwise send `bang` |
| `ramp` (`ear.objects~` only) | `10` | gain interpolation time in ms |
| `decorrelate` (`ear.objects~` only) | `1` | use the decorrelation filters (adds 255 samples of latency) |

Outlets of `ear.objects`: direct gains, diffuse gains, info (`channels`,
`positions`, `layouts`). Outlets of `ear.objects~`: one signal per loudspeaker
in the layout's channel order, e.g. `M+030 M-030 M+000 LFE1 M+110 M-110` for
`0+5+0`. Feed them to `mc.pack~` / `mc.dac~` or individual `dac~` channels.

### ear.direct

Attributes `layout`, `azimuth`, `elevation`, `distance`, `lfe` (marks the
channel as LFE), `packformat` (optional `audioPackFormatID` for pack-specific
mapping rules) and `autocalc`. Messages: `speakerlabel M+030 ...` (labels take
precedence; send with no arguments to clear), `bounds azMin azMax elMin elMax
[distMin distMax]`, a list `azimuth elevation [distance]`, and `bang`.

## Building

Requirements: CMake 3.19+, a C++17 compiler, git, and the Boost headers
(`optional`, `variant`, `math`, `algorithm`, `smart_ptr`) needed by libear.

```sh
git clone --recursive https://github.com/tsln-lab/libear-max.git
cd libear-max
```

**macOS** (Xcode 10+, universal x86_64/arm64 with Xcode 12+):

```sh
brew install boost
cmake --preset macos
cmake --build --preset macos
ctest --preset macos
```

**Windows** (Visual Studio 2022, Boost from vcpkg via the manifest in `vcpkg.json`):

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
cmake --preset windows
cmake --build --preset windows
ctest --preset windows
```

**Linux** (no Max, but the unit tests run against Min's mock kernel):

```sh
sudo apt-get install libboost-dev ninja-build
cmake --preset linux
cmake --build --preset linux
ctest --preset linux
```

The externals land in `externals/`, the test binaries in `tests/`. To use the
package in Max, clone (or symlink) the repository folder into
`~/Documents/Max 9/Packages/` (or the Max 8 equivalent) and build; Max picks
up `externals/` and `help/` directly. The `Build` GitHub Actions workflow also
uploads ready-made macOS and Windows packages as artifacts.

## Repository layout

```
CMakeLists.txt                  package build (Min-DevKit style) + libear
package-info.json.in            Max package manifest template
source/min-api/                 Min C++ API for Max (submodule; brings max-sdk-base and the mock kernel)
source/libear/                  libear (submodule; bundles Eigen, xsimd, KISS FFT)
source/projects/shared/         ear_max.h: layout helpers and the Objects metadata attribute base
source/projects/ear.objects/    ear.objects  (class in .h, registration in .cpp, Catch tests in _test.cpp)
source/projects/ear.direct/     ear.direct
source/projects/ear.objects_tilde/  ear.objects~
help/                           help patchers
```

Adding an object: create `source/projects/<name>/` with a `CMakeLists.txt`
copied from an existing project, a `<name>.h`/`<name>.cpp` pair and, optionally,
`<name>_test.cpp`. Folders are picked up automatically; `_tilde` in a folder
name becomes `~` in the external's name.

## Design notes

- libear is compiled as a static library and linked into each external, so
  the externals have no runtime dependencies beyond Max.
- Gain calculation happens on Max's main thread (attribute setters and
  messages are deferred there by Min). `ear.objects~` hands new gain vectors
  to the audio thread through a mutex that the audio thread only `try_lock`s,
  then ramps to them linearly over `@ramp` milliseconds.
- The decorrelation path uses libear's partitioned block convolver at the
  current signal vector size and its delay buffer for the direct path, exactly
  as described in `ear::GainCalculatorObjects`.
- On universal macOS builds libear's x86-specific SIMD kernels are disabled
  (`EAR_SIMD=OFF`), because their compiler flags are not valid for the arm64
  slice; the portable code path is used instead.

## Roadmap

- `ear.hoa`: decode matrices for HOA metadata (`ear::GainCalculatorHOA`).
- Zone exclusion and custom reference screens for Objects metadata.
- An `mc.ear.objects~` variant with a single multichannel outlet.
- Reference pages generated from the Min descriptions.

## License

libear-max is released under the MIT License. libear is Apache-2.0, Min-API
and max-sdk-base are MIT; see [License.md](License.md).
