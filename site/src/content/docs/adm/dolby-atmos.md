---
title: Dolby Atmos masters
description: Reading a Dolby Atmos master ADM BWF, and writing one with @profile dolby.
---

## Reading

A Dolby Atmos master ADM BWF (the Dolby Atmos
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
go through the normal path. The renderers allocate the channels a master
needs by themselves (up to 118 objects and 30 bed channels in a 128-track
master); the new channels take effect when the audio is restarted, so start
the audio after loading the file, or restart it.

## Writing a Dolby Atmos master

`@profile dolby` on `ear.adm` and
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
