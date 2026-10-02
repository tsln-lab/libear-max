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
			560.0
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
		"description": "Read and write ADM (BW64) files alongside the renderers: play a file's object metadata with the EAR's interpolation rules, or capture object messages and write them with recorded audio.",
		"digest": "Read and write ADM files",
		"tags": [
			"spatial audio",
			"ADM",
			"files"
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
					"text": "ear.adm \u2014 read and write ADM (BW64) files alongside the renderers"
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
						47.0
					],
					"text": "Reading: 'read' loads the ADM metadata of a BW64 file, reports its rendering items (which file tracks carry objects, beds and scenes) and sends the static metadata to the renderers. The transport ('start', 'stop', 'seek', 'time') emits the Objects metadata block by block with the EAR's interpolation rules, as 'setvalue N ramp ms' followed by the block's parameters. Play the audio with mc.sfplay~ at the same time and route its tracks with mc.ear.select~."
				}
			},
			{
				"box": {
					"id": "obj-3",
					"maxclass": "message",
					"patching_rect": [
						15.0,
						100.0,
						110.0,
						22.0
					],
					"text": "read myfile.wav",
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
						100.0,
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
						100.0,
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
						100.0,
						45.0,
						22.0
					],
					"text": "seek 0",
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
						100.0,
						65.0,
						22.0
					],
					"text": "time 1500",
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
						358.0,
						100.0,
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
					"id": "obj-9",
					"maxclass": "message",
					"patching_rect": [
						408.0,
						100.0,
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
					"id": "obj-10",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						140.0,
						60.0,
						22.0
					],
					"text": "ear.adm",
					"numinlets": 1,
					"numoutlets": 4,
					"outlettype": [
						"",
						"",
						"",
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-11",
					"maxclass": "comment",
					"patching_rect": [
						85.0,
						140.0,
						500.0,
						20.0
					],
					"text": "outlets: objects messages, direct messages, hoa messages, info (file, programmes, items, warnings, position, end)"
				}
			},
			{
				"box": {
					"id": "obj-12",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						180.0,
						170.0,
						22.0
					],
					"text": "mc.sfplay~ 12 @autostart 0",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"multichannelsignal",
						"bang"
					]
				}
			},
			{
				"box": {
					"id": "obj-13",
					"maxclass": "message",
					"patching_rect": [
						195.0,
						180.0,
						95.0,
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
					"id": "obj-14",
					"maxclass": "comment",
					"patching_rect": [
						300.0,
						180.0,
						480.0,
						20.0
					],
					"text": "the audio: one mc.sfplay~ for the file, started together with the transport (send 1 / 0 to it)"
				}
			},
			{
				"box": {
					"id": "obj-15",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						220.0,
						90.0,
						22.0
					],
					"text": "mc.ear.select~",
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
						115.0,
						220.0,
						90.0,
						22.0
					],
					"text": "mc.ear.select~",
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
						215.0,
						220.0,
						90.0,
						22.0
					],
					"text": "mc.ear.select~",
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
						315.0,
						220.0,
						480.0,
						20.0
					],
					"text": "the 'tracks' messages from ear.adm set the channels each renderer gets"
				}
			},
			{
				"box": {
					"id": "obj-19",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						260.0,
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
					"id": "obj-20",
					"maxclass": "newobj",
					"patching_rect": [
						185.0,
						260.0,
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
					"id": "obj-21",
					"maxclass": "newobj",
					"patching_rect": [
						345.0,
						260.0,
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
					"id": "obj-22",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						300.0,
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
					"id": "obj-23",
					"maxclass": "newobj",
					"patching_rect": [
						500.0,
						140.0,
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
					"id": "obj-24",
					"maxclass": "comment",
					"patching_rect": [
						15.0,
						340.0,
						780.0,
						47.0
					],
					"text": "Writing: 'record' captures the object messages sent to ear.adm (the same setvalue / applyvalues / parameter messages the renderers take; send them to both), timestamped from the moment of 'record'; 'stop' ends the capture. Record the object audio with mc.sfrecord~ at the same time, then 'write out.wav in.wav' copies that audio into a BW64 file with the captured timeline as ADM metadata (one Objects audioObject per track). 'writexml out.xml' writes the metadata alone."
				}
			},
			{
				"box": {
					"id": "obj-25",
					"maxclass": "message",
					"patching_rect": [
						15.0,
						400.0,
						45.0,
						22.0
					],
					"text": "record",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-26",
					"maxclass": "message",
					"patching_rect": [
						70.0,
						400.0,
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
					"id": "obj-27",
					"maxclass": "message",
					"patching_rect": [
						115.0,
						400.0,
						140.0,
						22.0
					],
					"text": "setvalue 1 azimuth 30",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-28",
					"maxclass": "message",
					"patching_rect": [
						265.0,
						400.0,
						130.0,
						22.0
					],
					"text": "setvalue 1 ramp 250",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-29",
					"maxclass": "message",
					"patching_rect": [
						405.0,
						400.0,
						110.0,
						22.0
					],
					"text": "name 1 voice",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-30",
					"maxclass": "message",
					"patching_rect": [
						525.0,
						400.0,
						195.0,
						22.0
					],
					"text": "write out.wav recorded.wav",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-31",
					"maxclass": "message",
					"patching_rect": [
						15.0,
						430.0,
						100.0,
						22.0
					],
					"text": "writexml out.xml",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-32",
					"maxclass": "message",
					"patching_rect": [
						125.0,
						430.0,
						40.0,
						22.0
					],
					"text": "clear",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-33",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						470.0,
						190.0,
						22.0
					],
					"text": "ear.adm @chans 2 @ramp 10",
					"numinlets": 1,
					"numoutlets": 4,
					"outlettype": [
						"",
						"",
						"",
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-34",
					"maxclass": "comment",
					"patching_rect": [
						215.0,
						470.0,
						560.0,
						33.0
					],
					"text": "@chans: objects captured (one per track of the recorded audio); @ramp: default interpolation time written for changes; @programmename: the audioProgramme name"
				}
			},
			{
				"box": {
					"id": "obj-35",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						510.0,
						40.0,
						22.0
					],
					"text": "print",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": []
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"destination": [
						"obj-10",
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
						"obj-10",
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
						"obj-10",
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
						"obj-10",
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
						"obj-10",
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
						"obj-10",
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
						"obj-10",
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
						"obj-23",
						0
					],
					"source": [
						"obj-10",
						3
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-12",
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
						"obj-15",
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
						"obj-16",
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
						"obj-17",
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
						"obj-15",
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
						"obj-16",
						0
					],
					"source": [
						"obj-10",
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
						"obj-10",
						2
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
						"obj-10",
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
						"obj-10",
						1
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
						"obj-10",
						2
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
						"obj-20",
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
						"obj-21",
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
						"obj-22",
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
						"obj-22",
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
						"obj-22",
						0
					],
					"source": [
						"obj-21",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-33",
						0
					],
					"source": [
						"obj-25",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-33",
						0
					],
					"source": [
						"obj-26",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-33",
						0
					],
					"source": [
						"obj-27",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-33",
						0
					],
					"source": [
						"obj-28",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-33",
						0
					],
					"source": [
						"obj-29",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-33",
						0
					],
					"source": [
						"obj-30",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-33",
						0
					],
					"source": [
						"obj-31",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-33",
						0
					],
					"source": [
						"obj-32",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-35",
						0
					],
					"source": [
						"obj-33",
						3
					]
				}
			}
		]
	}
}
