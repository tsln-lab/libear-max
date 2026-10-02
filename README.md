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
| `mc.ear.objects~` | Multichannel object renderer: every channel of the multichannel input is an object with its own metadata, the multichannel output has one channel per loudspeaker. The decorrelators run once per loudspeaker on the shared diffuse bus. |
| `mc.ear.direct~` | Multichannel bed renderer: every input channel is a DirectSpeakers channel (by label or position), rendered onto the layout. `inputlayout 0+5+0` labels the input after a BS.2051 layout. |

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
| `screenref` | `0` | screen scaling relative to the reference screen |
| `screenedgelock_h`, `screenedgelock_v` | `none` | lock to a screen edge: `left`/`right`, `top`/`bottom` |
| `autocalc` (`ear.objects` only) | `1` | output gains on every change; otherwise send `bang` |
| `ramp` (`ear.objects~` only) | `10` | gain interpolation time in ms |
| `decorrelate` (`ear.objects~` only) | `1` | use the decorrelation filters (adds 255 samples of latency) |

Zone exclusion is set with messages: `zone polar minAz maxAz minEl maxEl`,
`zone cartesian minX maxX minY maxY minZ maxZ` (repeat to add zones) and
`zone clear`.

Outlets of `ear.objects`: direct gains, diffuse gains, info (`channels`,
`positions`, `layouts`). Outlets of `ear.objects~`: one signal per loudspeaker
in the layout's channel order, e.g. `M+030 M-030 M+000 LFE1 M+110 M-110` for
`0+5+0`. Feed them to `mc.pack~` / `mc.dac~` or individual `dac~` channels.

### mc.ear.objects~ and mc.ear.direct~

Both take the output layout as argument and have one multichannel inlet and
one multichannel outlet. `@chans` sets the maximum number of input channels
(default 16; extra input channels are ignored with a warning). Metadata is
addressed per input channel in the style of Max's mc objects:

- `setvalue <n> <parameter> <values...>` sets one parameter of input `n`
  (1-based); `setvalue 0 ...` sets it for all inputs.
- `applyvalues <parameter> v1 v2 v3 ...` spreads values over inputs 1, 2, 3.
- Any parameter sent as a plain message (`diffuse 0.5`, `zone clear`) applies
  to all inputs; a list sets the position of all inputs.

`mc.ear.objects~` accepts every `ear.objects` parameter listed above plus
`position` and `zone`, and the `ramp` and `decorrelate` attributes.
`mc.ear.direct~` accepts `speakerlabel`, `position`, `azimuth`, `elevation`,
`distance`, `bounds`, `lfe` and `packformat`, plus `inputlayout <name>`, which
labels the input channels after a BS.2051 layout (LFE channels included), so
rendering a 5.1 bed to 9+10+3 is `[mc.ear.direct~ 9+10+3]` with
`inputlayout 0+5+0`.

### ear.direct

Attributes `layout`, `azimuth`, `elevation`, `distance`, `lfe` (marks the
channel as LFE), `packformat` (optional `audioPackFormatID` for pack-specific
mapping rules) and `autocalc`. Messages: `speakerlabel M+030 ...` (labels take
precedence; send with no arguments to clear), `bounds azMin azMax elMin elMax
[distMin distMax]`, a list `azimuth elevation [distance]`, and `bang`.

## Reference parity

