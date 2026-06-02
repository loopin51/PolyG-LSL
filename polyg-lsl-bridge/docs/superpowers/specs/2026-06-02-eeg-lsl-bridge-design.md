# PolyG → Python LSL Bridge — Design Spec

**Date:** 2026-06-02
**Status:** Approved (design); pending implementation plan
**Platform (v1):** Windows 64-bit

## 1. Purpose

Acquire EEG (and optionally other PolyG modalities) from a LAXTHA PolyG-series
device via the manufacturer `LXSM-D1WD10.dll`, and publish the channels as a
real-time **Lab Streaming Layer (LSL)** outlet from Python. A separate Python
**marker module** lets an experiment scenario push stimulus-onset markers into a
second LSL outlet, which LSL clock-aligns with the EEG stream at record time.

This is a **new standalone project**. The existing 3-PC Telescan automation
(`A_eeg_client.py`, `B_slide_client.py`, `C_controller.py`) is **untouched** and
unrelated to this system.

## 2. Decisions (locked)

| Topic | Decision |
|---|---|
| Marker mechanism | Software LSL marker stream (separate outlet); LSL clock-aligns with EEG. No hardware trigger wiring. The device's physical marking channel still arrives in every raw frame, but is not the marker source; whether to expose it in the EEG outlet is an open item (Section 13, default: drop). |
| Acquisition path | Keep the manufacturer C++ app (fix it); forward raw frames to a Python bridge. Do **not** call the DLL from Python via ctypes. |
| Project scope | New standalone project; existing 3-PC scripts untouched. |
| Device / channels | Configurable: device model + channel-index selection + labels. |
| Signal units | Convert to microvolts using a **user-supplied fixed front-end gain** × PGA gain (from `gain_idx`, manual Table-5). |
| C++↔Python transport | Two processes over **localhost UDP** with a small binary frame header. |
| Platform | Windows 64-bit for v1. macOS is a roadmap goal (Section 11). |

## 3. Why this shape

- The DLL delivers data by posting `WM_AcqUnitData` (`WM_USER+1`) to an `HWND`;
  it requires a running Win32 message pump. Reusing the manufacturer's working
  plumbing is far lower-risk than re-creating a hidden-window message pump in
  ctypes.
- Data rate is tiny (≤ ~65 KB/s even at 512 Hz × 32 ch), so localhost UDP
  serialization cost is negligible. Shared memory / named pipes would add
  Windows-specific complexity the data rate does not justify.
- Putting all LSL, scaling, config, and metadata logic in Python keeps the C++
  change minimal and concentrates the maintainable logic in one language.

## 4. Architecture

```
EEG PC (Windows 64-bit)
┌─────────────────────────────────────────────────────────────┐
│  PolyG device ──USB──► LXSM-D1WD10.dll                        │
│                          │ WM_AcqUnitData (float* frame)      │
│                          ▼                                    │
│   [C++ acquisition app]  (message pump preserved)            │
│     • correct init order + 0.1 s waits                        │
│     • NO printf in hot path, ONE reused UDP socket            │
│     • prepend 16-byte header, send raw frame to localhost     │
│                          │ localhost UDP :PORT                │
│                          ▼                                    │
│   [Python bridge]  eeg_bridge.py                              │
│     • parse + validate frame, de-interleave channels          │
│     • select configured channels, raw V → µV                  │
│     • pylsl EEG StreamOutlet (full metadata)                  │
└─────────────────────────────────────────────────────────────┘

Scenario side (wherever stimulus code runs)
┌─────────────────────────────────────────────────────────────┐
│   [Python marker module]  markers.py  (importable)           │
│     mk = MarkerStream("EEG_Scenario_Markers")                │
│     mk.push("scenario/choice1/onset")                        │
│        └─► pylsl Markers StreamOutlet (irregular rate)        │
└─────────────────────────────────────────────────────────────┘

Recorder (LabRecorder or custom) subscribes to BOTH outlets;
LSL estimates cross-stream / cross-PC clock offset → time-aligned.
```

A single shared `config.toml` is read by both the C++ app and the Python bridge.

