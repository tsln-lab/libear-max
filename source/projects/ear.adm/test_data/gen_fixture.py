"""Generate the ear.adm parity fixture with the EBU ADM Renderer (python ear 2.1):
reference.wav (BW64, 12 tracks, 0.1 s of audio, ADM metadata spanning 3 s),
reference.xml (the axml chunk) and reference.json (the rendering items the EAR
selects from it, with its block timing: start, end, interpolation length).

    python3 -m venv earve && . earve/bin/activate && pip install ear lxml
    python3 gen_fixture.py

The unit tests (ear.adm_test.cpp) check the items, blocks and messages
ear.adm produces against reference.json.
"""
import json, numpy as np
from fractions import Fraction
from ear.fileio import openBw64
from ear.fileio.adm.builder import ADMBuilder
from ear.fileio.adm.generate_ids import generate_ids
from ear.fileio.adm.xml import adm_to_xml
from ear.fileio.adm.chna import populate_chna_chunk
from ear.fileio.bw64.chunks import ChnaChunk, FormatInfoChunk
from ear.fileio.adm.elements import (AudioBlockFormatObjects, ObjectPolarPosition, ObjectCartesianPosition,
    JumpPosition, ChannelLock, ObjectDivergence, AudioBlockFormatDirectSpeakers, DirectSpeakerPolarPosition,
    BoundCoordinate, Frequency, TypeDefinition)
from ear.core.select_items import select_rendering_items
from ear.core.objectbased.renderer import InterpretObjectMetadata
import lxml.etree

sr = 48000
nsamples = 4800
builder = ADMBuilder()
builder.create_programme(audioProgrammeName="libear-max test programme")
builder.create_content(audioContentName="content")

objA = builder.create_item_objects(0, "object A", block_formats=[
    AudioBlockFormatObjects(rtime=Fraction(0), duration=Fraction(1),
        position=ObjectPolarPosition(azimuth=30.0, elevation=0.0, distance=1.0), gain=1.0),
    AudioBlockFormatObjects(rtime=Fraction(1), duration=Fraction(1),
        position=ObjectPolarPosition(azimuth=-30.0, elevation=10.0, distance=1.0),
        jumpPosition=JumpPosition(flag=True, interpolationLength=Fraction(1, 4)), width=20.0, gain=0.5),
    AudioBlockFormatObjects(rtime=Fraction(5, 2), duration=Fraction(1, 2),
        position=ObjectPolarPosition(azimuth=0.0, elevation=30.0, distance=1.0),
        jumpPosition=JumpPosition(flag=True)),
])
objB = builder.create_item_objects(1, "object B", block_formats=[
    AudioBlockFormatObjects(rtime=Fraction(0), duration=Fraction(2),
        position=ObjectCartesianPosition(X=0.5, Y=0.5, Z=0.0), cartesian=True, diffuse=0.5,
        channelLock=ChannelLock(maxDistance=0.3),
        objectDivergence=ObjectDivergence(value=0.2, positionRange=0.4), screenRef=True),
])
objB.audio_object.start = Fraction(1, 2)
objB.audio_object.duration = Fraction(2)

labels = ["M+030", "M-030", "M+000", "LFE1", "M+110", "M-110"]
positions = [(30, 0), (-30, 0), (0, 0), (0, -30), (110, 0), (-110, 0)]
bed_blocks = [[AudioBlockFormatDirectSpeakers(speakerLabel=[label],
        position=DirectSpeakerPolarPosition(bounded_azimuth=BoundCoordinate(float(az)),
                                            bounded_elevation=BoundCoordinate(float(el)),
                                            bounded_distance=BoundCoordinate(1.0)))]
              for label, (az, el) in zip(labels, positions)]
