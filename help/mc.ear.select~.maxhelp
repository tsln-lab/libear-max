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
			260.0
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
		"description": "Select channels of a multichannel signal by number, to route the tracks of an ADM file to the renderer that handles them.",
		"digest": "Select channels by number",
		"tags": [
			"spatial audio",
			"ADM",
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
					"text": "mc.ear.select~ \u2014 select channels of a multichannel signal by number"
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
						33.0
					],
					"text": "'tracks 7 8 9 10' outputs input channels 7 to 10 as output channels 1 to 4 (1-based, in the order given). ear.adm reports the file tracks of each rendering item in this form, so one mc.sfplay~ feeds every renderer through an mc.ear.select~. The output channel count follows the selection when the audio is restarted."
				}
			},
			{
				"box": {
					"id": "obj-3",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						90.0,
						125.0,
						22.0
					],
					"text": "mc.noise~ @chans 8",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"multichannelsignal"
					]
				}
			},
			{
				"box": {
					"id": "obj-4",
					"maxclass": "message",
					"patching_rect": [
						145.0,
						90.0,
						70.0,
						22.0
					],
					"text": "tracks 7 8",
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
						225.0,
						90.0,
						80.0,
						22.0
					],
					"text": "tracks 3 1 2",
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
						315.0,
						90.0,
						65.0,
						22.0
					],
					"text": "tracks 20",
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
					"maxclass": "comment",
					"patching_rect": [
						390.0,
						90.0,
						300.0,
						20.0
					],
					"text": "a channel the input does not have is silent"
				}
			},
			{
				"box": {
					"id": "obj-8",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						150.0,
						120.0,
						22.0
					],
					"text": "mc.ear.select~ 7 8",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"multichannelsignal"
					]
				}
			},
			{
				"box": {
					"id": "obj-9",
					"maxclass": "comment",
					"patching_rect": [
						145.0,
						150.0,
						300.0,
						20.0
					],
					"text": "arguments: the initial selection"
				}
			},
			{
				"box": {
					"id": "obj-10",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						190.0,
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
					"id": "obj-11",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						230.0,
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
					"id": "obj-12",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						110.0,
						70.0,
						22.0
					],
					"text": "mc.*~ 0.1",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						"multichannelsignal"
					]
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"destination": [
						"obj-12",
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
						"obj-8",
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
						"obj-8",
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
						"obj-8",
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
						"obj-8",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-11",
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
						"obj-8",
						0
					],
					"source": [
						"obj-12",
						0
					]
				}
			}
		]
	}
}
