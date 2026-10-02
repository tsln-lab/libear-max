# Changelog

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
