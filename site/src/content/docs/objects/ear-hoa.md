---
title: ear.hoa and mc.ear.hoa~
description: The multichannel ambisonic (ADM HOA) decoder, and its decoding matrix at control rate as lists and matrix~ messages.
---

`mc.ear.hoa~` is the multichannel ambisonic (ADM *HOA*) decoder: the input
carries the ambisonic components in ACN order, the output one channel per
loudspeaker, decoded with the EAR's AllRAD design for the layout. `ear.hoa` is
its control-rate counterpart: it outputs the decoding matrix for a layout,
order and normalization as lists, one per ambisonic component, and as
`matrix~` messages.

## mc.ear.hoa~

`[mc.ear.hoa~ 4+5+0 @order 1]` decodes an ambisonic scene to the layout. The
multichannel input carries the `(order+1)^2` components in ACN channel order
(`W Y Z X` for first order, then `V T R S U` for second order and so on),
normalized as SN3D by default (the ADM and AmbiX convention); `@normalization`
accepts `SN3D`, `N3D` and `FuMa`. `@order` goes from 0 to 8 and a new order
takes effect on the input channel count when the audio is restarted; an input
with a different number of channels is reported once (extra channels are
ignored, missing components are silent), except a single channel, which is
what `mc.ear.play~` carries for a file without a scene. The
decoder is libear's BS.2127 design (AllRAD over a spherical point set with
the layout's point-source panner, mean-power normalized), identical to the
reference renderer's. The layout's LFE channel stays silent, and
`screenRef` and `nfcRefDist` are not implemented, as in libear. Send
`components` to post which order and degree each input channel carries.

## ear.hoa

`[ear.hoa 0+5+0 @order 1]` outputs the same decoding matrix as lists instead
of applying it to signals. The left
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
