---
title: mc.ear.hoa~
description: The multichannel ambisonic (ADM HOA) decoder.
---

`mc.ear.hoa~` is the multichannel ambisonic (ADM *HOA*) decoder: the input
carries the ambisonic components in ACN order, the output one channel per
loudspeaker, decoded with the EAR's AllRAD design for the layout.

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

The control-rate counterpart, which outputs the same decoding matrix as
lists, is [ear.hoa](/libear-max/objects/ear-hoa/).
