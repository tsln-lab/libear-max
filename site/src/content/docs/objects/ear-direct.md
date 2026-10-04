---
title: ear.direct
description: The control-rate gain calculator for ADM DirectSpeakers channels.
---

`ear.direct` is the control-rate gain calculator for ADM *DirectSpeakers*
channels: it maps a channel by speaker label (e.g. `M+030`) or by nominal
position and bounds onto a layout.

Attributes `layout`, `azimuth`, `elevation`, `distance`, `lfe` (marks the
channel as LFE), `packformat` (optional `audioPackFormatID` for pack-specific
mapping rules) and `autocalc`. Messages: `speakerlabel M+030 ...` (labels take
precedence; send with no arguments to clear), `bounds azMin azMax elMin elMax
[distMin distMax]`, a list `azimuth elevation [distance]`, and `bang`.

For the multichannel bed renderer, which takes the same channel description
per input channel, see [mc.ear.direct~](/libear-max/objects/mc-ear-objects/#mceardirect).
