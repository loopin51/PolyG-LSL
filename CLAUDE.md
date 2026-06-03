# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

A real-time EEG acquisition pipeline that streams a LAXTHA **PolyG** device into
**Lab Streaming Layer (LSL)**. It has two active components plus an archive:

- **`polyg-lsl-bridge/`** — the **main** system. A Python package (`polyg_lsl`) that
  receives raw device frames over localhost UDP, parses/validates them, selects channels,
  converts to **µV**, and pushes them to an **LSL EEG outlet**. It also provides a
  scenario-facing **LSL Markers** outlet for stimulus-onset events.
- **`PolyG_DLL_API/`** — the **device-side front-end**. A manufacturer C++ MFC app
  (Visual Studio 2017) driving `LXSM-D1WD10.dll`. The DLL posts float frames via
  `WM_AcqUnitData`; the View handler forwards each frame **as-is** to the Python bridge
  over UDP (`Forwarder` class). It does **no** scaling or LSL work.
- **`legacy/`** — the **inactive** former system: a three-PC Telescan/PowerPoint
  automation script collection (`A_eeg_client.py`, `B_slide_client.py`, `C_controller.py`,
  `pick_coords.py`, `config.py`, `scenario.yaml`). Unrelated to the LSL bridge; kept for
  reference only. **Do not** treat it as the current system unless explicitly asked.

The C↔Python boundary is a **fixed binary wire format (LXEM)**. C++ only ships frames to
localhost; channel selection, µV conversion, LSL, and all config live in Python — so the
acquisition front-end can later be replaced (e.g. a Mac-native capture) without touching
the Python side. Comments/UI in the C++ app are **CP949**-encoded Korean.

## Running

Everything below runs inside `polyg-lsl-bridge/`. **Python 3.11+** is required (stdlib `tomllib`).

```bash
cd polyg-lsl-bridge
python3 -m venv .venv
. .venv/bin/activate            # Windows PowerShell: .venv\Scripts\Activate.ps1
pip install -e ".[dev]"         # numpy, pylsl, + pytest
python -m pytest -q             # expect "24 passed" (LSL tests skip without liblsl)
```

Two console entry points are installed (defined in `pyproject.toml [project.scripts]`):
`polyg-bridge` → `polyg_lsl.bridge:main`, `polyg-fake-device` → `polyg_lsl.fake_device:main`.

**Offline run (no device/C++, works on any OS)** — three terminals, each after
`cd polyg-lsl-bridge && . .venv/bin/activate`:

```bash
polyg-bridge --config config.toml        # LSL EEG outlet; listens on 127.0.0.1:51234
polyg-fake-device --config config.toml   # synthetic LXEM frames (--duration N to auto-stop)
python examples/example_scenario.py      # marker outlet demo
```

**Real-device run (EEG PC, Windows x64):** identical, but replace `polyg-fake-device`
with the built C++ app. Build/wire-up steps: `PolyG_DLL_API/BUILD_ko.md` and
`polyg-lsl-bridge/cpp/README.md`. In the app: `Init Device` → pick a **Set Sample …**
menu (required) → `Start Stream`.

`pylsl` needs native **liblsl**. If import fails: macOS `brew install labstreaminglayer/tap/lsl`;
Windows/Linux install a binary from the liblsl releases.

## Configuration that must be set per-deployment

`polyg-lsl-bridge/config.toml` is the single source of truth, **mirrored** into the C++
`PolyG_DLL_API/BridgeConfig.h` (v1 C++ does not parse TOML). Keep them in sync:

- `[device] model / max_channels / sample_freq_idx / gain_idx` — device setup. `gain_idx`
  feeds the µV conversion.
- `[scale] fixed_gain` — preamp fixed gain; **must be filled in** from the device spec/cal
  (default `1.0` is a placeholder).
- `[channels] select / labels` — 1-based device channels to emit and their labels;
  the two lists **must be equal length**.
- `[transport] host / port` — localhost UDP target (default `127.0.0.1:51234`); must equal
  `BridgeConfig.h`'s `BRIDGE_HOST` / `BRIDGE_PORT`.

> ⚠️ `[device].gain_idx` and C++ `BRIDGE_PGA_GAIN_IDX` **must be identical**. If they differ,
> the device's actual gain and Python's µV scaling disagree and **µV values are wrong**.

## Wire protocol (LXEM, localhost UDP)

