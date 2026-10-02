{
	"patcher": {
		"fileversion": 1,
		"appversion": {
			"major": 8,
			"minor": 6,
			"revision": 0,
			"architecture": "x64",
			"modernui": 1
		},
		"classnamespace": "box",
		"rect": [
			100.0,
			100.0,
			820.0,
			470.0
		],
		"bglocked": 0,
		"openinpresentation": 0,
		"default_fontsize": 12.0,
		"default_fontface": 0,
		"default_fontname": "Arial",
		"gridonopen": 1,
		"gridsize": [
			15.0,
			15.0
		],
		"gridsnaponopen": 1,
		"objectsnaponopen": 1,
		"statusbarvisible": 2,
		"toolbarvisible": 1,
		"lefttoolbarpinned": 0,
		"toptoolbarpinned": 0,
		"righttoolbarpinned": 0,
		"bottomtoolbarpinned": 0,
		"toolbars_unpinned_last_save": 0,
		"tallnewobj": 0,
		"boxanimatetime": 200,
		"enablehscroll": 1,
		"enablevscroll": 1,
		"devicewidth": 0.0,
		"description": "Record an ADM (BW64) file: the multichannel input is the objects' audio, and the object metadata sent to the object while it records is written as blocks timed by the audio.",
		"digest": "Record an ADM file, audio and metadata",
		"tags": [
			"spatial audio",
			"ADM",
			"files",
			"mc"
		],
		"style": "",
		"subpatcher_template": "",
		"assistshowspatchername": 0,
		"dependency_cache": [],
		"autosave": 0,
		"boxes": [
			{
				"box": {
					"id": "obj-1",
					"maxclass": "comment",
					"text": "mc.ear.record~ — record an ADM (BW64) file, audio and metadata together",
					"patching_rect": [
						15.0,
						10.0,
						780.0,
						20.0
					],
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 12.0
				}
			},
			{
				"box": {
					"id": "obj-2",
					"maxclass": "comment",
					"text": "The multichannel input carries the tracks in file order: the objects' audio (one channel per object, as fed to mc.ear.objects~), then a DirectSpeakers bed (@directchans channels) and an HOA scene ((@hoaorder+1)^2 components), combined with mc.combine~. While it records, the object metadata is sent to the object in the same format as to mc.ear.objects~ ('setvalue N parameter', 'applyvalues', lists, or a parameter as a message for all objects), and every change is written as an audioBlockFormat timed by the audio itself, with the ramp in force as its interpolation: audio and metadata cannot drift apart. The bed and scene metadata is static: 'direct ...' and 'hoa ...' in the formats of mc.ear.direct~ and mc.ear.hoa~. 'open path' names the file, 'start' (or 1) records, 'stop' (or 0) finishes the file and reports 'written path tracks length-ms'. Files over 4 GB are written as RF64.",
					"patching_rect": [
						15.0,
						35.0,
						780.0,
						101.0
					],
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 12.0
				}
			},
			{
				"box": {
					"id": "obj-3",
					"maxclass": "message",
					"text": "open myfile.wav",
					"patching_rect": [
						15.0,
						150.0,
						110.0,
						22.0
					],
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-4",
					"maxclass": "message",
					"text": "start",
					"patching_rect": [
						135.0,
						150.0,
						38.0,
						22.0
					],
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-5",
					"maxclass": "message",
					"text": "stop",
					"patching_rect": [
						183.0,
						150.0,
						35.0,
						22.0
					],
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-6",
					"maxclass": "message",
					"text": "position",
					"patching_rect": [
						228.0,
						150.0,
						60.0,
						22.0
					],
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-7",
					"maxclass": "message",
					"text": "name 1 voice",
					"patching_rect": [
						298.0,
						150.0,
						85.0,
						22.0
					],
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-8",
					"maxclass": "message",
					"text": "setvalue 1 azimuth 30",
					"patching_rect": [
						393.0,
						150.0,
						135.0,
						22.0
					],
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-9",
					"maxclass": "message",
					"text": "setvalue 1 ramp 100",
					"patching_rect": [
						538.0,
						150.0,
						125.0,
						22.0
					],
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-10",
					"maxclass": "message",
					"text": "applyvalues gain 1 0.5",
					"patching_rect": [
						673.0,
						150.0,
						135.0,
						22.0
					],
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-11",
					"maxclass": "newobj",
					"text": "mc.ear.encode~ 2",
					"patching_rect": [
						15.0,
						220.0,
						110.0,
						22.0
					],
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"multichannelsignal"
					]
				}
			},
			{
				"box": {
					"id": "obj-12",
					"maxclass": "comment",
					"text": "the tracks in file order: 2 objects, a 5.1 bed and 4 HOA components, combined with mc.combine~ into one multichannel signal",
					"patching_rect": [
						345.0,
						220.0,
						450.0,
						33.0
					],
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 12.0
				}
			},
			{
				"box": {
					"id": "obj-13",
					"maxclass": "newobj",
					"text": "mc.ear.record~ @chans 2 @directchans 6 @hoaorder 1",
					"patching_rect": [
						15.0,
						295.0,
						330.0,
						22.0
					],
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-14",
					"maxclass": "comment",
					"text": "outlet: recording, written (path, tracks, length in ms), failed, position",
					"patching_rect": [
						355.0,
						295.0,
						440.0,
						33.0
					],
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 12.0
				}
			},
			{
				"box": {
					"id": "obj-15",
					"maxclass": "newobj",
					"text": "print",
					"patching_rect": [
						15.0,
						340.0,
						40.0,
						22.0
					],
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": []
				}
			},
			{
				"box": {
					"id": "obj-16",
					"maxclass": "comment",
					"text": "The same messages can drive mc.ear.objects~ at the same time, so the mix is heard as it is recorded. Compared with ear.adm + mc.sfrecord~: one transport instead of two (the blocks are timed by the recorded frames, not by Max's scheduler), and files Max cannot write (RF64/BW64 over 4 GB). The file plays back with mc.ear.play~.",
					"patching_rect": [
						15.0,
						385.0,
						780.0,
						61.0
					],
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 12.0
				}
			},
			{
				"box": {
					"id": "obj-17",
					"maxclass": "message",
					"text": "direct inputlayout 0+5+0",
					"patching_rect": [
						15.0,
						180.0,
						160.0,
						22.0
					],
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-18",
					"maxclass": "message",
					"text": "direct setvalue 4 lfe 1",
					"patching_rect": [
						185.0,
						180.0,
						140.0,
						22.0
					],
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-19",
					"maxclass": "message",
					"text": "hoa order 1",
					"patching_rect": [
						335.0,
						180.0,
						80.0,
						22.0
					],
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-20",
					"maxclass": "message",
					"text": "hoa normalization SN3D",
					"patching_rect": [
						425.0,
						180.0,
						150.0,
						22.0
					],
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-21",
					"maxclass": "newobj",
					"text": "mc.combine~ 3",
					"patching_rect": [
						15.0,
						250.0,
						100.0,
						22.0
					],
					"numinlets": 3,
					"numoutlets": 1,
					"outlettype": [
						"multichannelsignal"
					]
				}
			},
			{
				"box": {
					"id": "obj-22",
					"maxclass": "comment",
					"text": "objects / bed / hoa inlets",
					"patching_rect": [
						125.0,
						250.0,
						200.0,
						20.0
					],
					"numinlets": 1,
					"numoutlets": 0,
					"fontsize": 12.0
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-3",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-4",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-5",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-6",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-7",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-8",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-9",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-10",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-15",
						0
					],
					"source": [
						"obj-13",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-17",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-18",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-19",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-20",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-21",
						0
					],
					"source": [
						"obj-11",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-21",
						0
					]
				}
			}
		]
	}
}