## 5. localhost wire protocol

Fixed 16-byte little-endian header + channel-major float payload. The payload
order matches the DLL stream-memory layout exactly (`ch1×N, ch2×N, …, marking×N`),
so the C++ side prepends a header and forwards the DLL buffer verbatim.

```
offset size  field
0      u32   magic   = 0x4C58454D ('LXEM')
4      u16   version = 1
6      u16   num_channels        // e.g. 33 (32 + marking) or 17
8      u16   samples_per_channel // = 512 / max_channels (e.g. 16)
10     u16   flags               // reserved (0)
12     u32   seq                 // frame counter, +1 per WM_AcqUnitData
16     f32[num_channels * samples_per_channel]   // channel-major
```

- Max packet ≈ 33×16×4 + 16 = **2128 bytes** → one UDP datagram on loopback.
- `seq` lets the bridge detect dropped frames.
- `num_channels` / `samples_per_channel` let the bridge validate every packet
  against config before trusting it.

## 6. C++ acquisition app (delta from manufacturer sample)

Work on a **copy** of `Test_LXSM_D1WD10_VC2017`; the original directory is not
modified. Changes are localized:

1. **`OnStreamData`** — remove the per-frame `printf` loop (528 lines/frame =
   realtime violation per manual p.11). Replace the per-call socket creation with
   a single reused socket; prepend the header and send. Keep the
   `ACQPLOT_DLL_*` live plotting for operator monitoring.
2. **Socket lifecycle** — `WSAStartup` once + create socket once at init; close
   at exit. Out of the hot path.
3. **Init sequence** — read device params from `config.toml`; enforce the manual
   call order `Init_Device → Set_ADCMaxNumChannel → Set_SampleFreq →
   Set_PGA_SourceGroup`, with the required `Sleep(100)` after
   `Set_ADCMaxNumChannel`, `Set_SampleFreq`, and `Set_PGA*`. Fix the build
   conditional `#if(x64)` → `#ifdef _WIN64`.

What stays identical: all DLL calls, the `WM_AcqUnitData` message map, ACQPLOT
plotting, MFC scaffold. Estimated change: ~30 lines edited + a ~40-line
`Forwarder` helper.

## 7. Python bridge (`eeg_bridge.py`)

1. **Recv & validate** — bind `127.0.0.1:PORT`; check magic/version; compare
   `num_channels` / `samples_per_channel` to config; track `seq` gaps (log
   dropped-frame count, never crash).
2. **De-interleave** — reshape payload to `(num_channels, samples_per_channel)`
   (channel-major); select configured channel indices.
3. **Scale to µV** — `µV = raw_volts / (fixed_gain × pga_gain) × 1e6`, where
   `pga_gain` is resolved from `gain_idx` via manual Table-5.
4. **Push to LSL** — `push_chunk(samples × channels, timestamp=local_clock())`
   on an outlet declaring `nominal_srate = sample_freq`; LSL back-fills
   per-sample times. `StreamInfo`: `type='EEG'`, `channel_format=cf_float32`,
   stable `source_id`. `desc` metadata carries: per-channel labels,
   `unit='microvolts'`, device model, sample_freq, max_channels, gain_idx,
   pga_gain, fixed_gain (full provenance for downstream conversion/audit).

## 8. Marker module (`markers.py`)

Importable, independent of the bridge:

```python
from markers import MarkerStream
mk = MarkerStream(name="EEG_Scenario_Markers", source_id="scn-01")
mk.push("scenario/choice1/onset")   # timestamps with local_clock() immediately
```

- `StreamInfo(type='Markers', channel_count=1, nominal_srate=IRREGULAR_RATE,
  channel_format=cf_string)`.
- Push as close to the real stimulus call as possible; LSL cross-stream /
  cross-PC clock estimation aligns it to EEG samples at record time.
- Ships with a small runnable example scenario.

## 9. Shared config (`config.toml`)

