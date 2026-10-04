---
title: ear.adm and mc.ear.select~
description: Reading and writing ADM files with ear.adm, and routing a file's tracks to the renderers with mc.ear.select~.
---

`ear.adm` reads and writes ADM files, the BW64 (`.wav`) files with ADM
metadata that the EAR renders. It handles the metadata only; the audio goes
through Max's own file objects, which keeps everything sample-accurate and
lets you use any player or recorder. `mc.ear.select~` picks channels of a
multichannel signal by number, to route the tracks of a file (as `ear.adm`
reports them) to the renderer that handles them.

To play or record a file with a single transport that carries the audio and
the metadata together, see [mc.ear.play~](/libear-max/adm/mc-ear-play/) and
[mc.ear.record~](/libear-max/adm/mc-ear-record/). Dolby Atmos master files have
[their own page](/libear-max/adm/dolby-atmos/).

## Reading

`read file.wav` loads the file's ADM (libbw64 and libadm) and
resolves the rendering items the way the EAR's `select_items` does: it follows
the selected `audioProgramme` (`programme n` picks another one) through the
contents and objects to the channel formats, and maps every item to the file
track that carries it through the `chna` chunk. The info outlet (the fourth)
reports the file (`file path samplerate channels frames`), the programmes, one
line per item (`object n track name blocks`, `directspeakers n track name
labels...`, `scene n name order normalization tracks...`) and warnings, and
the three renderer outlets send `tracks ...` lists, the static bed metadata
(`setvalue n speakerlabel/position/bounds/lfe/packformat` for
`mc.ear.direct~`) and the scene parameters (`order`, `normalization`,
`tracks` for `mc.ear.hoa~`). Objects metadata is timed: `start` runs a
transport on Max's scheduler that emits every `audioBlockFormat` at its start
time as `mc.ear.objects~` messages, `stop` stops it, `seek ms` moves it, and
`time ms` emits the blocks active at a position without running. Each block
is preceded by `setvalue n ramp ms` with the EAR's interpolation length for
that block (BS.2127 section 7.2: the block duration, the `interpolationLength`
when `jumpPosition` is set, or 0 for a jump; no interpolation when the block
does not follow the previous one directly), so the renderer produces the same
gain ramps as the reference. The audio is played by one `mc.sfplay~` on the
file (started together with the transport), and `mc.ear.select~` routes the
tracks to each renderer:

```
[read file.wav( [start(                 [mc.sfplay~ 12]
|                                        |         |         |
[ear.adm]                      [mc.ear.select~] [mc.ear.select~] [mc.ear.select~]
|  |  |                           |                |                |
|  |  +-- hoa messages -----------|----------------|----------------+
|  +----- direct messages --------|----------------+                |
+-------- objects messages -------+                |                |
                                  |                |                |
                         [mc.ear.objects~ 4+5+0] [mc.ear.direct~ 4+5+0] [mc.ear.hoa~ 4+5+0]
```

The `tracks` message from each renderer outlet goes to the `mc.ear.select~`
in front of that renderer as well (the help patcher shows the connections),
so the selection follows the file.

## Writing

`record` starts capturing the object messages sent to `ear.adm`:
the same `setvalue`, `applyvalues`, parameter and list messages
`mc.ear.objects~` takes (send them to both objects), timestamped on Max's
scheduler from the moment of `record`; `setvalue n ramp ms` sets the
interpolation written for the following changes of that object (the `ramp`
attribute is the default). `stop` ends the capture. Record the object audio
at the same time with `mc.sfrecord~` (one channel per object, in object
order), then `write out.wav recorded.wav` copies the audio into a BW64 file
with the captured timeline as ADM metadata: one `audioProgramme` and
`audioContent` (`@programmename`), and per object an `audioObject` named with
`name n symbol`, its pack, channel, stream and track formats, a track UID
and a `chna` entry, with one `audioBlockFormat` per captured change
(`jumpPosition` with the ramp as `interpolationLength`). `writexml out.xml`
writes the metadata alone, and `clear` discards the capture. `@chans` sets
the number of objects captured.

## Beds and scenes

A DirectSpeakers bed and an HOA scene are written after
the objects' tracks: `@directchans 6` adds six bed channels on the tracks
after the objects, `@hoaorder 1` four HOA components in ACN order after the
bed (the recorded audio has the tracks in that order: objects, bed, scene).
Their metadata is given in the renderers' own formats, prefixed with the
outlet it belongs to: `direct setvalue 4 speakerlabel LFE1`,
`direct setvalue 1 position 30 0`, `direct applyvalues lfe 0 0 0 1 0 0`,
`direct inputlayout 0+5+0` (labels, nominal positions and LFE after a
BS.2051 layout, exactly as `mc.ear.direct~`'s `inputlayout`), `direct name
music`; `hoa order 1`, `hoa normalization N3D`, `hoa name ambience`. A bed
whose channels name a common definitions layout (`inputlayout` sets the
`packformat`, as does a file read by `ear.adm`) and keeps its channels at
their nominal positions, without bounds, is written as a reference to that
layout and its channels, as the EAR's own tools do; any other bed (a label
or position edited after `inputlayout`, bounds or Cartesian coordinates
given) gets its own channel formats with the labels, positions and bounds
given.

A bed can change over time too: while `record` runs (or `mc.ear.record~`
records), a change of a channel's labels, position or bounds becomes a
timed `audioBlockFormat` at that moment, and such a bed is written with its
own channel formats (the common definitions are static). On reading, a
timed bed's blocks are emitted on the direct outlet at their start times by
the same transport as the Objects blocks (`mc.ear.direct~` ramps the gains
over its `ramp`); `lfe` and `packformat` are the channel's for the whole
file. HOA blocks are static: order, degree and normalization do not change
over time, and the timed HOA parameters (`nfcRefDist`, `screenRef`) are not
implemented by libear.

Because `tracks` messages are ignored, the direct and hoa outlets of a
reading `ear.adm` or `mc.ear.play~` can be fed straight into these messages
through `[prepend direct]` and `[prepend hoa]` to copy a file's bed and
scene.

## Limitations

Limitations of this first version: muted objects, silent tracks, tracks missing
from the `chna` chunk and unsupported types are skipped with a warning on
the info outlet; `audioObject`
importance and complementary object groups are not interpreted (every
object is rendered); nested objects use the innermost start and duration;
when a file has several HOA scenes only the first is sent to the hoa
outlet; one bed and one scene are written per file, and HOA blocks are
static; and `ear.adm` with `mc.sfplay~` or `mc.sfrecord~`
needs a file Max can open (RIFF, under 4 GB), where `mc.ear.play~` and
`mc.ear.record~` do not.

Upstream libadm does not read or write `zoneExclusion`, so this package builds
against a fork, [tsln-lab/libadm](https://github.com/tsln-lab/libadm) (branch
`zone-exclusion`), which adds the element with its Cartesian and polar zones;
the `zone` messages of the renderers are written and read back as they are.
