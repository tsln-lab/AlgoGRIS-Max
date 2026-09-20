{
 "patcher": {
  "fileversion": 1,
  "appversion": {
   "major": 9,
   "minor": 0,
   "revision": 0,
   "architecture": "x64",
   "modernui": 1
  },
  "classnamespace": "box",
  "rect": [
   100,
   80,
   860,
   760
  ],
  "boxes": [
   {
    "box": {
     "id": "title",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      12,
      300,
      30
     ],
     "text": "algogris~",
     "fontsize": 22,
     "fontface": 1
    }
   },
   {
    "box": {
     "id": "sub",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      44,
      560,
      20
     ],
     "text": "SpatGRIS spatialization (VBAP, MBAP, hybrid, binaural) from AlgoGRIS, as an MC object"
    }
   },
   {
    "box": {
     "id": "noise",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      20,
      90,
      50,
      22
     ],
     "text": "noise~",
     "outlettype": [
      "signal"
     ]
    }
   },
   {
    "box": {
     "id": "cyc",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      90,
      90,
      70,
      22
     ],
     "text": "cycle~ 330",
     "outlettype": [
      "signal"
     ]
    }
   },
   {
    "box": {
     "id": "g1",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20,
      120,
      50,
      22
     ],
     "text": "*~ 0.1",
     "outlettype": [
      "signal"
     ]
    }
   },
   {
    "box": {
     "id": "g2",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      90,
      120,
      50,
      22
     ],
     "text": "*~ 0.1",
     "outlettype": [
      "signal"
     ]
    }
   },
   {
    "box": {
     "id": "pack",
     "maxclass": "newobj",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20,
      155,
      120,
      22
     ],
     "text": "mc.pack~ 2",
     "outlettype": [
      "multichannelsignal"
     ]
    }
   },
   {
    "box": {
     "id": "c_src",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      150,
      155,
      380,
      20
     ],
     "text": "one channel per source: source 1 = noise, source 2 = sine"
    }
   },
   {
    "box": {
     "id": "m1",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20,
      200,
      170,
      22
     ],
     "text": "deg 1 -90. 0. 1. 0. 0.",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "cm1",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      200,
      200,
      260,
      20
     ],
     "text": "source 1 hard left"
    }
   },
   {
    "box": {
     "id": "m2",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20,
      228,
      170,
      22
     ],
     "text": "deg 1 90. 0. 1. 0. 0.",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "cm2",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      200,
      228,
      260,
      20
     ],
     "text": "source 1 hard right"
    }
   },
   {
    "box": {
     "id": "m3",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20,
      256,
      170,
      22
     ],
     "text": "deg 1 0. 45. 1. 0.2 0.2",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "cm3",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      200,
      256,
      260,
      20
     ],
     "text": "front, 45 deg up, some span"
    }
   },
   {
    "box": {
     "id": "m4",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20,
      284,
      170,
      22
     ],
     "text": "car 2 0. -1. 0. 0. 0.",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "cm4",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      200,
      284,
      260,
      20
     ],
     "text": "source 2 at the back (cartesian)"
    }
   },
   {
    "box": {
     "id": "m5",
     "maxclass": "message",
     "numinlets": 2,
     "numoutlets": 1,
     "patching_rect": [
      20,
      312,
      170,
      22
     ],
     "text": "clr 1",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "cm5",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      200,
      312,
      260,
      20
     ],
     "text": "clear source 1 (silent)"
    }
   },
   {
    "box": {
     "id": "udp",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      470,
      200,
      110,
      22
     ],
     "text": "udpreceive 18032",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "c_udp",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      470,
      224,
      260,
      34
     ],
     "text": "or drive it with SpatGRIS OSC (/spat/serv), e.g. from ControlGRIS"
    }
   },
   {
    "box": {
     "id": "a_render",
     "maxclass": "attrui",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      470,
      270,
      210,
      22
     ],
     "outlettype": [
      ""
     ],
     "attr": "render"
    }
   },
   {
    "box": {
     "id": "a_mode",
     "maxclass": "attrui",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      470,
      296,
      210,
      22
     ],
     "outlettype": [
      ""
     ],
     "attr": "mode"
    }
   },
   {
    "box": {
     "id": "a_setup",
     "maxclass": "attrui",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      470,
      322,
      210,
      22
     ],
     "outlettype": [
      ""
     ],
     "attr": "setup"
    }
   },
   {
    "box": {
     "id": "a_sources",
     "maxclass": "attrui",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      470,
      348,
      210,
      22
     ],
     "outlettype": [
      ""
     ],
     "attr": "sources"
    }
   },
   {
    "box": {
     "id": "a_monitor",
     "maxclass": "attrui",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      470,
      374,
      210,
      22
     ],
     "outlettype": [
      ""
     ],
     "attr": "monitor"
    }
   },
   {
    "box": {
     "id": "obj",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 3,
     "patching_rect": [
      20,
      420,
      660,
      22
     ],
     "text": "algogris~ @sources 2 @setup Cube_7.1.4_speaker_setup.xml @monitor 1 @layout algogris_layout",
     "outlettype": [
      "multichannelsignal",
      "multichannelsignal",
      ""
     ]
    }
   },
   {
    "box": {
     "id": "gain",
     "maxclass": "mc.live.gain~",
     "numinlets": 1,
     "numoutlets": 5,
     "patching_rect": [
      20,
      470,
      136,
      90
     ],
     "outlettype": [
      "multichannelsignal",
      "",
      "float",
      "list",
      ""
     ]
    }
   },
   {
    "box": {
     "id": "dac",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      575,
      70,
      22
     ],
     "text": "mc.dac~"
    }
   },
   {
    "box": {
     "id": "c_out",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      165,
      470,
      290,
      45
     ],
     "text": "speaker feeds: channel n = output patch n (12 channels for the 7.1.4 setup). Start with the gain down."
    }
   },
   {
    "box": {
     "id": "mgain",
     "maxclass": "mc.live.gain~",
     "numinlets": 1,
     "numoutlets": 5,
     "patching_rect": [
      470,
      470,
      136,
      90
     ],
     "outlettype": [
      "multichannelsignal",
      "",
      "float",
      "list",
      ""
     ]
    }
   },
   {
    "box": {
     "id": "mdac",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      470,
      575,
      100,
      22
     ],
     "text": "mc.dac~ 13 14"
    }
   },
   {
    "box": {
     "id": "c_mon",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      615,
      470,
      210,
      60
     ],
     "text": "binaural monitor of those same speaker feeds, 2 channels. Send it to your headphone outputs (13/14 here)."
    }
   },
   {
    "box": {
     "id": "print",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      200,
      620,
      110,
      22
     ],
     "text": "print algogris~"
    }
   },
   {
    "box": {
     "id": "c_print",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      200,
      645,
      320,
      20
     ],
     "text": "status: outputs, speakers, algorithm, monitor (or an error)"
    }
   },
   {
    "box": {
     "id": "dv",
     "maxclass": "newobj",
     "numinlets": 1,
     "numoutlets": 1,
     "patching_rect": [
      20,
      680,
      170,
      22
     ],
     "text": "dict.view algogris_layout",
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "c_dict",
     "maxclass": "comment",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      200,
      680,
      380,
      40
     ],
     "text": "the speaker layout, published as a dictionary: patch, x/y/z, azimuth, elevation, distance, directout, gain, highpass"
    }
   }
  ],
  "lines": [
   {
    "patchline": {
     "source": [
      "noise",
      0
     ],
     "destination": [
      "g1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "cyc",
      0
     ],
     "destination": [
      "g2",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "g1",
      0
     ],
     "destination": [
      "pack",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "g2",
      0
     ],
     "destination": [
      "pack",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "pack",
      0
     ],
     "destination": [
      "obj",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "m1",
      0
     ],
     "destination": [
      "obj",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "m2",
      0
     ],
     "destination": [
      "obj",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "m3",
      0
     ],
     "destination": [
      "obj",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "m4",
      0
     ],
     "destination": [
      "obj",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "m5",
      0
     ],
     "destination": [
      "obj",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "udp",
      0
     ],
     "destination": [
      "obj",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "a_render",
      0
     ],
     "destination": [
      "obj",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "a_mode",
      0
     ],
     "destination": [
      "obj",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "a_setup",
      0
     ],
     "destination": [
      "obj",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "a_sources",
      0
     ],
     "destination": [
      "obj",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "a_monitor",
      0
     ],
     "destination": [
      "obj",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj",
      0
     ],
     "destination": [
      "gain",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "gain",
      0
     ],
     "destination": [
      "dac",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj",
      1
     ],
     "destination": [
      "mgain",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "mgain",
      0
     ],
     "destination": [
      "mdac",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj",
      2
     ],
     "destination": [
      "print",
      0
     ]
    }
   }
  ]
 }
}