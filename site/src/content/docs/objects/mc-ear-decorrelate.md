---
title: mc.ear.decorrelate~
description: The BS.2127 decorrelation filters of libear as a multichannel effect, seeded per loudspeaker or per channel.
---

`mc.ear.decorrelate~` runs every channel of a multichannel signal through one
of the EAR's decorrelation filters, the ones the object renderers apply to
the diffuse part of an object. It is the way to widen a signal the renderers
do not decorrelate, a DirectSpeakers bed from
[`mc.ear.direct~`](/libear-max/objects/ear-direct/#mceardirect) or a source
panned by hand with the gains of `ear.objects`, and the way to get any number
of mutually decorrelated channels for other uses.

## The filters

ITU-R BS.2127 section 7.4 decorrelates with one all-pass FIR filter per
loudspeaker: 512 taps designed in the frequency domain with unit magnitude
and a random phase in every bin, so the filter changes the phase of each
frequency and nothing else. The randomness is fixed by the standard: the
phases come from a seeded generator, and the seed of a loudspeaker is its
rank in the alphabetical order of the layout's channel names, so the same
layout always gets the same filters and a render matches the reference
renderer sample for sample. Two loudspeakers fed the same signal through
their filters receive copies with different phase patterns, which is what
breaks the correlation between them. The filters have a group delay of 255
samples, which is why the EAR delays the direct path by that much.

## Seeding

With a BS.2051 layout as argument, `[mc.ear.decorrelate~ 4+5+0]` has the
layout's channel count and channel *n* gets the filter of loudspeaker *n*,
the same one `mc.ear.objects~ 4+5+0` applies on that loudspeaker. A bed
rendered by `mc.ear.direct~ 4+5+0` into it therefore sums coherently with
diffuse objects rendered the EAR way. Without a layout, channel *n* gets the
filter seeded by *n* and the count comes from `@chans` (2 by default), which
grows by itself when the input carries more channels, up to 1024; a new
count or layout takes effect when the audio is restarted. `channels` posts
which filter each channel gets.

## Mixing with the direct signal

`@diffuse` sets the share of the decorrelated signal in the output, 1 by
default. The split is power-preserving, as for an object's `diffuse`
parameter: the decorrelated signal is scaled by the square root of
`diffuse` and the direct share by the square root of `1 - diffuse`. The
direct share is delayed by the filters' compensation delay of 255 samples
so that both stay time-aligned, as the EAR's renderers delay their direct
path; `@align 0` passes it with no delay instead. Both paths always run, so
changing the mix never leaves one without history. With `@diffuse 0` the
object is a plain 255-sample delay, which is also how to align anything else
with a decorrelating renderer.

```
[mc.ear.direct~ 4+5+0]          [mc.ear.objects~ 4+5+0]
|                                |
[mc.ear.decorrelate~ 4+5+0 @diffuse 0.5]
|                                |
[mc.+~]--------------------------+
|
[mc.dac~]
```

Each channel costs one 512-tap partitioned convolution, the same as a
loudspeaker of the object renderers.
