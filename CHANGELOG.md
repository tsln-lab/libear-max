# Changelog

## [1.2.0](https://github.com/tsln-lab/libear-max/compare/v1.1.0...v1.2.0) (2026-10-04)


### Features

* check the ADM reader and writer against the EBU's test files ([#24](https://github.com/tsln-lab/libear-max/issues/24)) ([d1a4d4e](https://github.com/tsln-lab/libear-max/commit/d1a4d4ec917fdbc2b61a1710d2f223a1f9e836b1))
* **corpus:** add Netflix's Dolby Atmos master excerpts to the corpus ([#27](https://github.com/tsln-lab/libear-max/issues/27)) ([3530223](https://github.com/tsln-lab/libear-max/commit/353022325ed788307bd056d1400052c14de8dd00))
* **corpus:** list a file's chunks and summarise track renumbering ([#25](https://github.com/tsln-lab/libear-max/issues/25)) ([58b7064](https://github.com/tsln-lab/libear-max/commit/58b70648df1db9afd5cc8ccecc532ce1186351fb))
* Dolby Atmos beds are read, with Cartesian DirectSpeakers positions ([#20](https://github.com/tsln-lab/libear-max/issues/20)) ([81983ac](https://github.com/tsln-lab/libear-max/commit/81983acec9e512e9248e6338a88fd2db92616ed8))
* write Dolby Atmos masters with the profile attribute ([#21](https://github.com/tsln-lab/libear-max/issues/21)) ([e4180c5](https://github.com/tsln-lab/libear-max/commit/e4180c5ae6f6f0a1468c7b890adf4dd7102a7ae9))

## [1.1.0](https://github.com/tsln-lab/libear-max/compare/v1.0.0...v1.1.0) (2026-10-03)


### Features

* add ear.hoa, the HOA decoding matrix at control rate ([#19](https://github.com/tsln-lab/libear-max/issues/19)) ([8429608](https://github.com/tsln-lab/libear-max/commit/842960846edcd7de7537400768f90677eee7755b))
* add mc.ear.encode~, an ambisonic encoder with the mc.ear.objects~ interface ([#10](https://github.com/tsln-lab/libear-max/issues/10)) ([f54e191](https://github.com/tsln-lab/libear-max/commit/f54e1917903c901dce47a8a0b97116fdd46f9b19))
* DirectSpeakers beds and HOA scenes are written by ear.adm and mc.ear.record~ ([#15](https://github.com/tsln-lab/libear-max/issues/15)) ([5e01350](https://github.com/tsln-lab/libear-max/commit/5e01350ff224163092f183bfb1723c73fef5eafb))
* ear.adm reads and writes ADM files, mc.ear.select~ routes file tracks ([#12](https://github.com/tsln-lab/libear-max/issues/12)) ([f54e191](https://github.com/tsln-lab/libear-max/commit/f54e1917903c901dce47a8a0b97116fdd46f9b19))
* mc.ear.play~ converts a file at another sample rate to the audio's ([#16](https://github.com/tsln-lab/libear-max/issues/16)) ([69c99dd](https://github.com/tsln-lab/libear-max/commit/69c99ddb214a0e7cab34cccae6bcc41491cd9828))
* mc.ear.play~ plays an ADM file, audio and metadata together ([#13](https://github.com/tsln-lab/libear-max/issues/13)) ([f54e191](https://github.com/tsln-lab/libear-max/commit/f54e1917903c901dce47a8a0b97116fdd46f9b19))
* mc.ear.record~ records an ADM file, audio and metadata together ([#14](https://github.com/tsln-lab/libear-max/issues/14)) ([460b5f6](https://github.com/tsln-lab/libear-max/commit/460b5f632bb7a7f5ae2060a774a684fd8858d55a))
* timed DirectSpeakers blocks are read, played and captured ([f54e191](https://github.com/tsln-lab/libear-max/commit/f54e1917903c901dce47a8a0b97116fdd46f9b19))
* zone exclusion is read from, played from and written to ADM files ([#18](https://github.com/tsln-lab/libear-max/issues/18)) ([98e1402](https://github.com/tsln-lab/libear-max/commit/98e14029ae7837c37f195b984b9a6392c2cab714))

## 1.0.0 (2026-10-02)


### Features

* add mc.ear.hoa~, an ambisonic (ADM HOA) decoder ([c22c9b9](https://github.com/tsln-lab/libear-max/commit/c22c9b961f9b3159379c5eed95d252a2674f1f0b))
* add mc.ear.hoa~, an ambisonic (ADM HOA) decoder ([dace06f](https://github.com/tsln-lab/libear-max/commit/dace06f4b63a79755d6f7a3b6bae4518c99d4c4d))
* align mc.ear.direct~ with the object renderers' latency ([1973e91](https://github.com/tsln-lab/libear-max/commit/1973e9158e9830bcfc5740354c9e4a95eb88bf1e))
* align mc.ear.direct~ with the object renderers' latency ([03aa578](https://github.com/tsln-lab/libear-max/commit/03aa57872602d9ab94fde0a3118d8c179daec94f))
* list settable attributes as messages in the reference pages ([6152ad2](https://github.com/tsln-lab/libear-max/commit/6152ad2d00e96784ee35c5218f2794c63f647d9b))


### Bug Fixes

* avoid an ambiguous symbol to string cast in the reference page generator ([a8cef5c](https://github.com/tsln-lab/libear-max/commit/a8cef5ca476ad322fb2d516c2c79e3b7b90a15ae))
* do not use std::filesystem in the reference page test ([0817da8](https://github.com/tsln-lab/libear-max/commit/0817da8789832cd584ae1a0a8cc0e80e73412df9))
* keep the compensation delay running while alignment is bypassed ([8f55468](https://github.com/tsln-lab/libear-max/commit/8f55468a4b09613b90ee2855af73f440b75f498a))
* silence dropped components when the HOA order is lowered while running ([24bef8f](https://github.com/tsln-lab/libear-max/commit/24bef8f74b40e7df9ace87780ac5279376ee6f36))
