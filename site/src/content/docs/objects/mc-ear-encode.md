---
title: mc.ear.encode~
description: The multichannel ambisonic encoder, the counterpart of mc.ear.objects~.
---

`mc.ear.encode~` is the multichannel ambisonic encoder, the counterpart of
[`mc.ear.objects~`](/libear-max/objects/ear-objects/#mcearobjects): every input channel is an object
positioned with the same messages, the output carries the summed ambisonic
components in ACN order.

`[mc.ear.encode~ 1 @chans 8]` is a drop-in counterpart of `mc.ear.objects~`
whose output is an ambisonic scene instead of loudspeaker signals: every
input channel is an object, addressed with the same [`setvalue`](/libear-max/objects/ear-objects/#mcearobjects), `applyvalues`,
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
