---
title: Testing against the EBU's ADM test files
description: The opt-in check that runs the reader, the calculators and the writer's round trip over the EBU's ADM test materials.
---

Beyond the unit tests, which only read files this package wrote itself, an
opt-in check runs the ADM reader, the gain calculators and the writer's round
trip over files made by other tools, fetched as the release assets of
[tsln-lab/adm-test-corpus](https://github.com/tsln-lab/adm-test-corpus); they
are not part of this repository (about three gigabytes). The corpus has two
parts, and `source/corpus/assets.txt` names the release each file comes from:

- **The EBU's ADM test materials** (release `v1.0.0`): channel-based beds
  up to 22.2, objects over one or many tracks, several programmes, and the
  "kitchen sink" file that carries every BS.2076-1 parameter. The files are
  the EBU's, published at <https://qc.ebu.io/testmaterials/?path=/ADM/>,
  and redistributed unmodified.
- **Netflix's Dolby Atmos masters** (release `netflix-v1.0.0`): the first
  60 seconds of the master ADM files of *Meridian*, *Nocturne* and
  *Sol Levante*, from [Netflix Open Content](https://opencontent.netflix.com/)
  under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), trimmed
  with the corpus repository's `tools/trim_bw64.py` so that the ADM
  metadata, the `chna` chunk and Dolby's `dbmd` chunk are the full master's.
  They bring 58 to 92 tracks, up to 82 objects and 43,000 blocks per file,
  Dolby's `RoomCentric` bed channel formats, several beds and contents per
  programme, and a programme that starts at a timecode rather than zero.
  Because their metadata runs past their audio, the round trip compares
  block ends no further than the end of the audio.

The licence and provenance of each part are in the corpus repository's
`ATTRIBUTION.md`.

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
