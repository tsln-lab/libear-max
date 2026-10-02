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
			380.0
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
		"description": "Play an ADM (BW64) file with the audio streamed from disk to three multichannel outlets routed for the renderers, and the metadata emitted from the audio clock.",
		"digest": "Play an ADM file, audio and metadata",
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
					"patching_rect": [
						15.0,
						10.0,
						780.0,
						20.0
					],
					"text": "mc.ear.play~ \u2014 play an ADM (BW64) file, audio and metadata together"
				}
			},
			{
				"box": {
					"id": "obj-2",
					"maxclass": "comment",
					"patching_rect": [
						15.0,
						35.0,
						780.0,
						61.0
					],
					"text": "'open' reads the file's ADM and reports its rendering items on the info outlet. The audio is streamed from disk and comes out on three multichannel outlets with the file's tracks already routed: the Objects tracks for mc.ear.objects~, the DirectSpeakers tracks for mc.ear.direct~ and the HOA components for mc.ear.hoa~ (the channel counts follow the file when the audio is restarted). The three message outlets carry the metadata for the same renderers; the Objects blocks are emitted from the audio clock, with the EAR's interpolation rules ('setvalue N ramp ms' before each block), so audio and metadata stay together. Files over 4 GB play; no sample-rate conversion is done."
				}
			},
			{
				"box": {
					"id": "obj-3",
					"maxclass": "message",
					"patching_rect": [
						15.0,
						110.0,
						110.0,
						22.0
					],
					"text": "open myfile.wav",
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
					"patching_rect": [
						135.0,
						110.0,
						38.0,
						22.0
					],
					"text": "start",
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
					"patching_rect": [
						183.0,
						110.0,
						35.0,
						22.0
					],
					"text": "stop",
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
					"patching_rect": [
						228.0,
						110.0,
						45.0,
						22.0
					],
					"text": "pause",
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
					"patching_rect": [
						283.0,
						110.0,
						50.0,
						22.0
					],
					"text": "resume",
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
					"patching_rect": [
						343.0,
						110.0,
						70.0,
						22.0
					],
					"text": "seek 1500",
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
					"patching_rect": [
						423.0,
						110.0,
						45.0,
						22.0
					],
					"text": "loop 1",
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
					"patching_rect": [
						478.0,
						110.0,
						40.0,
						22.0
					],
					"text": "dump",
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
					"maxclass": "message",
					"patching_rect": [
						528.0,
						110.0,
						60.0,
						22.0
					],
					"text": "position",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-12",
					"maxclass": "message",
					"patching_rect": [
						598.0,
						110.0,
						85.0,
						22.0
					],
					"text": "programme 1",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-13",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						160.0,
						90.0,
						22.0
					],
					"text": "mc.ear.play~",
					"numinlets": 1,
					"numoutlets": 7,
					"outlettype": [
						"multichannelsignal",
						"multichannelsignal",
						"multichannelsignal",
						"",
						"",
						"",
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-14",
					"maxclass": "comment",
					"patching_rect": [
						115.0,
						160.0,
						680.0,
						33.0
					],
					"text": "outlets: objects audio, direct audio, hoa audio, objects messages, direct messages, hoa messages, info (file, programmes, items, warnings, position, end)"
				}
			},
			{
				"box": {
					"id": "obj-15",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						220.0,
						160.0,
						22.0
					],
					"text": "mc.ear.objects~ 4+5+0",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"multichannelsignal"
					]
				}
			},
			{
				"box": {
					"id": "obj-16",
					"maxclass": "newobj",
					"patching_rect": [
						185.0,
						220.0,
						150.0,
						22.0
					],
					"text": "mc.ear.direct~ 4+5+0",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"multichannelsignal"
					]
				}
			},
			{
				"box": {
					"id": "obj-17",
					"maxclass": "newobj",
					"patching_rect": [
						345.0,
						220.0,
						140.0,
						22.0
					],
					"text": "mc.ear.hoa~ 4+5+0",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"multichannelsignal"
					]
				}
			},
			{
				"box": {
					"id": "obj-18",
					"maxclass": "comment",
					"patching_rect": [
						495.0,
						220.0,
						300.0,
						33.0
					],
					"text": "each renderer gets its tracks and its metadata; the three outputs are summed"
				}
			},
			{
				"box": {
					"id": "obj-19",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						270.0,
						60.0,
						22.0
					],
					"text": "mc.dac~",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": []
				}
			},
			{
				"box": {
					"id": "obj-20",
					"maxclass": "newobj",
					"patching_rect": [
						600.0,
						160.0,
						40.0,
						22.0
					],
					"text": "print",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": []
				}
			},
			{
				"box": {
					"id": "obj-21",
					"maxclass": "comment",
					"patching_rect": [
						15.0,
						310.0,
						780.0,
						47.0
					],
					"text": "Compared with ear.adm + mc.sfplay~: one transport instead of two, no mc.ear.select~ routing, and files Max cannot open (RF64/BW64 over 4 GB). 'start' or 1 plays from the beginning (the metadata starts over), 'stop' or 0 stops, 'pause' and 'resume' keep the position, 'seek' moves it and emits the metadata there. The 'end' message arrives on the info outlet when the file is over; with @loop 1 it starts over instead."
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
						"obj-13",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-20",
						0
					],
					"source": [
						"obj-13",
						6
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
						"obj-16",
						0
					],
					"source": [
						"obj-13",
						1
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-17",
						0
					],
					"source": [
						"obj-13",
						2
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
						3
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-16",
						0
					],
					"source": [
						"obj-13",
						4
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-17",
						0
					],
					"source": [
						"obj-13",
						5
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-19",
						0
					],
					"source": [
						"obj-15",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-19",
						0
					],
					"source": [
						"obj-16",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-19",
						0
					],
					"source": [
						"obj-17",
						0
					]
				}
			}
		]
	}
}
