# libear-max

Max externals for rendering [ADM](https://adm.ebu.io/) audio objects with
[libear](https://github.com/ebu/libear), the EBU's C++ implementation of the
ITU-R BS.2127 renderer (EAR). Built as a [Min-DevKit](https://github.com/Cycling74/min-devkit)
package, so it produces a regular Max package with externals, help patchers
and unit tests.

## Objects

| Object          | What it does |
| --------------- | ------------ |
| `ear.objects`   | Control-rate gain calculator for ADM *Objects* metadata. Outputs one list of direct gains and one of diffuse gains, one value per loudspeaker of a BS.2051 layout. |
| `ear.objects~`  | Signal-rate object renderer: one signal inlet, one signal outlet per loudspeaker. Gains are interpolated; the diffuse part runs through the BS.2127 decorrelation filters and the direct part is delay-compensated. |
| `ear.direct`    | Control-rate gain calculator for ADM *DirectSpeakers* channels: maps a channel by speaker label (e.g. `M+030`) or by nominal position and bounds onto a layout. |
| `mc.ear.objects~` | Multichannel object renderer: every channel of the multichannel input is an object with its own metadata, the multichannel output has one channel per loudspeaker. The decorrelators run once per loudspeaker on the shared diffuse bus. |
| `mc.ear.direct~` | Multichannel bed renderer: every input channel is a DirectSpeakers channel (by label or position), rendered onto the layout. `inputlayout 0+5+0` labels the input after a BS.2051 layout. |
| `mc.ear.hoa~` | Multichannel ambisonic (ADM *HOA*) decoder: the input carries the ambisonic components in ACN order, the output one channel per loudspeaker, decoded with the EAR's AllRAD design for the layout. |
| `ear.hoa` | Control-rate counterpart of `mc.ear.hoa~`: outputs the decoding matrix for a layout, order and normalization as lists, one per ambisonic component, and as `matrix~` messages. |
| `mc.ear.encode~` | Multichannel ambisonic encoder, the counterpart of `mc.ear.objects~`: every input channel is an object positioned with the same messages, the output carries the summed ambisonic components in ACN order. |
| `ear.adm` | Reads and writes ADM files (BW64 with ADM metadata): resolves a file's rendering items like the EAR, plays its object metadata to the renderers with the reference's interpolation rules, and captures object messages to write them with recorded audio as a new ADM file. |
| `mc.ear.select~` | Picks channels of a multichannel signal by number, to route the tracks of a file (as `ear.adm` reports them) to the renderer that handles them. |
| `mc.ear.play~` | Plays an ADM file, audio and metadata together: the audio is streamed from disk to three multichannel outlets already routed for the three renderers, and the metadata is emitted from the audio clock. Plays files `mc.sfplay~` cannot open (RF64/BW64 over 4 GB) and converts a file at another sample rate to the audio's. |
| `mc.ear.record~` | Records an ADM file, audio and metadata together: the multichannel input carries the objects' audio, a DirectSpeakers bed and an HOA scene, and the object metadata sent to it (as to `mc.ear.objects~`) is written as blocks timed by the recorded audio. Writes files over 4 GB as RF64. |

All objects take a BS.2051 layout name as argument (`0+2+0`, `0+5+0`, `2+5+0`,
`4+5+0`, `4+5+1`, `3+7+0`, `4+9+0`, `9+10+3`, `0+7+0`, `4+7+0`); the default is
`0+5+0`. Send `layouts` to list the names, `channels` for the channel order of
the current layout, and `positions` for the nominal loudspeaker positions.

### ear.objects / ear.objects~ metadata

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

Outlets of `ear.objects`: direct gains, diffuse gains, info (`channels`,
`positions`, `layouts`). Outlets of `ear.objects~`: one signal per loudspeaker
in the layout's channel order, e.g. `M+030 M-030 M+000 LFE1 M+110 M-110` for
`0+5+0`. Feed them to `mc.pack~` / `mc.dac~` or individual `dac~` channels.

### mc.ear.objects~ and mc.ear.direct~

Both take the output layout as argument and have one multichannel inlet and
one multichannel outlet. `@chans` sets the maximum number of input channels
(default 16; extra input channels are ignored with a warning). Metadata is
addressed per input channel in the style of Max's mc objects:

- `setvalue <n> <parameter> <values...>` sets one parameter of input `n`
  (1-based); `setvalue 0 ...` sets it for all inputs.
- `applyvalues <parameter> v1 v2 v3 ...` spreads values over inputs 1, 2, 3.
- Any parameter sent as a plain message (`diffuse 0.5`, `zone clear`) applies
  to all inputs; a list sets the position of all inputs.

`mc.ear.objects~` accepts every `ear.objects` parameter listed above plus
`position` and `zone`, and the `ramp` and `decorrelate` attributes. `ramp`
is also a per-object parameter (`setvalue 3 ramp 250`): it sets the
interpolation time of the next changes of that object, until a negative
value returns it to the attribute; `ear.adm` uses it to reproduce the
interpolation of each ADM block.
`mc.ear.direct~` accepts `speakerlabel`, `position`, `azimuth`, `elevation`,
`distance`, `bounds`, `lfe` and `packformat`, plus `cartesian` with `x`,
`y`, `z` (then `position` is `x y z` and `bounds` are `XMin XMax YMin YMax
ZMin ZMax`, as Dolby Atmos masters give their beds), and `inputlayout <name>`, which
labels the input channels after a BS.2051 layout (LFE channels included), so
rendering a 5.1 bed to 9+10+3 is `[mc.ear.direct~ 9+10+3]` with
`inputlayout 0+5+0`.

### mc.ear.hoa~

`[mc.ear.hoa~ 4+5+0 @order 1]` decodes an ambisonic scene to the layout. The
multichannel input carries the `(order+1)^2` components in ACN channel order
(`W Y Z X` for first order, then `V T R S U` for second order and so on),
normalized as SN3D by default (the ADM and AmbiX convention); `@normalization`
accepts `SN3D`, `N3D` and `FuMa`. `@order` goes from 0 to 8 and a new order
takes effect on the input channel count when the audio is restarted. The
decoder is libear's BS.2127 design (AllRAD over a spherical point set with
the layout's point-source panner, mean-power normalized), identical to the
reference renderer's. The layout's LFE channel stays silent, and
`screenRef` and `nfcRefDist` are not implemented, as in libear. Send
`components` to post which order and degree each input channel carries.

### ear.hoa

`[ear.hoa 0+5+0 @order 1]` is the control-rate counterpart: the same
decoding matrix, output as lists instead of applied to signals. The left
outlet sends one list per ambisonic component, in ACN order: the component's
channel number (1-based) followed by a gain per loudspeaker in the layout's
channel order, which `coll` stores as a row. The middle outlet sends the
matrix as `matrix~` messages, `input output gain` (0-based) for every entry,
so `[ear.hoa 0+5+0]` feeding `[matrix~ 4 6]` decodes a first-order scene
without mc objects. The right outlet answers `components` (the order and degree of each
ACN channel), `channels`, `positions` and `layouts`. The matrix is output
whenever `layout`, `order` or `normalization` changes (`@autocalc 1`) or on
`bang`; nothing is output when the object is created, so use `loadbang` for
the initial matrix. Both outlets send `clear` before the matrix, so `coll`
and `matrix~` keep nothing of a previous order or layout.

### mc.ear.encode~

`[mc.ear.encode~ 1 @chans 8]` is a drop-in counterpart of `mc.ear.objects~`
whose output is an ambisonic scene instead of loudspeaker signals: every
input channel is an object, addressed with the same `setvalue`, `applyvalues`,
plain-message and list conventions, and the output carries the summed
`(order+1)^2` components in ACN order, SN3D by default, ready for
`mc.ear.hoa~` or any AmbiX decoder. The order is the argument or `@order`
(0 to 8, 3 with `@normalization FuMa`). The encoding uses the EAR's spherical
harmonics (BS.2076 section 10.1), so a source at azimuth 90 is +Y and a
source above is +Z. Only the position (polar, or cartesian through the ADM
conversion to polar) and the `gain` of an object affect the encoding; the
other `mc.ear.objects~` parameters are accepted but ignored. The encoder has
no latency. Send `components` to post the order and degree of each output
channel.

#### Mixing beds, objects and scenes

As in the EAR reference renderer, every item type is rendered to the same
layout and the results are summed. `mc.ear.direct~`, `mc.ear.objects~` and
`mc.ear.hoa~` with the same layout argument produce the same channels in the
same order, so connect them to the same mc inlet (Max sums them) or use
`mc.+~`. The object renderers' decorrelation path has a latency of 255
samples; by default `mc.ear.direct~` and `mc.ear.hoa~` delay their output by
the same amount (`@align 1`), which is what the reference does, so beds,
scenes and objects stay time-aligned. Set `@align 0` for zero latency when no
decorrelating renderer is mixed in, or set `@decorrelate 0` on the object
renderer instead.

### ear.adm and mc.ear.select~: ADM files

`ear.adm` reads and writes ADM files, the BW64 (`.wav`) files with ADM
metadata that the EAR renders. It handles the metadata only; the audio goes
through Max's own file objects, which keeps everything sample-accurate and
lets you use any player or recorder.

**Reading.** `read file.wav` loads the file's ADM (libbw64 and libadm) and
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

**Writing.** `record` starts capturing the object messages sent to `ear.adm`:
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

**Beds and scenes.** A DirectSpeakers bed and an HOA scene are written after
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

**Dolby Atmos masters.** A Dolby Atmos master ADM BWF (the Dolby Atmos
Master ADM Profile) describes its bed with Dolby's own channel formats:
labels `RC_L`, `RC_R`, `RC_C`, `RC_LFE`, `RC_Lss`, `RC_Rss`, `RC_Lrs`,
`RC_Rrs`, `RC_Lts`, `RC_Rts` (`RC_Ls`, `RC_Rs` in 5.1) with Cartesian
positions at the corners of the cube, and no frequency element for the
LFE. On reading, these labels become the BS.2051 labels of the loudspeakers
at those places (`M+030`, `M-030`, `M+000`, `LFE1`, `M+090`, `M-090`,
`M+135`, `M-135`, `U+090`, `U-090`; `M+110`, `M-110`), so a layout that has
the loudspeaker takes the channel directly and the LFE is known as such,
and the Cartesian positions are passed on (`setvalue N cartesian 1`,
`setvalue N position x y z`), so a layout without the loudspeaker places
the channel by position as the EAR does; the profile's positions coincide
with the EAR's allocentric positions of these loudspeakers. The objects of
such a file (Cartesian positions, sampled blocks with 5 ms ramps, zones)
go through the normal path. Playing a 128-track master wants
`@chans 118` on `mc.ear.objects~`.

**Writing a Dolby Atmos master.** `@profile dolby` on `ear.adm` and
`mc.ear.record~` writes the file in the shape of the profile (`@profile
ebu`, the default, is BS.2076 as libadm writes it). The bed gets Dolby's
channel formats (`RoomCentricLeft`...), labels and Cartesian positions,
chosen by its channels' labels (the BS.2051 labels `inputlayout` and a read
file give, or Dolby's), and should be one of the profile's channel
configurations in its order: 2.0, 3.0, 5.0, 5.1, 7.0, 7.1, 7.0.2 or 7.1.2
(L R C LFE Lss Rss Lrs Rrs Lts Rts, Ls Rs for 5.x). The objects get
Cartesian blocks (polar positions and extents converted as the EAR does,
width, depth and height written equal, diffuse 0 or 1), `jumpPosition` 1
with the profile's 5.208 ms interpolation (0 on the first block) and only
the exclusion zones of the profile's vocabulary (`ZM1`, `ZM2L`/`ZM2R`,
`ZM3L`/`ZM3Lss`/`ZM3R`/`ZM3Rss`, `ZM4`, `ZM5`, `ZB`, `ZT`, by label or
bounds). The stream and track formats are named `PCM_` and the channel's
name, the stream references both the channel and the pack, the track UIDs
carry the sample rate and bit depth, the programme has its start and end,
the content is `Atmos_Master_Content` with `dialogue 2`, the ids follow the
profile (the bed is `AO_1001`, the objects start at `AO_100b`) and a file
of 4 GB or more is marked `RF64`. What the profile has no place for is left
out and reported (on the info outlet of `mc.ear.record~`, in the Max
window for `ear.adm`): an HOA scene (its tracks are written without
metadata), `objectDivergence`, `screenRef`, `screenEdgeLock`, the
`channelLock` `maxDistance`, zones outside the vocabulary and timed changes
of the bed; a sample rate other than 48 or 96 kHz or a bit depth other than
24 is reported as well. The `dbmd` chunk the profile also asks for is not
written yet.

**Playing with one transport: `mc.ear.play~`.** `ear.adm` with `mc.sfplay~`
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

**Recording with one transport: `mc.ear.record~`.** The counterpart for
writing: the multichannel input carries the tracks in file order, the
objects' audio first (one channel per object, as fed to `mc.ear.objects~`),
then the bed's channels (`@directchans`) and the HOA components
(`@hoaorder`), combined with `mc.combine~` when they come from different
places. The object metadata is sent to `mc.ear.record~` in the same format
as to the renderer (`setvalue n parameter`, `applyvalues`, lists, or a
parameter as a message for all objects), so the same messages can drive
both at once and the mix is heard as it is recorded; the bed and scene
metadata is given with `direct ...` and `hoa ...` as in `ear.adm`, a bed
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

### ear.direct

Attributes `layout`, `azimuth`, `elevation`, `distance`, `lfe` (marks the
channel as LFE), `packformat` (optional `audioPackFormatID` for pack-specific
mapping rules) and `autocalc`. Messages: `speakerlabel M+030 ...` (labels take
precedence; send with no arguments to clear), `bounds azMin azMax elMin elMax
[distMin distMax]`, a list `azimuth elevation [distance]`, and `bang`.

## Reference parity

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

## Testing against the EBU's ADM test files

Beyond the unit tests, which only read files this package wrote itself, an
opt-in check runs the ADM reader, the gain calculators and the writer's round
trip over the EBU's ADM test materials: files made by other tools, with
channel-based beds up to 22.2, objects over one or many tracks, several
programmes, and the "kitchen sink" file that carries every BS.2076-1
parameter. The files are the EBU's, published under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) at
<https://qc.ebu.io/testmaterials/?path=/ADM/>, and redistributed unmodified
as the release assets of
[tsln-lab/adm-test-corpus](https://github.com/tsln-lab/adm-test-corpus) so
that this repository can fetch them from one place; they are not part of this
repository (close to a gigabyte).

```sh
tools/fetch_corpus.sh ~/adm-corpus              # downloads and verifies the files
EARMAX_ADM_CORPUS=~/adm-corpus ctest --test-dir build -R adm_corpus --output-on-failure
```

`tests/adm_corpus` (built with the package, `source/corpus/`) reads every
file, selects every programme as `ear.adm` does, feeds every block to libear's
calculators for the 4+5+0 layout, writes the selection back with the document
builder and reads it again, and compares what it found with
`source/corpus/manifest.txt`, which records the expected result per file:
item counts, warnings, calculator errors and the differences of the round
trip. The check fails on any difference, which is a change of behaviour to
look at rather than necessarily a bug; after a deliberate change,
`tests/adm_corpus --record source/corpus/manifest.txt ~/adm-corpus` rewrites
the manifest for review. Without `EARMAX_ADM_CORPUS` the test reports itself
as skipped. The `ADM corpus` workflow runs the check weekly and on request,
with the files cached between runs.

The manifest also documents what the capture model cannot represent: objects
that share one track over different spans come back on one track each, an
`audioObject`'s own start and duration are not written (every object spans
the file), and the DirectSpeakers channels of several objects are written as
one bed, so a programme with two mono dialogue objects loses their packs. The
kitchen sink file is refused by libadm's validation (a `maxDuckingDepth`
outside its range, among others) and is recorded as such.

## Building

Requirements: CMake 3.19+, a C++17 compiler, git, and the Boost headers
needed by libear (`optional`, `variant`, `math`, `algorithm`, `smart_ptr`)
and libadm (`range`, `rational`, `iterator`, `functional`, `integer`).

```sh
git clone --recursive https://github.com/tsln-lab/libear-max.git
cd libear-max
```

**macOS** (Xcode 10+, universal x86_64/arm64 with Xcode 12+):

```sh
brew install boost
cmake --preset macos
cmake --build --preset macos
ctest --preset macos
```

**Windows** (Visual Studio 2022 or later, Boost from vcpkg via the manifest in `vcpkg.json`):

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
cmake --preset windows
cmake --build --preset windows
ctest --preset windows
```

**Linux** (no Max, but the unit tests run against Min's mock kernel):

```sh
sudo apt-get install libboost-dev ninja-build
cmake --preset linux
cmake --build --preset linux
ctest --preset linux
```

The externals land in `externals/`, the test binaries in `tests/`. To use the
package in Max, clone (or symlink) the repository folder into
`~/Documents/Max 9/Packages/` (or the Max 8 equivalent) and build; Max picks
up `externals/` and `help/` directly.

## Releases

Pre-built packages are published on the
[Releases](https://github.com/tsln-lab/libear-max/releases) page. Each release
contains `libear-max-<version>.zip`, a complete Max package with the macOS
(universal) and Windows (x64) externals, help patchers and `package-info.json`;
unzip it into your `Max 9/Packages` folder. Per-platform archives are attached
too.

Releases are cut with [release-please](https://github.com/googleapis/release-please)
from [conventional commits](https://www.conventionalcommits.org/):

1. Commit messages and pull request titles use the conventional format:
   `feat: ...` for a new feature (minor version), `fix: ...` for a bug fix
   (patch version), `feat!: ...` or a `BREAKING CHANGE:` footer for a breaking
   change. `docs:`, `ci:`, `chore:`, `refactor:`, `test:`, `build:` and
   `perf:` are accepted and do not trigger a release on their own (`docs:`
   and `perf:` are listed in the changelog). The *PR title* workflow checks
   the title, which becomes the commit message when a pull request is
   squash-merged; with a merge commit, release-please reads the individual
   commits of the branch instead, so those need the format too.
2. On every push to `main`, the *Release Please* workflow keeps a release pull
   request up to date that bumps `version.txt`, updates `CHANGELOG.md` and
   shows the release notes.
3. Merging that pull request creates the `vX.Y.Z` tag and the GitHub release,
   and dispatches the *Build* workflow on the tag, which builds and tests on
   macOS, Windows and Linux and attaches the packages to the release. The
   version in `package-info.json` is taken from the tag.

The first release pull request opens once a `feat:` or `fix:` commit lands on
`main`; a `Release-As: 1.2.3` commit footer forces a specific version. Pushing
a `v*` tag by hand still works and produces a release the same way. While the
version is below 1.0.0, breaking changes bump the minor version and features
bump the patch version. Release Please needs the repository setting
*Actions → General → Allow GitHub Actions to create and approve pull requests*
enabled.

Without the secrets described below, the macOS externals are ad-hoc signed
but not notarized. If Max refuses to load a downloaded package, clear the
quarantine flag once:

```sh
xattr -dr com.apple.quarantine "~/Documents/Max 9/Packages/libear-max"
```

### Signing and notarization (macOS)

The workflow signs the `.mxo` bundles with a Developer ID certificate, submits
them to Apple's notary service and staples the tickets whenever the repository
has the secrets below. Without them every signing step is skipped and the
build is ad-hoc signed. Windows externals are not code-signed.

Prerequisites on the Apple side (a paid Apple Developer Program membership is
required for both):

1. **Developer ID Application certificate.** In Xcode, *Settings → Accounts →
   Manage Certificates → + → Developer ID Application* (or create it at
   developer.apple.com/account/resources/certificates). Then export it from
   Keychain Access as a `.p12` with a password: open *My Certificates*, select
   the certificate together with its private key, *File → Export Items*.
   The certificate's name is the signing identity, e.g.
   `Developer ID Application: Your Name (TEAMID1234)`; check it with
   `security find-identity -v -p codesigning`.
2. **App Store Connect API key for notarization.** At
   appstoreconnect.apple.com → *Users and Access → Integrations → App Store
   Connect API → Team Keys*, generate a key with the *Developer* role. Note the
   *Key ID* and the *Issuer ID*, and download the `AuthKey_<KEYID>.p8` file
   (it can only be downloaded once).

Then add these repository secrets (*Settings → Secrets and variables →
Actions*):

| Secret | Value |
| --- | --- |
| `APPLE_SIGNING_IDENTITY` | the certificate name, e.g. `Developer ID Application: Your Name (TEAMID1234)` |
| `APPLE_CERTIFICATE_P12` | the `.p12` file, base64-encoded: `base64 -i certificate.p12 \| pbcopy` |
| `APPLE_CERTIFICATE_PASSWORD` | the password chosen when exporting the `.p12` |
| `APPLE_API_KEY_ID` | the App Store Connect API *Key ID* |
| `APPLE_API_ISSUER_ID` | the App Store Connect API *Issuer ID* |
| `APPLE_API_KEY_P8` | the full contents of `AuthKey_<KEYID>.p8` (paste the text as is) |

What the workflow then does, in `.github/workflows/build.yml`:

1. Imports the certificate into a temporary keychain that is deleted at the
   end of the job.
2. Re-signs each `.mxo` with `codesign --options runtime --timestamp`, which
   replaces the ad-hoc signature from the build. The hardened runtime and a
   secure timestamp are both required by the notary service.
3. Zips the externals, submits them with `xcrun notarytool submit --wait`,
   fails the job (and prints the notary log) if the submission is not
   accepted, and staples the resulting tickets to the bundles with
   `xcrun stapler staple`.
4. Packages the stapled bundles; the release notes say whether the build was
   notarized.

To test the setup before tagging a release, run the workflow manually from
the *Actions* tab (it has a `workflow_dispatch` trigger) and check the
"Notarize and staple externals" step. Locally you can verify a downloaded
bundle with:

```sh
codesign --verify --strict --verbose=2 ear.objects.mxo
xcrun stapler validate ear.objects.mxo
```

## Repository layout

```
CMakeLists.txt                  package build (Min-DevKit style) + libear
package-info.json.in            Max package manifest template
source/min-api/                 Min C++ API for Max (submodule; brings max-sdk-base and the mock kernel)
source/libear/                  libear fork with reference-parity ports (submodule; bundles Eigen, xsimd, KISS FFT)
source/projects/shared/         ear_max.h: layout helpers and the Objects metadata attribute base
source/projects/ear.objects/    ear.objects  (class in .h, registration in .cpp, Catch tests in _test.cpp)
source/projects/ear.direct/     ear.direct
source/projects/ear.objects_tilde/  ear.objects~
source/projects/mc.ear.objects_tilde/  mc.ear.objects~ (multichannel)
source/projects/mc.ear.direct_tilde/   mc.ear.direct~ (multichannel)
source/projects/mc.ear.hoa_tilde/      mc.ear.hoa~ (multichannel ambisonic decoder)
source/projects/ear.hoa/               ear.hoa (the decoding matrix at control rate)
source/projects/mc.ear.encode_tilde/   mc.ear.encode~ (multichannel ambisonic encoder)
source/projects/mc.ear.select_tilde/   mc.ear.select~ (channel selection for routing file tracks)
source/projects/ear.adm/               ear.adm (ADM file reading and writing; test_data/ holds the EAR-generated fixture)
source/projects/mc.ear.play_tilde/     mc.ear.play~ (plays an ADM file: streamed audio and metadata together)
source/projects/mc.ear.record_tilde/   mc.ear.record~ (records an ADM file: audio and captured metadata together)
source/projects/shared/ear_max_adm.h   rendering item selection and block timing like the EAR, ADM document building
source/projects/shared/ear_max_adm_player.h  the loaded file's items, their reports and timed emission (ear.adm, mc.ear.play~)
source/projects/shared/ear_max_adm_capture.h capturing object, bed and scene metadata for writing (ear.adm, mc.ear.record~)
source/projects/shared/ear_max_stream.h      disk streaming of a BW64 file: reader thread and ring buffer for the audio thread
source/projects/shared/ear_max_resample.h    windowed-sinc polyphase sample-rate conversion for the streamed audio
source/projects/shared/ear_max_sink.h        disk writing of a BW64 file with ADM: ring buffer from the audio thread and writer thread
source/libadm/                         libadm, the EBU ADM library (submodule: the tsln-lab fork, which adds zoneExclusion)
source/libbw64/                        libbw64, the EBU BW64 file library (submodule, header-only)
source/projects/shared/ear_max_hoa.h   spherical harmonics in the EAR's conventions
source/projects/shared/ear_max_dsp.h   bus_renderer: the shared N-in / L-out gain matrix with ramps and decorrelation
help/                           help patchers
docs/                           reference pages (generated by the unit tests, see below)
```

Adding an object: create `source/projects/<name>/` with a `CMakeLists.txt`
copied from an existing project, a `<name>.h`/`<name>.cpp` pair and, optionally,
`<name>_test.cpp`. Folders are picked up automatically; `_tilde` in a folder
name becomes `~` in the external's name.

### Reference pages

The reference pages in `docs/` are generated from the description strings in
the source by the unit tests (`EARMAX_TEST_GENERATE_MAXREF` in
`source/projects/shared/ear_max_test.h`, generator in `ear_max_doc.h`) and
shipped with the package. Unlike the pages Min writes when an external is
first loaded, they list every settable attribute under *Messages* as well,
since an attribute is set by sending its name as a message (`azimuth 30`),
and they escape XML. The externals set `documentation_flags::do_not_generate`
so Max does not overwrite them. After changing a description, run the tests
and commit the regenerated page; CI fails when `docs/` is out of date or a
page is not well-formed XML.

## Design notes

- libear is compiled as a static library and linked into each external, so
  the externals have no runtime dependencies beyond Max. The same goes for
  libadm (static) and libbw64 (header-only) in `ear.adm`.
- Gain calculation happens on Max's main thread (attribute setters and
  messages are deferred there by Min). The signal objects share one DSP core
  (`bus_renderer`) that mixes N inputs onto L loudspeakers; new gain vectors
  reach the audio thread through a mutex that the audio thread only
  `try_lock`s, and are ramped linearly over `@ramp` milliseconds per input.
- The multichannel outlets use the Max C API directly (`multichanneloutputs`
  and `inputchanged`, registered from `maxclass_setup`), since Min only
  supports multichannel inlets.
- `mc.ear.play~` streams the file on its own thread: the reader keeps a
  ring buffer filled in 4096-frame chunks and performs the seeks and the
  file hand-over; the audio thread `try_lock`s the ring only to pull a
  vector through the routing table, and never blocks, allocates or frees.
  It wakes the main thread with a Max queue to emit the metadata that
  falls within the next vector. The ring holds frames at the audio's rate:
  a file at another rate is converted chunk by chunk on the reader thread
  (Kaiser-windowed sinc, 32 taps per side at the band-limiting rate, 256
  phases with linear interpolation), the history before a seek position is
  primed from the file so a seek lands on the exact frame, and the file
  position the metadata runs on is derived from the output frames pulled.
- `mc.ear.record~` is the mirror image: the audio thread interleaves each
  vector into a ring buffer (a full ring drops the vector and counts it),
  a writer thread drains it to libbw64 in 4096-frame chunks, and on `stop`
  the same thread builds the ADM document from the captured blocks and
  closes the file with its `chna` and `axml` chunks (libbw64 writes the
  `axml` after the data, so the metadata only has to be known at the end).
  The capture's clock is the number of frames the audio thread has pushed.
- The decorrelation path uses libear's partitioned block convolver at the
  current signal vector size and its delay buffer for the direct path, exactly
  as described in `ear::GainCalculatorObjects`.
- On universal macOS builds libear's x86-specific SIMD kernels are disabled
  (`EAR_SIMD=OFF`), because their compiler flags are not valid for the arm64
  slice; the portable code path is used instead.

## Roadmap

- Upstream the reference parity work to `ebu/libear`.
- Custom reproduction screens (currently the default screen is used for
  `screenref` and screen edge lock).
- HOA `screenRef` and `nfcRefDist`, once libear and the reference implement them.

## License

libear-max is released under the MIT License. libear is Apache-2.0, Min-API
and max-sdk-base are MIT; see [License.md](License.md).
