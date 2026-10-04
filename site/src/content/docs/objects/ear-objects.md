---
title: ear.objects and ear.objects~
description: The Objects metadata attributes shared by the control-rate gain calculator and the signal-rate renderer.
---

`ear.objects` is the control-rate gain calculator for ADM *Objects* metadata:
it outputs one list of direct gains and one of diffuse gains, one value per
loudspeaker of a BS.2051 layout. `ear.objects~` is the signal-rate object
renderer: one signal inlet, one signal outlet per loudspeaker. Gains are
interpolated; the diffuse part runs through the BS.2127 decorrelation filters
and the direct part is delay-compensated.

## Metadata

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

## Outlets

Outlets of `ear.objects`: direct gains, diffuse gains, info (`channels`,
`positions`, `layouts`). Outlets of `ear.objects~`: one signal per loudspeaker
in the layout's channel order, e.g. `M+030 M-030 M+000 LFE1 M+110 M-110` for
`0+5+0`. Feed them to `mc.pack~` / `mc.dac~` or individual `dac~` channels.