One UDP datagram per frame: a **16-byte little-endian header + channel-major float32
payload**. The C++ `Forwarder::Send` layout and Python `protocol.py` must match exactly
(`HEADER_FORMAT = "<IHHHHI"`, `MAGIC = 0x4C58454D` 'LXEM', `VERSION = 1`):

| offset | size | field |
|--------|------|-------|
| 0 | 4 | magic `0x4C58454D` |
| 4 | 2 | version (`1`) |
| 6 | 2 | num_channels (C++ `DISPMAXCH`) |
| 8 | 2 | samples_per_channel (C++ `DISPDATANUM`) |
| 10 | 2 | flags (`0`) |
| 12 | 4 | seq |
| 16 | payload | `num_channels × samples_per_channel × 4` bytes |

## Python package layout (`src/polyg_lsl/`)

- `protocol.py` — LXEM `parse_header` / `parse_frame` / `build_frame`; raises `FrameError`
  on bad magic/version/length.
- `scaling.py` — `raw_to_microvolts(volts, fixed_gain, pga_gain)`:
  `µV = volts / (fixed_gain × pga_gain) × 1e6`.
- `config.py` — `Config` dataclass + `load_config()` with validation (`ConfigError`);
  `expected_num_channels`, `expected_samples_per_channel`, `select_zero_based`.
- `bridge.py` — `EEGBridge` owns the EEG `StreamOutlet`; `handle_datagram` parses a frame,
  selects channels, converts µV, and `push_chunk(..., local_clock())`. `SeqTracker` counts
  dropped frames; `build_stream_info` constructs the LSL `StreamInfo`. `main()` is the CLI.
- `markers.py` — `MarkerStream(name, source_id).push(label, timestamp=None)`: an
  irregular-rate single-channel string Markers outlet; stamps `local_clock()` if no ts.
- `fake_device.py` — synthetic LXEM frame source for offline testing.

## How the pieces fit together (the non-obvious parts)

- **C++ is a dumb forwarder.** `OnStreamData` draws the waveform (ACQPLOT) then calls
  `m_forwarder.Send(m_seq++, frame, DISPMAXCH, DISPDATANUM)`. No scaling/LSL in C++.

- **Winsock header order is load-bearing.** `Forwarder.h` includes `<winsock2.h>`, which
  must precede `<windows.h>`. So the View header only forward-declares `Forwarder* m_forwarder;`
  and includes `Forwarder.h` from the `.cpp` only. **`Forwarder.cpp` is excluded from
  precompiled headers** (`<PrecompiledHeader>NotUsing</PrecompiledHeader>` in the vcxproj):
  adding `#include "stdafx.h"` would pull MFC's `windows.h`→`winsock.h` in *before*
  `winsock2.h` and break the build (this is the fix for the C1010 PCH error — don't undo it).

- **Marker timing.** Call `MarkerStream.push()` as close as possible to the actual stimulus
  presentation. Start the LSL recorder **before** the scenario so the first marker is captured.
  EEG and marker outlets may live on different PCs; LSL clock-corrects and aligns them.

- **Frame/seq diagnostics.** The bridge logs `frame/config mismatch` when an incoming frame
  disagrees with `config.toml` (channel count / model), and `dropped N frame(s)` from the
  seq gap (occasional UDP loss at high sampling is normal).

## Testing

```bash
cd polyg-lsl-bridge && python -m pytest -v   # 24 tests
```

Protocol/scaling/config tests run without liblsl. LSL round-trip tests
(`test_markers`, `test_integration_lsl`, `test_end_to_end`) **auto-skip** when liblsl is absent.

## Editing guidance

- Changing the frame layout means editing **both** `protocol.py` and C++ `Forwarder.cpp`
  (and bumping `VERSION` in both) — they are a contract.
- Adding/removing a config field: update `config.py` (parse + validate) and, if the C++ side
  needs it, mirror it into `BridgeConfig.h`.
- The C++ sources are **CP949**; read them with `iconv -f CP949 -t UTF-8 <file>` so Korean
  comments don't garble. Docs (`docs/*.md`) and the root README are UTF-8.
- After changing the C++ app, rebuild **x64** in Visual Studio 2017. Re-select a sampling
  frequency menu after changing the channel count (manufacturer constraint).

## Documentation

Start at `docs/index.md`. Key pages: `docs/cpp-acquisition-app.md` (DLL API, View handlers,
Forwarder), `docs/network-and-streams.md` (UDP packet ↔ LSL flow),
`docs/protocols-and-formats.md`. The root `readme.md` is the user-facing guide; the bridge's
own `polyg-lsl-bridge/README.md` has the deepest install/run/config reference.
