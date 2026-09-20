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
[algogris~ @sources N @setup MySetup.xml]
 |                        \
[mc.live.gain~] -> [mc.dac~]   [print]   status: outputs, speakers, algorithm
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
| `multicore` | 0 | Parallel VBAP/MBAP |
| `attenuation`, `attenuation_db`, `attenuation_freq` | 0, 0 dB, 16 kHz | MBAP distance attenuation beyond radius 1 |

Changing an attribute rebuilds the renderer on the main thread; source positions are kept. MBAP setups take a moment to rebuild (the gain matrices are computed then), and audio is silent for a block while the new renderer is swapped in.

Speaker gains and high-pass filters from the setup file are applied, as in SpatGRIS. Not supported: direct outs, solo/mute from project files, pink noise, and SpatGRIS's newer SOFA-based binaural (this builds the AlgoGRIS `main` branch, which uses the KEMAR set).

## Speaker setups

`setups/` holds the setups that ship with the package. `Dome_default_speaker_setup.xml` and `Cube_default_speaker_setup.xml` are copied from AlgoGRIS when you configure the build; `Cube_7.1.4_speaker_setup.xml` is part of this repo.

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

### Testing without Max

`tests/run_engine_smoke.sh` builds the engine (everything except the Max glue) against a Linux or macOS AlgoGRIS build and checks each render mode: speakers, MBAP, legacy messages, binaural left/right, rebuilds.

## Layout

| Path | |
|---|---|
| `source/projects/algogris_tilde/algogris_engine.*` | AlgoGRIS host code (what SpatGRIS's audio processor and OSC input do), no Max types |
| `source/projects/algogris_tilde/algogris_tilde.cpp` | The Min object: attributes, messages, MC output count |
| `source/min-api` | Min API (submodule) |
| `help/algogris~.maxhelp` | Help patcher |
