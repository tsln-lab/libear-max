---
title: mc.ear.play~
description: Playing an ADM file with one transport, audio streamed from disk and metadata emitted from the audio clock.
---

`mc.ear.play~` plays an ADM file, audio and metadata together: the audio is
streamed from disk to three multichannel outlets already routed for the three
renderers, and the metadata is emitted from the audio clock. It plays files
`mc.sfplay~` cannot open (RF64/BW64 over 4 GB) and converts a file at another
sample rate to the audio's.

## Playing with one transport

[`ear.adm`](/libear-max/adm/ear-adm/) with `mc.sfplay~`
runs two transports, Max's scheduler for the metadata and the audio for the
sound, which drift apart over a long file and need `mc.ear.select~` to route
the tracks. `mc.ear.play~` does both itself: `open file.wav` reads the ADM
(the same reports and static metadata as `ear.adm`, without the `tracks`
lists) and opens the audio for streaming; the three multichannel outlets
carry the Objects tracks, the DirectSpeakers tracks and the HOA components,
in item order, so they connect straight to `mc.ear.objects~`,
`mc.ear.direct~` and `mc.ear.hoa~` (the channel counts follow the file when
the audio is restarted, as with `mc.ear.select~`). `start` (or `1`) plays
from the beginning, `stop` (or `0`), `pause`, `resume` and `seek ms` control
the transport, `@loop 1` starts over at the end instead of sending `end`.
The Objects blocks are emitted by the audio thread's clock: one signal
vector before a block starts, the main thread is woken to send the block's
messages (with the same `setvalue n ramp ms` as `ear.adm`), so audio and
metadata cannot drift apart; the remaining offset is Max's main-thread
latency, under a few milliseconds. A reader thread streams the audio in
chunks through a ring buffer, so the audio thread never touches the disk;
when the disk falls behind, the vectors that could not be filled are
counted and reported in the Max console. Files over 4 GB (RF64/BW64) play.
A file at another sample rate is converted to the audio's as it streams:
the reader thread resamples each chunk (a windowed-sinc polyphase
resampler, band-limited to the lower Nyquist frequency, so a 96 kHz file
played at 48 kHz loses nothing it could keep and aliases nothing), and the
transport, `position` and the metadata stay on the file's own clock.

```
[open file.wav( [start(
|
[mc.ear.play~]
|   |   |   |   |   |   |
|   |   |   |   |   |   +-- info
|   |   |   +---+---+------ objects / direct / hoa messages
|   |   +------------------ [mc.ear.hoa~ 4+5+0]
|   +---------------------- [mc.ear.direct~ 4+5+0]
+-------------------------- [mc.ear.objects~ 4+5+0]
```