bed = builder.create_item_multichannel(type=TypeDefinition.DirectSpeakers, track_indices=[2,3,4,5,6,7], name="bed 0+5+0", block_formats=bed_blocks)
bed.channel_formats[3].frequency = Frequency(lowPass=120.0)
hoa = builder.create_item_hoa([8, 9, 10, 11], orders=[0, 1, 1, 1], degrees=[0, -1, 0, 1], name="scene", normalization="SN3D")

adm = builder.adm
generate_ids(adm)
axml = lxml.etree.tostring(adm_to_xml(adm), pretty_print=True)
chna = ChnaChunk()
populate_chna_chunk(chna, adm)
fmt = FormatInfoChunk(formatTag=1, channelCount=12, sampleRate=sr, bitsPerSample=24)
audio = np.zeros((nsamples, 12), dtype=np.float32)
for i in range(12):
    audio[:, i] = (i + 1) / 100.0
import os
out_dir = os.path.dirname(os.path.abspath(__file__)) + "/"
with openBw64(out_dir + "reference.wav", 'w', chna=chna, formatInfo=fmt, axml=axml) as f:
    f.write(audio)

items = select_rendering_items(adm)
interp = InterpretObjectMetadata(None)
def num(x):
    return None if x is None else float(x)
def blocks_of(src):
    out = []
    while True:
        tm = src.get_next_block()
        if tm is None: break
        out.append(tm)
    return out
expected = []
for item in items:
    t = type(item).__name__.replace("RenderingItem", "")
    path = item.adm_path if hasattr(item, "adm_path") else item.adm_paths[0]
    e = {"type": t, "name": path.audioObjects[-1].audioObjectName}
    if t == "Object":
        e["track"] = item.track_spec.track_index
        interp = InterpretObjectMetadata(None)
        blocks = []
        for tm in blocks_of(item.metadata_source):
            start, end = interp.block_start_end(tm)
            il = interp.interp_length(tm.block_format, end - start)
            effective = il if (interp.last_block_end is not None and start == interp.last_block_end) else Fraction(0)
            interp.last_block_end = end
            bf = tm.block_format
            b = {"start": num(start), "end": num(end), "interp": num(effective),
                 "gain": float(bf.gain), "diffuse": float(bf.diffuse), "width": float(bf.width), "height": float(bf.height), "depth": float(bf.depth),
                 "cartesian": bool(bf.cartesian), "screenRef": bool(bf.screenRef),
                 "channelLock": bf.channelLock is not None, "channelLockDistance": num(bf.channelLock.maxDistance) if bf.channelLock else None,
                 "divergence": float(bf.objectDivergence.value) if bf.objectDivergence else 0.0}
            if bf.cartesian:
                b.update({"X": float(bf.position.X), "Y": float(bf.position.Y), "Z": float(bf.position.Z)})
            else:
                b.update({"azimuth": float(bf.position.azimuth), "elevation": float(bf.position.elevation), "distance": float(bf.position.distance)})
            blocks.append(b)
        e["blocks"] = blocks
    elif t == "DirectSpeakers":
        e["track"] = item.track_spec.track_index
        bf = blocks_of(item.metadata_source)[0].block_format
        e["speakerLabels"] = list(bf.speakerLabel)
        e["azimuth"] = float(bf.position.azimuth); e["elevation"] = float(bf.position.elevation)
        e["lfe"] = bool(item.metadata_source.get_next_block is not None and path.audioChannelFormat.frequency is not None and path.audioChannelFormat.frequency.lowPass is not None)
    elif t == "HOA":
        e["tracks"] = [ts.track_index for ts in item.track_specs]
        tm = blocks_of(item.metadata_source)[0]
        e["orders"] = list(tm.orders); e["degrees"] = list(tm.degrees); e["normalization"] = tm.normalization
    expected.append(e)
json.dump({"sampleRate": sr, "samples": nsamples, "tracks": 12, "items": expected}, open(out_dir + "reference.json", "w"), indent=1)
open(out_dir + "reference.xml", "wb").write(axml)
print(json.dumps(expected)[:2500])
