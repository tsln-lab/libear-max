---
title: Reference parity
description: How closely the externals match the EBU ADM Renderer, and what is validated against it.
---

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
