# ambisonic

Stereo → SN3D/ACN ambisonic encoder. The DSP core is shared by a command-line
tool and a JUCE VST3 plugin ("The Potate").

## Requirements

- CMake 3.22 or newer
- A C++17 compiler (Visual Studio 2022+ / MSVC, Clang, or GCC)
- Git (JUCE 9.0.4 is fetched automatically on the first configure)

## Building the VST3 plugin

The default build produces a **3rd-order** (16-channel) plugin:

```sh
cmake -S . -B build
cmake --build build --config Release --target ambi_encoder_VST3
```

On Windows with the Visual Studio generator, specify the architecture:

```sh
cmake -S . -B build -A x64
cmake --build build --config Release --target ambi_encoder_VST3 -- /m
```

For single-config generators (Ninja / Makefiles), set the build type at
configure time instead of passing `--config`:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ambi_encoder_VST3
```

The plugin is copied into your per-user VST3 folder after the build
(`%LOCALAPPDATA%\Common\VST3\The Potate.vst3` on Windows).

## Compiling for a given ambisonic order

The ambisonic order is fixed at **configure time** by the `AMBI_ORDER` cache
variable (integer `0`–`7`, default `3`). The plugin's output channel count is
`(order + 1)²`.

| `AMBI_ORDER` | Output channels |
| :----------: | :-------------: |
| 0            | 1               |
| 1            | 4               |
| 2            | 9               |
| 3 (default)  | 16              |
| 4            | 25              |
| 5            | 36              |
| 6            | 49              |
| 7            | 64              |

Pass `-DAMBI_ORDER=<n>` when configuring:

```sh
cmake -S . -B build -DAMBI_ORDER=1
cmake --build build --config Release --target ambi_encoder_VST3
```

To change the order of an existing build directory, reconfigure it with a new
value and rebuild:

```sh
cmake -S . -B build -DAMBI_ORDER=5
cmake --build build --config Release --target ambi_encoder_VST3
```

The plugin is always installed as `The Potate.vst3`, so a rebuild overwrites the
previous order, and only one order can be installed at a time. To switch between
orders quickly without reconfiguring, keep a separate build directory per order
and rebuild the one you want:

```sh
cmake -S . -B build-order1 -DAMBI_ORDER=1
cmake -S . -B build-order3 -DAMBI_ORDER=3
cmake --build build-order1 --config Release --target ambi_encoder_VST3   # installs order 1
cmake --build build-order3 --config Release --target ambi_encoder_VST3   # installs order 3
```

The channel count is baked into the plugin, so a host project must expect the
matching bus size (order 3 → 16 channels, etc.).

## Exporting the VST3

The build produces a self-contained VST3 **bundle** — a directory, not a single
file — at `build/ambi_encoder_artefacts/Release/VST3/The Potate.vst3`. To export
it (for distribution, archiving, or installing on another machine), copy that
entire `.vst3` folder into the target's VST3 location (e.g.
`%LOCALAPPDATA%\Common\VST3` or `C:\Program Files\Common Files\VST3` on Windows,
`~/Library/Audio/Plug-Ins/VST3` on macOS) or zip the whole folder and unzip it on
the destination. Copy only the `.vst3` folder itself: the inner binary (e.g.
`Contents\x86_64-win\The Potate.vst3`) is not enough on its own, because the
bundle also carries `Contents\Resources\moduleinfo.json`. The post-build copy
into `%LOCALAPPDATA%\Common\VST3` is simply this same bundle installed for you —
whatever you distribute is exactly that folder.

## Command-line tool

The `ambisonic` executable uses the same DSP core:

```sh
cmake --build build --config Release --target ambisonic
```