The goal is to match the reference renderer, the EBU ADM Renderer
([`ear`](https://github.com/ebu/ebu_adm_renderer)), as closely as possible.
Upstream libear implements the polar Objects path, DirectSpeakers and HOA but
not every Objects parameter, so this package builds against a fork,
[tsln-lab/libear](https://github.com/tsln-lab/libear) (branch
`reference-parity`), where the missing features have been ported from the
Python reference. Every port is validated in the fork against gains generated
from the reference implementation: 1,906 Objects block formats (point sources,
extent, channel lock, divergence, Cartesian, zone exclusion, screen scaling,
screen edge lock, wide screen loudspeakers) and 2,557 DirectSpeakers block
formats (labels, URNs, LFE, bounds, Cartesian, screen edge lock, common
definition packs) across the BS.2051 layouts.

| Feature | Status |
| --- | --- |
| polar position, extent (width/height/depth), gain, diffuse | matches reference (upstream libear) |
| `channellock`, `channellock_distance` | matches reference (ported) |
| `divergence`, `divergence_range`, polar and Cartesian | matches reference (ported) |
| `cartesian`, `x`, `y`, `z`: allocentric panning and Cartesian extent | matches reference (ported) |
| `zone polar ...` / `zone cartesian ...` / `zone clear` messages | matches reference (ported) |
| `screenref`, `screenedgelock_h`, `screenedgelock_v` | matches reference (ported) |
| `M+SC`/`M-SC` loudspeakers wider than 25° | matches reference (ported) |
| DirectSpeakers: Cartesian positions and bounds, screen edge lock | matches reference (ported) |
| HOA `screenRef` and `nfcRefDist` | not implemented in libear (warnings, as upstream) |

Gains match to 1e-15, or about 1e-7 where libear's single-precision extent
panner is involved. The parity data and the generator scripts live in the
fork under `tools/reference/` and `tests/reference/`. Cartesian rendering is
only defined for the BS.2051 layouts, which have allocentric loudspeaker
positions; custom layouts render in polar mode only.

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

**Windows** (Visual Studio 2022 or later, Boost from vcpkg via the manifest in `vcpkg.json`):

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
up `externals/` and `help/` directly.

## Releases

Pre-built packages are published on the
[Releases](https://github.com/tsln-lab/libear-max/releases) page. Each release
contains `libear-max-<version>.zip`, a complete Max package with the macOS
(universal) and Windows (x64) externals, help patchers and `package-info.json`;
unzip it into your `Max 9/Packages` folder. Per-platform archives are attached
too.

Releases are cut with [release-please](https://github.com/googleapis/release-please)
from [conventional commits](https://www.conventionalcommits.org/):

1. Commit messages and pull request titles use the conventional format:
   `feat: ...` for a new feature (minor version), `fix: ...` for a bug fix
   (patch version), `feat!: ...` or a `BREAKING CHANGE:` footer for a breaking
   change. `docs:`, `ci:`, `chore:`, `refactor:`, `test:`, `build:` and
   `perf:` are accepted and do not trigger a release on their own (`docs:`
   and `perf:` are listed in the changelog). The *PR title* workflow checks
   the title, which becomes the commit message when a pull request is
   squash-merged; with a merge commit, release-please reads the individual
   commits of the branch instead, so those need the format too.
2. On every push to `main`, the *Release Please* workflow keeps a release pull
   request up to date that bumps `version.txt`, updates `CHANGELOG.md` and
   shows the release notes.
3. Merging that pull request creates the `vX.Y.Z` tag and the GitHub release,
   and dispatches the *Build* workflow on the tag, which builds and tests on
   macOS, Windows and Linux and attaches the packages to the release. The
   version in `package-info.json` is taken from the tag.

The first release pull request opens once a `feat:` or `fix:` commit lands on
`main`; a `Release-As: 1.2.3` commit footer forces a specific version. Pushing
a `v*` tag by hand still works and produces a release the same way. While the
version is below 1.0.0, breaking changes bump the minor version and features
bump the patch version. Release Please needs the repository setting
*Actions → General → Allow GitHub Actions to create and approve pull requests*
enabled.

Without the secrets described below, the macOS externals are ad-hoc signed
but not notarized. If Max refuses to load a downloaded package, clear the
quarantine flag once:

```sh
xattr -dr com.apple.quarantine "~/Documents/Max 9/Packages/libear-max"
```

### Signing and notarization (macOS)

The workflow signs the `.mxo` bundles with a Developer ID certificate, submits
them to Apple's notary service and staples the tickets whenever the repository
has the secrets below. Without them every signing step is skipped and the
build is ad-hoc signed. Windows externals are not code-signed.

Prerequisites on the Apple side (a paid Apple Developer Program membership is
required for both):

1. **Developer ID Application certificate.** In Xcode, *Settings → Accounts →
   Manage Certificates → + → Developer ID Application* (or create it at
   developer.apple.com/account/resources/certificates). Then export it from
   Keychain Access as a `.p12` with a password: open *My Certificates*, select
   the certificate together with its private key, *File → Export Items*.
   The certificate's name is the signing identity, e.g.
   `Developer ID Application: Your Name (TEAMID1234)`; check it with
   `security find-identity -v -p codesigning`.
2. **App Store Connect API key for notarization.** At
   appstoreconnect.apple.com → *Users and Access → Integrations → App Store
   Connect API → Team Keys*, generate a key with the *Developer* role. Note the
   *Key ID* and the *Issuer ID*, and download the `AuthKey_<KEYID>.p8` file
   (it can only be downloaded once).

Then add these repository secrets (*Settings → Secrets and variables →
Actions*):

| Secret | Value |
| --- | --- |
| `APPLE_SIGNING_IDENTITY` | the certificate name, e.g. `Developer ID Application: Your Name (TEAMID1234)` |
| `APPLE_CERTIFICATE_P12` | the `.p12` file, base64-encoded: `base64 -i certificate.p12 \| pbcopy` |
| `APPLE_CERTIFICATE_PASSWORD` | the password chosen when exporting the `.p12` |
| `APPLE_API_KEY_ID` | the App Store Connect API *Key ID* |
| `APPLE_API_ISSUER_ID` | the App Store Connect API *Issuer ID* |
| `APPLE_API_KEY_P8` | the full contents of `AuthKey_<KEYID>.p8` (paste the text as is) |

What the workflow then does, in `.github/workflows/build.yml`:

1. Imports the certificate into a temporary keychain that is deleted at the
   end of the job.
2. Re-signs each `.mxo` with `codesign --options runtime --timestamp`, which
   replaces the ad-hoc signature from the build. The hardened runtime and a
   secure timestamp are both required by the notary service.
3. Zips the externals, submits them with `xcrun notarytool submit --wait`,
   fails the job (and prints the notary log) if the submission is not
   accepted, and staples the resulting tickets to the bundles with
   `xcrun stapler staple`.
4. Packages the stapled bundles; the release notes say whether the build was
   notarized.

To test the setup before tagging a release, run the workflow manually from
the *Actions* tab (it has a `workflow_dispatch` trigger) and check the
"Notarize and staple externals" step. Locally you can verify a downloaded
bundle with:

```sh
codesign --verify --strict --verbose=2 ear.objects.mxo
xcrun stapler validate ear.objects.mxo
```

## Repository layout

```
CMakeLists.txt                  package build (Min-DevKit style) + libear
package-info.json.in            Max package manifest template
source/min-api/                 Min C++ API for Max (submodule; brings max-sdk-base and the mock kernel)
source/libear/                  libear fork with reference-parity ports (submodule; bundles Eigen, xsimd, KISS FFT)
source/projects/shared/         ear_max.h: layout helpers and the Objects metadata attribute base
source/projects/ear.objects/    ear.objects  (class in .h, registration in .cpp, Catch tests in _test.cpp)
source/projects/ear.direct/     ear.direct
source/projects/ear.objects_tilde/  ear.objects~
source/projects/mc.ear.objects_tilde/  mc.ear.objects~ (multichannel)
source/projects/mc.ear.direct_tilde/   mc.ear.direct~ (multichannel)
source/projects/shared/ear_max_dsp.h   bus_renderer: the shared N-in / L-out gain matrix with ramps and decorrelation
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
  messages are deferred there by Min). The signal objects share one DSP core
  (`bus_renderer`) that mixes N inputs onto L loudspeakers; new gain vectors
  reach the audio thread through a mutex that the audio thread only
  `try_lock`s, and are ramped linearly over `@ramp` milliseconds per input.
- The multichannel outlets use the Max C API directly (`multichanneloutputs`
  and `inputchanged`, registered from `maxclass_setup`), since Min only
  supports multichannel inlets.
- The decorrelation path uses libear's partitioned block convolver at the
  current signal vector size and its delay buffer for the direct path, exactly
  as described in `ear::GainCalculatorObjects`.
- On universal macOS builds libear's x86-specific SIMD kernels are disabled
  (`EAR_SIMD=OFF`), because their compiler flags are not valid for the arm64
  slice; the portable code path is used instead.

## Roadmap

- Upstream the reference parity work to `ebu/libear`.
- Custom reproduction screens (currently the default screen is used for
  `screenref` and screen edge lock).
- `ear.hoa` / `mc.ear.hoa~`: HOA decoding (`ear::GainCalculatorHOA`).
- Reading and writing ADM/BW64 files (libadm, libbw64) in a later phase.
- Reference pages generated from the Min descriptions.

## License

libear-max is released under the MIT License. libear is Apache-2.0, Min-API
and max-sdk-base are MIT; see [License.md](License.md).
