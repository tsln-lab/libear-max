---
title: Building
description: Building the package from source on macOS, Windows and Linux.
---

Requirements: CMake 3.19+, a C++17 compiler, git, and the Boost headers
needed by libear (`optional`, `variant`, `math`, `algorithm`, `smart_ptr`)
and libadm (`range`, `rational`, `iterator`, `functional`, `integer`).

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

For the layout of the sources and how to add an object, see
[Repository layout](/libear-max/development/repository-layout/).
