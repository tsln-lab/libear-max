---
title: mc.ear.record~
description: Recording an ADM file with one transport, the metadata timed by the recorded audio.
---

`mc.ear.record~` records an ADM file, audio and metadata together: the
multichannel input carries the objects' audio, a DirectSpeakers bed and an
HOA scene, and the object metadata sent to it (as to `mc.ear.objects~`) is
written as blocks timed by the recorded audio. Files over 4 GB are written
as RF64.

## Recording with one transport

The counterpart of [`mc.ear.play~`](/libear-max/adm/mc-ear-play/) for
writing: the multichannel input carries the tracks in file order, the
objects' audio first (one channel per object, as fed to `mc.ear.objects~`),
then the bed's channels (`@directchans`) and the HOA components
(`@hoaorder`), combined with `mc.combine~` when they come from different
places. The object metadata is sent to `mc.ear.record~` in the same format
as to the renderer (`setvalue n parameter`, `applyvalues`, lists, or a
parameter as a message for all objects), so the same messages can drive
both at once and the mix is heard as it is recorded; the bed and scene
metadata is given with `direct ...` and `hoa ...` as in
[`ear.adm`](/libear-max/adm/ear-adm/#beds-and-scenes), a bed
change while recording becoming a timed block. `open file.wav` names the file, `start` (or `1`) records
from the objects' current parameters, `stop` (or `0`) finishes it and
reports `written path tracks length-ms` on the outlet (`failed path` when
the file could not be written). Every change while recording becomes an
`audioBlockFormat` at the time of the audio recorded so far (the frames the
audio thread has handed over), with the ramp in force as its interpolation,
so the blocks land on the samples they belong to instead of on Max's
scheduler clock. `@chans`, `@directchans` and `@hoaorder` set the channels
written (extra input channels are ignored with a warning, missing ones are
silent), `@bitdepth` the sample format (16, 24 or 32), `@ramp` and `name n
symbol` are as in `ear.adm`. A writer thread drains a ring buffer to disk in chunks and adds
the `chna` and `axml` chunks when the recording stops, so the audio thread
never touches the disk; vectors dropped because the disk fell behind are
counted and reported in the Max console. Files over 4 GB are written as
RF64, which `mc.ear.play~` plays and `mc.sfplay~` cannot.

```
[open take.wav( [start( [stop(   [setvalue 1 azimuth 30(  [direct inputlayout 0+5+0(
|               |       |        |                        |
|               |       |        +-- also to [mc.ear.objects~] to hear it
[mc.ear.record~ @chans 16 @directchans 6] <-- [mc.combine~ 2] <-- objects audio, bed audio
|
[print]   recording / written take.wav 22 120000 / failed / position
```

To write the file in the shape of the Dolby Atmos Master ADM Profile, set
`@profile dolby`: see [Dolby Atmos masters](/libear-max/adm/dolby-atmos/#writing-a-dolby-atmos-master).
