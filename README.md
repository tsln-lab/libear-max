# libear-max

Max externals for rendering [ADM](https://adm.ebu.io/) audio objects with
[libear](https://github.com/ebu/libear), the EBU's C++ implementation of the
ITU-R BS.2127 renderer (EAR). Built as a [Min-DevKit](https://github.com/Cycling74/min-devkit)
package, so it produces a regular Max package with externals, help patchers
and unit tests.

**Documentation: <https://tsln-lab.github.io/libear-max/>** (the pages are the
Markdown files under [`site/src/content/docs/`](site/src/content/docs/)).

## Objects

| Object          | What it does |
| --------------- | ------------ |
| `ear.objects`   | Control-rate gain calculator for ADM *Objects* metadata. Outputs one list of direct gains and one of diffuse gains, one value per loudspeaker of a BS.2051 layout. |
| `ear.objects~`  | Signal-rate object renderer: one signal inlet, one signal outlet per loudspeaker. Gains are interpolated; the diffuse part runs through the BS.2127 decorrelation filters and the direct part is delay-compensated. |
| `ear.direct`    | Control-rate gain calculator for ADM *DirectSpeakers* channels: maps a channel by speaker label (e.g. `M+030`) or by nominal position and bounds onto a layout. |
| `mc.ear.objects~` | Multichannel object renderer: every channel of the multichannel input is an object with its own metadata, the multichannel output has one channel per loudspeaker. The decorrelators run once per loudspeaker on the shared diffuse bus. |
| `mc.ear.direct~` | Multichannel bed renderer: every input channel is a DirectSpeakers channel (by label or position), rendered onto the layout. `inputlayout 0+5+0` labels the input after a BS.2051 layout. |
| `mc.ear.hoa~` | Multichannel ambisonic (ADM *HOA*) decoder: the input carries the ambisonic components in ACN order, the output one channel per loudspeaker, decoded with the EAR's AllRAD design for the layout. |
| `ear.hoa` | Control-rate counterpart of `mc.ear.hoa~`: outputs the decoding matrix for a layout, order and normalization as lists, one per ambisonic component, and as `matrix~` messages. |
| `mc.ear.encode~` | Multichannel ambisonic encoder, the counterpart of `mc.ear.objects~`: every input channel is an object positioned with the same messages, the output carries the summed ambisonic components in ACN order. |
| `ear.adm` | Reads and writes ADM files (BW64 with ADM metadata): resolves a file's rendering items like the EAR, plays its object metadata to the renderers with the reference's interpolation rules, and captures object messages to write them with recorded audio as a new ADM file. |
| `mc.ear.select~` | Picks channels of a multichannel signal by number, to route the tracks of a file (as `ear.adm` reports them) to the renderer that handles them. |
| `mc.ear.play~` | Plays an ADM file, audio and metadata together: the audio is streamed from disk to three multichannel outlets already routed for the three renderers, and the metadata is emitted from the audio clock. Plays files `mc.sfplay~` cannot open (RF64/BW64 over 4 GB) and converts a file at another sample rate to the audio's. |
| `mc.ear.record~` | Records an ADM file, audio and metadata together: the multichannel input carries the objects' audio, a DirectSpeakers bed and an HOA scene, and the object metadata sent to it (as to `mc.ear.objects~`) is written as blocks timed by the recorded audio. Writes files over 4 GB as RF64. |

All objects take a BS.2051 layout name as argument (`0+2+0`, `0+5+0`, `2+5+0`,
`4+5+0`, `4+5+1`, `3+7+0`, `4+9+0`, `9+10+3`, `0+7+0`, `4+7+0`); the default is
`0+5+0`. Each object is described on its own page of the documentation, with
the [ADM file objects](https://tsln-lab.github.io/libear-max/adm/ear-adm/),
[Dolby Atmos masters](https://tsln-lab.github.io/libear-max/adm/dolby-atmos/)
and [reference parity](https://tsln-lab.github.io/libear-max/reference-parity/)
on pages of their own.

## Installing

Pre-built packages are published on the
[Releases](https://github.com/tsln-lab/libear-max/releases) page: unzip
`libear-max-<version>.zip` into your `Max 9/Packages` folder. See
[Releases](https://tsln-lab.github.io/libear-max/releases/) for how releases
are cut and signed.

## Building

Requirements: CMake 3.19+, a C++17 compiler, git, and the Boost headers
needed by libear and libadm.

```sh
git clone --recursive https://github.com/tsln-lab/libear-max.git
cd libear-max
cmake --preset macos      # or windows (with $env:VCPKG_ROOT set), or linux
cmake --build --preset macos
ctest --preset macos
```

The externals land in `externals/`, the test binaries in `tests/`. To use the
package in Max, clone (or symlink) the repository folder into
`~/Documents/Max 9/Packages/` and build. The details per platform, the
[repository layout](https://tsln-lab.github.io/libear-max/development/repository-layout/),
the [design notes](https://tsln-lab.github.io/libear-max/development/design-notes/)
and the [check against the EBU's ADM test files](https://tsln-lab.github.io/libear-max/adm/test-corpus/)
are in the documentation.

## Documentation site

The site is built with [Astro](https://astro.build/) and
[Starlight](https://starlight.astro.build/) from `site/` and published to
GitHub Pages by the *Docs* workflow on every push to `main`:

```sh
cd site
npm install
npm run dev        # http://localhost:4321/libear-max/
```

## License

libear-max is released under the MIT License. libear is Apache-2.0, Min-API
and max-sdk-base are MIT; see [License.md](License.md).
