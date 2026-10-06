---
title: ear.direct and mc.ear.direct~
description: The DirectSpeakers gain calculator and the multichannel bed renderer.
---

`ear.direct` is the control-rate gain calculator for ADM *DirectSpeakers*
channels: it maps a channel by speaker label (e.g. `M+030`) or by nominal
position and bounds onto a layout. `mc.ear.direct~` is the multichannel bed
renderer: every input channel is a DirectSpeakers channel (by label or
position), rendered onto the layout.

## ear.direct

Attributes `layout`, `azimuth`, `elevation`, `distance`, `lfe` (marks the
channel as LFE), `packformat` (optional `audioPackFormatID` for pack-specific
mapping rules) and `autocalc`. Messages: `speakerlabel M+030 ...` (labels take
precedence; send with no arguments to clear), `bounds azMin azMax elMin elMax
[distMin distMax]`, a list `azimuth elevation [distance]`, and `bang`.

## mc.ear.direct~

Like [`mc.ear.objects~`](/libear-max/objects/ear-objects/#mcearobjects),
`mc.ear.direct~` takes the output layout as argument, has one multichannel
inlet and one multichannel outlet, grows its `@chans` with the input, and
addresses metadata per input channel with `setvalue`, `applyvalues` and plain
messages.

It accepts `speakerlabel`, `position`, `azimuth`, `elevation`,
`distance`, `bounds`, `lfe` and `packformat`, plus `cartesian` with `x`,
`y`, `z` (then `position` is `x y z` and `bounds` are `XMin XMax YMin YMax
ZMin ZMax`, as Dolby Atmos masters give their beds), and `inputlayout <name>`, which
labels the input channels after a BS.2051 layout (LFE channels included), so
rendering a 5.1 bed to 9+10+3 is `[mc.ear.direct~ 9+10+3]` with
`inputlayout 0+5+0`.
