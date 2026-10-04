---
title: Mixing beds, objects and scenes
description: Summing the three renderers to one layout, and keeping them time-aligned.
---

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
