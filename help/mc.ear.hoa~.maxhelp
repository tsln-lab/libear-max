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
			780.0,
			350.0
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
		"description": "Decode an ambisonic scene (ADM HOA) to loudspeaker signals with libear (ITU-R BS.2127).",
		"digest": "Decode an ambisonic scene to loudspeaker signals",
		"tags": [
			"spatial audio",
			"ADM",
			"ambisonics",
			"mc"
		],
		"style": "",
		"subpatcher_template": "",
		"assistshowspatchername": 0,
		"boxes": [
			{
				"box": {
					"id": "obj-1",
					"maxclass": "comment",
					"patching_rect": [
						15.0,
						10.0,
						740.0,
						20.0
					],
					"text": "mc.ear.hoa~ \u2014 decode an ambisonic scene (ADM HOA) to a multichannel loudspeaker signal (libear, ITU-R BS.2127)"
				}
			},
			{
				"box": {
					"id": "obj-2",
					"maxclass": "comment",
					"patching_rect": [
						15.0,
						35.0,
						740.0,
						33.0
					],
					"text": "The input carries the ambisonic components in ACN order (W Y Z X for first order), SN3D by default. The output has one channel per loudspeaker of the layout given as argument; sum it with mc.ear.objects~ and mc.ear.direct~ to mix scene-, object- and channel-based audio.",
					"linecount": 2
				}
			},
			{
				"box": {
					"id": "obj-3",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						90.0,
						80.0,
						22.0
					],
					"text": "cycle~ 220",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						"signal"
					]
				}
			},
			{
				"box": {
					"id": "obj-4",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						120.0,
						50.0,
						22.0
					],
					"text": "*~ 0.1",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						"signal"
					]
				}
			},
			{
				"box": {
					"id": "obj-5",
					"maxclass": "comment",
					"patching_rect": [
						250.0,
						150.0,
						400.0,
						20.0
					],
					"text": "a plane wave from the front at first order: W = X = signal, Y = Z = 0"
				}
			},
			{
				"box": {
					"id": "obj-6",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						150.0,
						120.0,
						22.0
					],
					"text": "mc.pack~ 4",
					"numinlets": 4,
					"numoutlets": 1,
					"outlettype": [
						"multichannelsignal"
					]
				}
			},
			{
				"box": {
					"id": "obj-7",
					"maxclass": "message",
					"patching_rect": [
						15.0,
						190.0,
						55.0,
						22.0
					],
					"text": "order 1",
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
						80.0,
						190.0,
						55.0,
						22.0
					],
					"text": "order 2",
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
						145.0,
						190.0,
						125.0,
						22.0
					],
					"text": "normalization SN3D",
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
						280.0,
						190.0,
						120.0,
						22.0
					],
					"text": "normalization N3D",
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
						410.0,
						190.0,
						125.0,
						22.0
					],
					"text": "normalization FuMa",
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
						545.0,
						190.0,
						55.0,
						22.0
					],
					"text": "align 0",
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
					"maxclass": "message",
					"patching_rect": [
						610.0,
						190.0,
						80.0,
						22.0
					],
					"text": "components",
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
					"maxclass": "message",
					"patching_rect": [
						700.0,
						190.0,
						65.0,
						22.0
					],
					"text": "channels",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-15",
					"maxclass": "newobj",
					"patching_rect": [
						15.0,
						240.0,
						180.0,
						22.0
					],
					"text": "mc.ear.hoa~ 4+5+0 @order 1",
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
						15.0,
						290.0,
						60.0,
						22.0
					],
					"text": "mc.dac~",
					"numinlets": 1,
					"numoutlets": 0
				}
			},
			{
				"box": {
					"id": "obj-17",
					"maxclass": "comment",
					"patching_rect": [
						250.0,
						290.0,
						400.0,
						20.0
					],
					"text": "output channels: M+030 M-030 M+000 LFE1 M+110 M-110 U+030 U-030 U+110 U-110"
				}
			},
			{
				"box": {
					"id": "obj-18",
					"maxclass": "comment",
					"patching_rect": [
						15.0,
						330.0,
						740.0,
						33.0
					],
					"text": "Attributes: order (0-8; (order+1)^2 input channels), normalization (SN3D, N3D, FuMa), ramp (ms), align (delay the output by the 255-sample latency of the object renderers, on by default). The LFE channel of the layout stays silent.",
					"linecount": 2
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"destination": [
						"obj-4",
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
						"obj-6",
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
						"obj-6",
						3
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
						"obj-15",
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
						"obj-16",
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
						"obj-15",
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
						"obj-15",
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
						"obj-15",
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
						"obj-15",
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
						"obj-15",
						0
					],
					"source": [
						"obj-14",
						0
					]
				}
			}
		],
		"dependency_cache": [],
		"autosave": 0
	}
}