---
title: Releases
description: Pre-built packages, and how releases are cut from conventional commits with release-please.
---

Pre-built packages are published on the
[Releases](https://github.com/tsln-lab/libear-max/releases) page. Each release
contains `libear-max-<version>.zip`, a complete Max package with the macOS
(universal) and Windows (x64) externals, help patchers and `package-info.json`;
unzip it into your `Max 9/Packages` folder. Per-platform archives are attached
too.

## Cutting a release

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

## Unsigned builds

Without the secrets described under
[Signing and notarization](/libear-max/signing/), the macOS externals are ad-hoc signed
but not notarized. If Max refuses to load a downloaded package, clear the
quarantine flag once:

```sh
xattr -dr com.apple.quarantine "~/Documents/Max 9/Packages/libear-max"
```
