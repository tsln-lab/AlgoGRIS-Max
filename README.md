# AlgoGRIS-Max

A Max package with one object, `algogris~`, that runs [AlgoGRIS](https://github.com/GRIS-UdeM/AlgoGRIS) (the spatialization DSP behind SpatGRIS) inside Max. No SpatGRIS, no virtual audio driver.

- **Algorithms:** VBAP (dome), MBAP (cube), hybrid (per source), plus binaural (KEMAR, 16 virtual speakers) and SpatGRIS's stereo reduction
- **Speaker setups:** SpatGRIS speaker setup files, legacy and current (4.x) formats
- **Control:** SpatGRIS's `/spat/serv` messages, so `[udpreceive 18032]` or a ControlGRIS emulator can drive it directly
- **License:** GPL-3.0-or-later, like AlgoGRIS

## Using it

```
[mc.pack~ N]                      one channel per source
 |
[algogris~ @sources N @setup MySetup.xml @monitor 1 @layout mylayout]
 |                  |                 \
 |                  |                  [print]  status: outputs, speakers, algorithm, monitor
 |                  [mc.dac~ 13 14]    binaural monitor, 2 channels
 [mc.live.gain~] -> [mc.dac~]          speaker feeds
```

**Output:** channel *n* is the speaker with output patch *n*, so the channel count is the highest output patch in the setup. In `binaural` and `stereo` render modes the output is 2 channels.

### Messages (left inlet)

The same as SpatGRIS's `/spat/serv` OSC messages, without the address. See [spatgris-osc.md](../dsp-research/spatgris-osc.md) for the full protocol.

| Message | Arguments | Index |
|---|---|---|
| `car` | index x y z hspan vspan | 1-based |
| `pol` | index azimuth elevation radius hspan vspan (radians) | 1-based |
| `deg` | index azimuth elevation radius hspan vspan (degrees) | 1-based |
| `clr` | index | 1-based |
| `alg` | index `dome`\|`cube` (hybrid mode only) | 1-based |
| list | id azimuth elevation azimuthspan elevationspan distance [gain]: legacy ControlGRIS | 0-based |
| `reset` | id: legacy clear | 0-based |
| `/spat/serv …` | any of the above, as output by `[udpreceive]` | |
| `rebuild` | reload the speaker setup | |

Azimuth: 0 = front, clockwise. Elevation: 0 = horizon. Positions arrive at control rate; gains ramp over each signal vector (or glide, with `@interpolation`).

### Attributes

| Attribute | Default | |
|---|---|---|
| `setup` | Dome_default_speaker_setup.xml | Speaker setup: absolute path, a file in Max's search path, or a file in this package's `setups/` |
| `mode` | `setup` | `setup` (use the setup's own mode), `vbap`, `mbap`, `hybrid` |
| `render` | `speakers` | `speakers`, `binaural`, `stereo` |
| `sources` | 16 | Number of sources, 1–256 |
| `interpolation` | 0 | Gain smoothing, 0–1 |
| `gain` | 0 | Master gain, dB |
| `monitor` | 0 | Binaural monitor of the speaker feeds on the second outlet (see below) |
| `layout` | none | Name of a dictionary to publish the speaker layout into (see below) |
| `multicore` | 0 | Parallel VBAP/MBAP |
| `attenuation`, `attenuation_db`, `attenuation_freq` | 0, 0 dB, 16 kHz | MBAP distance attenuation beyond radius 1 |

Changing an attribute rebuilds the renderer on the main thread; source positions are kept. MBAP setups take a moment to rebuild (the gain matrices are computed then), and audio is silent for a block while the new renderer is swapped in.

Speaker gains and high-pass filters from the setup file are applied, as in SpatGRIS. Not supported: direct outs, solo/mute from project files, pink noise, and SpatGRIS's newer SOFA-based binaural (this builds the AlgoGRIS `main` branch, which uses the KEMAR set).

## Binaural monitor

`@monitor 1` renders a headphone version of **the speaker feeds themselves** on the second outlet: each speaker is convolved with the KEMAR response for its own direction, and the results are summed. It works with any layout, because only each speaker's direction matters.

It is much cheaper than a second object in `binaural` render mode, and it monitors the mix you are actually sending to the room, including per-speaker gains and high-pass. Measured on one core at 48 kHz, 8 sources, with the 7.1.4 setup:

| | CPU |
|---|---|
| Speaker feeds only | 1.1 % |
| Speaker feeds + binaural monitor (11 responses) | **3.7 %** |
| A second object in `binaural` mode | 92 % |

`binaural` render mode is expensive because it always convolves 16 virtual speakers, and with MBAP every one of them gets signal. The monitor convolves only the speakers you have.

Limits: responses are far-field, so speaker distance is not modelled; the KEMAR set covers elevations from −40° to +90°; and cost grows with speaker count (fine to roughly 30 speakers, too much for 93).

## Speaker layout dictionary

`@layout <name>` publishes the parsed speaker setup into a named Max dictionary, refreshed on every rebuild, as parallel arrays: `patch`, `x`, `y`, `z`, `azimuth`, `elevation`, `distance`, `directout`, `gain`, `highpass`, plus `speakers` (a count). Angles are degrees, azimuth 0 = front and positive clockwise.

Any object can read it — `dict.view` to inspect it, `js` or `jsui` to draw the room, a patch to route test tones by patch number — without a second copy of the setup parser.

## Speaker setups

`setups/` holds the setups that ship with the package. `Dome_default_speaker_setup.xml` and `Cube_default_speaker_setup.xml` are copied from AlgoGRIS when you configure the build; the two 7.1.4 setups are part of this repo.

### The two 7.1.4 setups

Both give the same 12 output channels in Dolby's order, and differ only in how sources are positioned:

| | `Cube_7.1.4_speaker_setup.xml` | `Dome_7.1.4_speaker_setup.xml` |
|---|---|---|
| Algorithm | MBAP | VBAP |
| Speakers | On a room box | On the unit sphere, at 7.1.4's angles |
| Sources | Room coordinates, inside or outside the speakers | Directions only; radius is ignored |
| Like | Atmos tools, room-centric | Classic surround panning around a sweet spot |
| Use when | You think in room positions, or move sources past the walls | You think in angles, or want VBAP's sharper phantom images |

Dome speaker angles: L/R at ±30°, C at 0°, sides at ±90°, rears at ±135°, and the four height speakers at ±45° and ±135° azimuth, 45° up. Nothing sits directly overhead, so a source straight up is shared by the four top speakers.

### Cube_7.1.4_speaker_setup.xml

A 7.1.4 room, in Dolby's channel order, so output channel *n* is the usual 7.1.4 channel *n*:

| Patch | Channel | Position (x right, y front, z up) |
|---|---|---|
| 1, 2 | L, R | front corners, ±0.76, 0.76, 0 |
| 3 | C | front centre |
| 4 | LFE | **direct out**: nothing is panned to it, and it stays silent (no bass management) |
| 5, 6 | Lss, Rss | side walls, ±0.76, 0, 0 |
| 7, 8 | Lrs, Rrs | rear corners |
| 9–12 | Ltf, Rtf, Ltr, Rtr | ceiling, z = 0.85 |

It's a Cube (MBAP) setup, so positions are room coordinates like the ones Atmos tools use, rather than angles around a sweet spot. The speakers sit where ITU-R BS.2127 places them for layout 4+7+0.

`DIFFUSION="0.0"` in the file makes panning as focused as MBAP allows: a source at a speaker's position is effectively that speaker alone, and one halfway between two speakers splits between them. Edit that value towards 1.0 for a wider, more diffuse spread.

**It is not a Dolby Atmos renderer.** MBAP weights every speaker by distance, so, for example, a source at the centre of the room comes from C and the two sides rather than from all speakers. Atmos's own panner interpolates separately along each axis. See `../dsp-research/mbap-max-msp.md`.

## Building

Needs CMake ≥ 3.30 and, on Windows, Visual Studio 2022 (C++ workload); on macOS, Xcode. AlgoGRIS is expected next to this folder (`../AlgoGRIS`, with its submodules checked out); set `-DALGOGRIS_DIR=` otherwise.

```sh
git submodule update --init --recursive
cmake -B build                       # Windows: add -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target algogris_tilde
```

The external lands in `externals/`. Configuring also copies the binaural data (`support/algogris-data/`) and two default speaker setups (`setups/`) from AlgoGRIS. Then copy or link this folder into Max's `Packages` folder.

**From WSL:** use the Windows CMake on this folder and keep the build folder on the Windows disk (outside OneDrive), e.g.
`cmake.exe -S \\wsl.localhost\Ubuntu\home\<you>\Projects\AlgoGRIS-Max -B %LOCALAPPDATA%\AlgoGRIS-Max\build -G "Visual Studio 17 2022" -A x64`.

### Releases

Pushing a tag like `v0.2.0` builds both platforms and publishes a GitHub release with `AlgoGRIS-Max-0.2.0.zip`, its SHA-256, install notes and generated release notes. A tag containing a hyphen (`v0.2.0-rc.1`) is published as a pre-release. A release fails rather than publishes if either platform's external is missing.

The package version comes from `git describe`, so it matches the tag. Building locally from WSL, Windows git refuses `\\wsl.localhost` paths ("dubious ownership") and the version falls back to 1.0.0; harmless, or run
`git.exe config --global --add safe.directory '%(prefix)///wsl.localhost/Ubuntu/home/<you>/Projects/AlgoGRIS-Max'`.

### Testing without Max

`tests/run_engine_smoke.sh` builds the engine (everything except the Max glue) against a Linux or macOS AlgoGRIS build and checks each render mode: speakers, MBAP, legacy messages, binaural left/right, rebuilds.

## Layout

| Path | |
|---|---|
| `source/projects/algogris_tilde/algogris_engine.*` | AlgoGRIS host code (what SpatGRIS's audio processor and OSC input do), no Max types |
| `source/projects/algogris_tilde/algogris_tilde.cpp` | The Min object: attributes, messages, MC output count |
| `source/min-api` | Min API (submodule) |
| `help/algogris~.maxhelp` | Help patcher |
