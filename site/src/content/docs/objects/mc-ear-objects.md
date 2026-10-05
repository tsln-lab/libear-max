---
title: mc.ear.objects~ and mc.ear.direct~
description: The multichannel object and bed renderers, and how metadata is addressed per input channel.
---

`mc.ear.objects~` is the multichannel object renderer: every channel of the
multichannel input is an object with its own metadata, the multichannel output
has one channel per loudspeaker. The decorrelators run once per loudspeaker on
the shared diffuse bus. `mc.ear.direct~` is the multichannel bed renderer:
every input channel is a DirectSpeakers channel (by label or position),
rendered onto the layout.

Both take the output layout as argument and have one multichannel inlet and
one multichannel outlet. `@chans` is the number of input channels allocated,
16 by default; it grows by itself when the input signal carries more channels
or a message addresses a higher one, up to 1024, and the new channels take
effect when the audio is restarted (a message posts the new count). Metadata
is addressed per input channel in the style of Max's mc objects:

- `setvalue <n> <parameter> <values...>` sets one parameter of input `n`
  (1-based); `setvalue 0 ...` sets it for all inputs.
- `applyvalues <parameter> v1 v2 v3 ...` spreads values over inputs 1, 2, 3.
- Any parameter sent as a plain message (`diffuse 0.5`, `zone clear`) applies
  to all inputs; a list sets the position of all inputs.

## mc.ear.objects~

`mc.ear.objects~` accepts every `ear.objects` parameter
([listed here](/libear-max/objects/ear-objects/)) plus `position` and `zone`, and the `ramp`
and `decorrelate` attributes. `ramp` is also a per-object parameter
(`setvalue 3 ramp 250`): it sets the interpolation time of the next changes
of that object, until a negative value returns it to the attribute;
`ear.adm` uses it to reproduce the interpolation of each ADM block.

## mc.ear.direct~

`mc.ear.direct~` accepts `speakerlabel`, `position`, `azimuth`, `elevation`,
`distance`, `bounds`, `lfe` and `packformat`, plus `cartesian` with `x`,
`y`, `z` (then `position` is `x y z` and `bounds` are `XMin XMax YMin YMax
ZMin ZMax`, as Dolby Atmos masters give their beds), and `inputlayout <name>`, which
labels the input channels after a BS.2051 layout (LFE channels included), so
rendering a 5.1 bed to 9+10+3 is `[mc.ear.direct~ 9+10+3]` with
`inputlayout 0+5+0`.
