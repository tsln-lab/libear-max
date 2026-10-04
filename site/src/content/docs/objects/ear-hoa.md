---
title: ear.hoa
description: The HOA decoding matrix at control rate, as lists and as matrix~ messages.
---

`ear.hoa` is the control-rate counterpart of [`mc.ear.hoa~`](/libear-max/objects/mc-ear-hoa/):
it outputs the decoding matrix for a layout, order and normalization as
lists, one per ambisonic component, and as `matrix~` messages.

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