```toml
[device]
model           = "PolyG-A"   # → device_id 14 (PolyG-E 15 / PolyG-I 16 / PolyG-U 17)
max_channels    = 32          # 2 / 4 / 8 / 16 / 32
sample_freq_idx = 9           # 2^idx Hz → 512 Hz
gain_idx        = 9           # Table-5 → pga_gain 4.25

[scale]
fixed_gain = 1.0              # USER-SUPPLIED front-end amp gain; used for µV conversion

[channels]
select = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21]  # device channel indices
labels = ["Fp1","Fp2","..."]                                      # len(labels) == len(select)

[transport]
host = "127.0.0.1"
port = 51234
```

Validation: `len(labels) == len(select)`; selected indices within device channel
count; `sample_freq` legal for `max_channels` (manual Table-2); `gain_idx` 0–15.

## 10. Error handling & edge cases

- **Realtime budget** — PolyG-A 32ch/512Hz frame interval ≈ 31 ms; per-frame
  work (parse + scale + push) stays well under it. `push_chunk` is non-blocking.
- **Dropped frames** — `seq` gap → log warning with count; continue (UDP is
  lossy by design; LSL consumers see a time gap).
- **Config/frame mismatch** — channel/sample mismatch → log error, skip frame
  (never push garbage).
- **C++ send failure** — log + continue acquisition; forwarding never kills the
  device thread.
- **Startup order independence** — outlets publish even with no consumer; LSL
  buffers. Bridge and recorder can start in any order.
- **Clock-sync caveat** — EEG and marker outlets may live on different PCs; rely
  on LSL time-sync (LabRecorder does this by default). Documented for users.

## 11. Roadmap — macOS native acquisition (future, out of v1 scope)

**Goal:** Connect a PolyG device directly to a MacBook and acquire via a C++
implementation, reusing this project's Python LSL/marker side unchanged.

**Reality:** `LXSM-D1WD10.dll` is a Windows-only PE binary bound to
`HWND`/`SendMessage`/message pump — it cannot run on macOS. The realistic paths:

1. Obtain a macOS / cross-platform driver or protocol source from LAXTHA, **or**
2. Talk to the device's USB communication chip directly (likely FTDI-class) via
   `libusb`, reverse-engineering the wire protocol.

**Design payoff:** The localhost wire protocol (Section 5) is the **portability
seam**. A macOS-native acquisition front-end only needs to emit the *same frame
format* to localhost; the Python bridge, LSL EEG outlet, marker module, and
config are reused with zero changes. Keep this boundary stable to preserve the
port.

## 12. Testing strategy

- **Fake-device generator** (`fake_device.py`) — emits synthetic frames in the
  exact wire format to localhost. Enables developing/testing the **entire Python
  side without the device, the C++ app, or Windows** (works on macOS).
- **Unit tests** — frame parse/validate, de-interleave correctness, µV math
  (known input → known µV), seq-gap detection, config loading + validation.
- **Integration test** — `fake_device → bridge → pylsl inlet`: assert received
  samples / labels / metadata; `MarkerStream.push → inlet` receives label +
  timestamp.
- **C++ smoke test** — on the EEG PC against the real device: run app + bridge,
  confirm valid frames at the expected rate with zero `seq` gaps at target
  config.

## 13. Open items to confirm during implementation

- The **fixed front-end gain** numeric value (user to supply from datasheet /
  calibration) for `[scale].fixed_gain`.
- Final **channel label montage** for the target device.
- Whether to include the **marking channel** as a passthrough data channel in the
  EEG outlet or drop it (default: drop; markers come from the software stream).

## 14. Component summary

| Component | Language | Responsibility | Depends on |
|---|---|---|---|
| Acquisition app (copy) | C++ / MFC | Device init + receive `WM_AcqUnitData` + forward framed UDP | `LXSM-D1WD10.dll`, ACQPLOT, config |
| `eeg_bridge.py` | Python | Parse/validate frames, µV scale, EEG LSL outlet | pylsl, config |
| `markers.py` | Python | Scenario-facing marker LSL outlet | pylsl |
| `fake_device.py` | Python | Synthetic frame source for tests | config, wire protocol |
| `config.toml` | — | Single source of device/scale/channel/transport config | — |
