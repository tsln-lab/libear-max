---
title: Design notes
description: How the externals are built, threaded and linked.
---

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
