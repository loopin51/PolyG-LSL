# PolyG → Python LSL Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Acquire PolyG EEG via the manufacturer C++/DLL app, forward framed UDP to a Python bridge that publishes an LSL EEG outlet in microvolts, plus an importable Python marker outlet for scenario stimulus markers.

**Architecture:** Two processes on the EEG PC. A fixed 64-bit C++ acquisition app receives `WM_AcqUnitData` frames from `LXSM-D1WD10.dll` and forwards each frame (16-byte header + channel-major float payload) over localhost UDP. A Python bridge parses/validates frames, de-interleaves channels, scales raw ADC volts to µV, and pushes to a `pylsl` EEG outlet. A separate Python `MarkerStream` class publishes a string Markers outlet. The localhost wire protocol is the portability seam for a future macOS-native acquisition front-end.

**Tech Stack:** Python 3.11+ (stdlib `tomllib`, `struct`, `socket`, `logging`), `numpy`, `pylsl`; C++/MFC + Winsock2 (Windows 64-bit). Tests via `pytest`.

---

## File Structure

```
polyg-lsl-bridge/
├── pyproject.toml                     # package metadata, deps, console scripts, pytest config
├── config.toml                        # example shared config
├── README.md                          # Python-side usage
├── src/polyg_lsl/
│   ├── __init__.py
│   ├── protocol.py                    # wire protocol: constants, GAIN_TABLE, DEVICE_IDS, FrameHeader, parse/build
│   ├── scaling.py                     # raw volts → µV
│   ├── config.py                      # Config dataclass + load_config + validation
│   ├── bridge.py                      # frame_to_microvolts, SeqTracker, build_stream_info, EEGBridge, main()
│   ├── markers.py                     # MarkerStream
│   └── fake_device.py                 # synthetic frame generator + main()
├── examples/
│   └── example_scenario.py            # runnable marker demo
├── tests/
│   ├── test_protocol.py
│   ├── test_scaling.py
│   ├── test_config.py
│   ├── test_bridge.py                 # pure logic: frame_to_microvolts, SeqTracker
│   ├── test_markers.py                # pylsl round-trip (importorskip)
│   └── test_integration_lsl.py        # bridge outlet → inlet round-trip (importorskip)
└── cpp/
    ├── Forwarder.h                    # reusable UDP framed sender
    ├── Forwarder.cpp
    ├── BridgeConfig.h                 # constants mirroring config.toml (host/port/device params)
    └── README.md                      # how to copy the Test app + apply edits + smoke test
```

**Responsibility boundaries:** `protocol.py` owns byte layout only (no I/O). `scaling.py` is one pure math function. `config.py` owns parsing/validation and derived values. `bridge.py` separates pure transforms (`frame_to_microvolts`, `SeqTracker`) from LSL/socket I/O (`EEGBridge`). `markers.py` is independent of the bridge. `fake_device.py` reuses `protocol.build_frame` so the test source and real C++ source agree by construction.

**Note on C++ config:** v1 C++ reads its parameters from a hand-edited `cpp/BridgeConfig.h` that must mirror `config.toml` (full TOML parsing in C++ is deferred). This is a deliberate simplification of the spec's "single shared file" ideal to avoid adding a TOML dependency to the MFC project; documented in `cpp/README.md`.

---

## Task 1: Project scaffold

**Files:**
- Create: `polyg-lsl-bridge/pyproject.toml`
- Create: `polyg-lsl-bridge/src/polyg_lsl/__init__.py`
- Create: `polyg-lsl-bridge/tests/__init__.py`

- [ ] **Step 1: Create `pyproject.toml`**

```toml
[build-system]
requires = ["setuptools>=68"]
build-backend = "setuptools.build_meta"

[project]
name = "polyg-lsl"
version = "0.1.0"
description = "PolyG device -> Python LSL bridge"
requires-python = ">=3.11"
dependencies = ["numpy>=1.24", "pylsl>=1.16"]

[project.optional-dependencies]
dev = ["pytest>=7"]

[project.scripts]
polyg-bridge = "polyg_lsl.bridge:main"
polyg-fake-device = "polyg_lsl.fake_device:main"

[tool.setuptools.packages.find]
where = ["src"]

[tool.pytest.ini_options]
testpaths = ["tests"]
```

- [ ] **Step 2: Create empty package files**

`src/polyg_lsl/__init__.py`:

```python
"""PolyG device to Python LSL bridge."""

__version__ = "0.1.0"
```

`tests/__init__.py`: empty file.

- [ ] **Step 3: Create and activate a virtualenv, install dev deps**

Run:
```bash
cd polyg-lsl-bridge
python3.11 -m venv .venv
. .venv/bin/activate
pip install -e ".[dev]"
```
Expected: installs numpy, pylsl, pytest with no errors. (If `pylsl` cannot find liblsl at runtime later, install the native lib: macOS `brew install labstreaminglayer/tap/lsl`, Linux use the bundled wheel binary.)

- [ ] **Step 4: Verify pytest runs (no tests yet)**

Run: `python -m pytest -q`
Expected: `no tests ran` (exit code 5) — confirms pytest is wired up.

- [ ] **Step 5: Commit**

```bash
git add pyproject.toml src/polyg_lsl/__init__.py tests/__init__.py
git commit -m "chore: scaffold polyg_lsl package"
```

---

## Task 2: Wire protocol (`protocol.py`)

**Files:**
- Create: `src/polyg_lsl/protocol.py`
- Test: `tests/test_protocol.py`

- [ ] **Step 1: Write failing tests**

`tests/test_protocol.py`:

```python
import numpy as np
import pytest

from polyg_lsl.protocol import (
    MAGIC, VERSION, HEADER_SIZE, GAIN_TABLE, DEVICE_IDS,
    FrameError, FrameHeader, build_frame, parse_header, parse_frame,
)


def _frame(nch=3, spc=4, seq=7):
    data = np.arange(nch * spc, dtype="<f4").reshape(nch, spc)
    return build_frame(nch, spc, seq, data), data


def test_header_size_is_16():
    assert HEADER_SIZE == 16


def test_gain_table_known_values():
    assert GAIN_TABLE[4] == 1.0
    assert GAIN_TABLE[15] == 17.0
    assert len(GAIN_TABLE) == 16


def test_device_ids():
    assert DEVICE_IDS["PolyG-A"] == 14
    assert DEVICE_IDS["PolyG-I"] == 16


def test_parse_header_roundtrip():
    buf, _ = _frame(nch=3, spc=4, seq=7)
    h = parse_header(buf)
    assert h == FrameHeader(MAGIC, VERSION, 3, 4, 0, 7)


def test_parse_frame_shape_and_values():
    buf, data = _frame(nch=3, spc=4, seq=7)
    h, arr = parse_frame(buf)
    assert h.num_channels == 3 and h.samples_per_channel == 4
    assert arr.shape == (3, 4)
    np.testing.assert_array_equal(arr, data)


def test_bad_magic_raises():
    buf, _ = _frame()
    corrupted = b"\x00\x00\x00\x00" + buf[4:]
    with pytest.raises(FrameError):
        parse_header(corrupted)


def test_truncated_payload_raises():
    buf, _ = _frame(nch=3, spc=4)
    with pytest.raises(FrameError):
        parse_frame(buf[:-4])
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `python -m pytest tests/test_protocol.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'polyg_lsl.protocol'`.

- [ ] **Step 3: Implement `protocol.py`**

`src/polyg_lsl/protocol.py`:

```python
"""LXEM localhost wire protocol: byte layout only, no socket I/O."""
from __future__ import annotations

import struct
from dataclasses import dataclass

import numpy as np

MAGIC = 0x4C58454D  # 'LXEM' little-endian
VERSION = 1
HEADER_FORMAT = "<IHHHHI"  # magic u32, version u16, num_ch u16, spc u16, flags u16, seq u32
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)  # 16

# manual Table-5: gain_idx -> PGA voltage gain
GAIN_TABLE = {
    0: 0.1, 1: 0.2, 2: 0.4, 3: 0.7, 4: 1.0, 5: 1.36, 6: 1.70, 7: 2.55,
    8: 3.40, 9: 4.25, 10: 5.67, 11: 6.80, 12: 8.50, 13: 10.20, 14: 11.90, 15: 17.0,
}

# manual Table-4: model -> USB device id
DEVICE_IDS = {"PolyG-A": 14, "PolyG-E": 15, "PolyG-I": 16, "PolyG-U": 17}

# manual Set_ADCMaxNumChannel: model -> max analog channels
DEVICE_MAX_CHANNELS = {"PolyG-A": 32, "PolyG-I": 16, "PolyG-U": 8}

# manual Table-2: max_channels -> highest legal sample frequency (Hz)
MAX_SAMPLE_FREQ_BY_CHANNELS = {2: 4096, 4: 2048, 8: 1024, 16: 512, 32: 512}


class FrameError(Exception):
    """Raised when a received buffer is not a valid LXEM frame."""


@dataclass(frozen=True)
class FrameHeader:
    magic: int
    version: int
    num_channels: int
    samples_per_channel: int
    flags: int
    seq: int


def parse_header(buf: bytes) -> FrameHeader:
    if len(buf) < HEADER_SIZE:
        raise FrameError(f"buffer too short for header: {len(buf)} < {HEADER_SIZE}")
    magic, version, nch, spc, flags, seq = struct.unpack(HEADER_FORMAT, buf[:HEADER_SIZE])
    if magic != MAGIC:
        raise FrameError(f"bad magic: {magic:#010x} != {MAGIC:#010x}")
    if version != VERSION:
        raise FrameError(f"unsupported version: {version}")
    return FrameHeader(magic, version, nch, spc, flags, seq)


def parse_frame(buf: bytes) -> tuple[FrameHeader, np.ndarray]:
    header = parse_header(buf)
    count = header.num_channels * header.samples_per_channel
    expected = HEADER_SIZE + count * 4
    if len(buf) != expected:
        raise FrameError(f"bad length: {len(buf)} != {expected}")
    data = np.frombuffer(buf, dtype="<f4", count=count, offset=HEADER_SIZE)
    return header, data.reshape(header.num_channels, header.samples_per_channel)


def build_frame(num_channels: int, samples_per_channel: int, seq: int,
                data: np.ndarray, flags: int = 0) -> bytes:
    arr = np.ascontiguousarray(data, dtype="<f4")
    if arr.shape != (num_channels, samples_per_channel):
        raise ValueError(f"data shape {arr.shape} != {(num_channels, samples_per_channel)}")
    header = struct.pack(HEADER_FORMAT, MAGIC, VERSION,
                         num_channels, samples_per_channel, flags, seq)
    return header + arr.tobytes()
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `python -m pytest tests/test_protocol.py -v`
Expected: PASS (7 passed).

- [ ] **Step 5: Commit**

```bash
git add src/polyg_lsl/protocol.py tests/test_protocol.py
git commit -m "feat: add LXEM wire protocol parse/build"
```

---

## Task 3: µV scaling (`scaling.py`)

**Files:**
- Create: `src/polyg_lsl/scaling.py`
- Test: `tests/test_scaling.py`

- [ ] **Step 1: Write failing test**

`tests/test_scaling.py`:

```python
import numpy as np

from polyg_lsl.scaling import raw_to_microvolts


def test_known_conversion():
    # volts / (fixed_gain * pga_gain) * 1e6
    volts = np.array([1.0, 0.5], dtype=np.float64)
    out = raw_to_microvolts(volts, fixed_gain=1000.0, pga_gain=4.0)
    # 1.0 / 4000 * 1e6 = 250 ; 0.5 -> 125
    np.testing.assert_allclose(out, [250.0, 125.0])


def test_unity_gain_is_microvolts_of_volts():
    volts = np.array([1.0], dtype=np.float64)
    out = raw_to_microvolts(volts, fixed_gain=1.0, pga_gain=1.0)
    np.testing.assert_allclose(out, [1e6])
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m pytest tests/test_scaling.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'polyg_lsl.scaling'`.

- [ ] **Step 3: Implement `scaling.py`**

`src/polyg_lsl/scaling.py`:

```python
"""Raw ADC volts (-1.25..+1.25 V) to physical microvolts."""
from __future__ import annotations

import numpy as np


def raw_to_microvolts(volts: np.ndarray, fixed_gain: float, pga_gain: float) -> np.ndarray:
    """uV = volts / (fixed_gain * pga_gain) * 1e6.

    fixed_gain is the user-supplied front-end amp gain; pga_gain comes from the
    device gain_idx (manual Table-5).
    """
    total_gain = fixed_gain * pga_gain
    return np.asarray(volts, dtype=np.float64) / total_gain * 1e6
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python -m pytest tests/test_scaling.py -v`
Expected: PASS (2 passed).

- [ ] **Step 5: Commit**

```bash
git add src/polyg_lsl/scaling.py tests/test_scaling.py
git commit -m "feat: add raw-volts to microvolts scaling"
```

---

## Task 4: Config loading + validation (`config.py`)

**Files:**
- Create: `src/polyg_lsl/config.py`
- Test: `tests/test_config.py`

- [ ] **Step 1: Write failing tests**

`tests/test_config.py`:

```python
import textwrap

import pytest

from polyg_lsl.config import Config, ConfigError, load_config

VALID = textwrap.dedent("""
    [device]
    model = "PolyG-A"
    max_channels = 32
    sample_freq_idx = 9
    gain_idx = 9

    [scale]
    fixed_gain = 1000.0

    [channels]
    select = [1, 2, 3]
    labels = ["Fp1", "Fp2", "F3"]

    [transport]
    host = "127.0.0.1"
    port = 51234
""")


def _write(tmp_path, text):
    p = tmp_path / "config.toml"
    p.write_text(text)
    return p


def test_load_valid(tmp_path):
    cfg = load_config(_write(tmp_path, VALID))
    assert isinstance(cfg, Config)
    assert cfg.device_id == 14
    assert cfg.sample_freq == 512          # 2**9
    assert cfg.pga_gain == 4.25            # GAIN_TABLE[9]
    assert cfg.expected_num_channels == 33  # 32 + marking
    assert cfg.expected_samples_per_channel == 16  # 512 // 32
    assert cfg.select_zero_based == (0, 1, 2)


def test_label_select_length_mismatch(tmp_path):
    bad = VALID.replace('labels = ["Fp1", "Fp2", "F3"]', 'labels = ["Fp1", "Fp2"]')
    with pytest.raises(ConfigError, match="labels"):
        load_config(_write(tmp_path, bad))


def test_bad_model(tmp_path):
    bad = VALID.replace('model = "PolyG-A"', 'model = "PolyG-Z"')
    with pytest.raises(ConfigError, match="model"):
        load_config(_write(tmp_path, bad))


def test_gain_idx_out_of_range(tmp_path):
    bad = VALID.replace("gain_idx = 9", "gain_idx = 16")
    with pytest.raises(ConfigError, match="gain_idx"):
        load_config(_write(tmp_path, bad))


def test_sample_freq_too_high_for_channels(tmp_path):
    # 32 channels -> max 512 Hz (idx 9). idx 12 = 4096 Hz is illegal.
    bad = VALID.replace("sample_freq_idx = 9", "sample_freq_idx = 12")
    with pytest.raises(ConfigError, match="sample_freq"):
        load_config(_write(tmp_path, bad))


def test_select_index_out_of_range(tmp_path):
    bad = VALID.replace("select = [1, 2, 3]", "select = [1, 2, 99]")
    bad = bad.replace('labels = ["Fp1", "Fp2", "F3"]', 'labels = ["a", "b", "c"]')
    with pytest.raises(ConfigError, match="select"):
        load_config(_write(tmp_path, bad))
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `python -m pytest tests/test_config.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'polyg_lsl.config'`.

- [ ] **Step 3: Implement `config.py`**

`src/polyg_lsl/config.py`:

```python
"""Load and validate the shared config.toml into a Config dataclass."""
from __future__ import annotations

import os
import tomllib
from dataclasses import dataclass

from .protocol import (
    DEVICE_IDS, DEVICE_MAX_CHANNELS, GAIN_TABLE, MAX_SAMPLE_FREQ_BY_CHANNELS,
)

LEGAL_MAX_CHANNELS = (2, 4, 8, 16, 32)


class ConfigError(Exception):
    """Raised when config.toml is missing fields or has illegal values."""


@dataclass(frozen=True)
class Config:
    model: str
    device_id: int
    max_channels: int
    sample_freq_idx: int
    sample_freq: int
    gain_idx: int
    pga_gain: float
    fixed_gain: float
    select: tuple[int, ...]
    labels: tuple[str, ...]
    host: str
    port: int

    @property
    def expected_num_channels(self) -> int:
        return self.max_channels + 1  # device appends the marking channel

    @property
    def expected_samples_per_channel(self) -> int:
        return 512 // self.max_channels

    @property
    def select_zero_based(self) -> tuple[int, ...]:
        return tuple(i - 1 for i in self.select)


def load_config(path: str | os.PathLike) -> Config:
    with open(path, "rb") as f:
        raw = tomllib.load(f)

    try:
        dev = raw["device"]
        model = dev["model"]
        max_channels = dev["max_channels"]
        sample_freq_idx = dev["sample_freq_idx"]
        gain_idx = dev["gain_idx"]
        fixed_gain = raw["scale"]["fixed_gain"]
        select = list(raw["channels"]["select"])
        labels = list(raw["channels"]["labels"])
        host = raw["transport"]["host"]
        port = raw["transport"]["port"]
    except KeyError as e:
        raise ConfigError(f"missing config key: {e}") from e

    if model not in DEVICE_IDS:
        raise ConfigError(f"unknown model {model!r}; expected one of {sorted(DEVICE_IDS)}")
    if max_channels not in LEGAL_MAX_CHANNELS:
        raise ConfigError(f"max_channels must be one of {LEGAL_MAX_CHANNELS}")
    if not 0 <= sample_freq_idx <= 14:
        raise ConfigError("sample_freq_idx must be in 0..14")
    if not 0 <= gain_idx <= 15:
        raise ConfigError("gain_idx must be in 0..15")
    if len(labels) != len(select):
        raise ConfigError(f"labels ({len(labels)}) and select ({len(select)}) length mismatch")

    sample_freq = 2 ** sample_freq_idx
    max_freq = MAX_SAMPLE_FREQ_BY_CHANNELS[max_channels]
    if sample_freq > max_freq:
        raise ConfigError(
            f"sample_freq {sample_freq} Hz exceeds max {max_freq} Hz for {max_channels} channels"
        )

    # device data channels are 1..max_channels; marking channel is excluded by default
    device_cap = DEVICE_MAX_CHANNELS.get(model, max_channels)
    upper = min(max_channels, device_cap)
    for i in select:
        if not 1 <= i <= upper:
            raise ConfigError(f"select index {i} out of range 1..{upper}")

    return Config(
        model=model,
        device_id=DEVICE_IDS[model],
        max_channels=max_channels,
        sample_freq_idx=sample_freq_idx,
        sample_freq=sample_freq,
        gain_idx=gain_idx,
        pga_gain=GAIN_TABLE[gain_idx],
        fixed_gain=float(fixed_gain),
        select=tuple(select),
        labels=tuple(labels),
        host=host,
        port=port,
    )
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `python -m pytest tests/test_config.py -v`
Expected: PASS (6 passed).

- [ ] **Step 5: Commit**

```bash
git add src/polyg_lsl/config.py tests/test_config.py
git commit -m "feat: add config loading and validation"
```

---

## Task 5: Bridge pure logic (`bridge.py` part 1)

**Files:**
- Create: `src/polyg_lsl/bridge.py`
- Test: `tests/test_bridge.py`

- [ ] **Step 1: Write failing tests**

`tests/test_bridge.py`:

```python
import numpy as np

from polyg_lsl.bridge import SeqTracker, frame_to_microvolts


def test_frame_to_microvolts_selects_scales_and_transposes():
    # 3 channels x 2 samples, channel-major
    data = np.array([[1.0, 2.0],
                     [3.0, 4.0],
                     [5.0, 6.0]], dtype="<f4")
    # select device channels 1 and 3 -> zero-based (0, 2)
    out = frame_to_microvolts(data, (0, 2), fixed_gain=1.0, pga_gain=1.0)
    # shape becomes (samples, channels) = (2, 2)
    assert out.shape == (2, 2)
    assert out.dtype == np.float32
    # uV = volts / 1 * 1e6 ; rows are samples, cols are selected channels
    np.testing.assert_allclose(out, [[1e6, 5e6], [2e6, 6e6]], rtol=1e-6)


def test_seqtracker_first_and_consecutive_have_no_drops():
    t = SeqTracker()
    assert t.update(10) == 0
    assert t.update(11) == 0
    assert t.update(12) == 0


def test_seqtracker_counts_gap():
    t = SeqTracker()
    t.update(10)
    assert t.update(13) == 2  # 11, 12 missing


def test_seqtracker_handles_u32_wraparound():
    t = SeqTracker()
    t.update(0xFFFFFFFF)
    assert t.update(1) == 1  # 0 missing
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `python -m pytest tests/test_bridge.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'polyg_lsl.bridge'`.

- [ ] **Step 3: Implement the pure logic in `bridge.py`**

Create `src/polyg_lsl/bridge.py` with ONLY the pure pieces for now (LSL/socket parts added in Task 7):

```python
"""EEG bridge: pure frame transforms now; LSL/socket I/O added later."""
from __future__ import annotations

import numpy as np

from .scaling import raw_to_microvolts


def frame_to_microvolts(data: np.ndarray, select_zero_based, fixed_gain: float,
                        pga_gain: float) -> np.ndarray:
    """(num_channels, samples) channel-major volts -> (samples, n_selected) uV float32."""
    selected = np.asarray(data, dtype=np.float64)[list(select_zero_based), :]
    uv = raw_to_microvolts(selected, fixed_gain, pga_gain)
    return np.ascontiguousarray(uv.T, dtype=np.float32)


class SeqTracker:
    """Counts dropped frames from a monotonically increasing u32 seq counter."""

    def __init__(self) -> None:
        self._last: int | None = None

    def update(self, seq: int) -> int:
        if self._last is None:
            self._last = seq
            return 0
        dropped = (seq - self._last - 1) & 0xFFFFFFFF
        self._last = seq
        return dropped
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `python -m pytest tests/test_bridge.py -v`
Expected: PASS (4 passed).

- [ ] **Step 5: Commit**

```bash
git add src/polyg_lsl/bridge.py tests/test_bridge.py
git commit -m "feat: add frame_to_microvolts and SeqTracker"
```

---

## Task 6: Marker outlet (`markers.py`)

**Files:**
- Create: `src/polyg_lsl/markers.py`
- Test: `tests/test_markers.py`

- [ ] **Step 1: Write failing test**

`tests/test_markers.py`:

```python
import time

import pytest

pytest.importorskip("pylsl")
from pylsl import StreamInlet, resolve_byprop  # noqa: E402

from polyg_lsl.markers import MarkerStream  # noqa: E402


def test_marker_push_is_received():
    mk = MarkerStream(name="UnitTestMarkers", source_id="unit-test-1")
    streams = resolve_byprop("name", "UnitTestMarkers", timeout=5.0)
    assert streams, "marker stream not discoverable"
    inlet = StreamInlet(streams[0])
    time.sleep(0.2)  # let the inlet subscribe before pushing

    mk.push("scenario/choice1/onset")

    sample, ts = None, None
    deadline = time.time() + 5.0
    while time.time() < deadline:
        sample, ts = inlet.pull_sample(timeout=0.5)
        if sample:
            break
    assert sample == ["scenario/choice1/onset"]
    assert ts is not None
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m pytest tests/test_markers.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'polyg_lsl.markers'` (or skip if pylsl/liblsl unavailable — install liblsl to run it).

- [ ] **Step 3: Implement `markers.py`**

`src/polyg_lsl/markers.py`:

```python
"""Scenario-facing LSL Markers outlet."""
from __future__ import annotations

from pylsl import IRREGULAR_RATE, StreamInfo, StreamOutlet, cf_string, local_clock


class MarkerStream:
    """A single-channel string Markers outlet for stimulus-onset events."""

    def __init__(self, name: str, source_id: str, *, stream_type: str = "Markers") -> None:
        info = StreamInfo(
            name=name,
            type=stream_type,
            channel_count=1,
            nominal_srate=IRREGULAR_RATE,
            channel_format=cf_string,
            source_id=source_id,
        )
        self.outlet = StreamOutlet(info)

    def push(self, label: str, timestamp: float | None = None) -> None:
        ts = local_clock() if timestamp is None else timestamp
        self.outlet.push_sample([label], ts)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python -m pytest tests/test_markers.py -v`
Expected: PASS (1 passed) when liblsl is available; otherwise SKIPPED.

- [ ] **Step 5: Commit**

```bash
git add src/polyg_lsl/markers.py tests/test_markers.py
git commit -m "feat: add MarkerStream LSL outlet"
```

---

## Task 7: Bridge LSL outlet + datagram handling (`bridge.py` part 2)

**Files:**
- Modify: `src/polyg_lsl/bridge.py`
- Test: `tests/test_integration_lsl.py`

- [ ] **Step 1: Write failing integration test**

`tests/test_integration_lsl.py`:

```python
import time

import numpy as np
import pytest

pytest.importorskip("pylsl")
from pylsl import StreamInlet, resolve_byprop  # noqa: E402

from polyg_lsl.bridge import EEGBridge  # noqa: E402
from polyg_lsl.config import Config  # noqa: E402
from polyg_lsl.protocol import build_frame  # noqa: E402


def _cfg():
    # 2-channel device (max=2 -> 3 frame channels incl. marking, spc = 256)
    return Config(
        model="PolyG-A", device_id=14, max_channels=2, sample_freq_idx=9,
        sample_freq=512, gain_idx=4, pga_gain=1.0, fixed_gain=1.0,
        select=(1, 2), labels=("c1", "c2"), host="127.0.0.1", port=51299,
    )


def test_bridge_outlet_roundtrip_and_metadata():
    cfg = _cfg()
    bridge = EEGBridge(cfg)  # creates the outlet

    streams = resolve_byprop("type", "EEG", timeout=5.0)
    assert streams, "EEG stream not discoverable"
    inlet = StreamInlet(streams[0])
    info = inlet.info()
    assert info.channel_count() == 2
    assert info.nominal_srate() == 512
    # metadata labels
    ch = info.desc().child("channels").child("channel")
    assert ch.child_value("label") == "c1"
    assert ch.child_value("unit") == "microvolts"

    time.sleep(0.2)
    # frame: 3 channels (2 data + marking) x 256 samples, all data = 1.0 V
    nch, spc = cfg.expected_num_channels, cfg.expected_samples_per_channel
    data = np.ones((nch, spc), dtype="<f4")
    dropped = bridge.handle_datagram(build_frame(nch, spc, 0, data))
    assert dropped == 0

    chunk, _ = inlet.pull_chunk(timeout=2.0, max_samples=spc)
    arr = np.asarray(chunk)
    assert arr.shape == (spc, 2)
    # 1.0 V / (1.0 * 1.0) * 1e6 = 1e6 uV
    np.testing.assert_allclose(arr, 1e6, rtol=1e-5)


def test_handle_datagram_rejects_mismatched_frame():
    cfg = _cfg()
    bridge = EEGBridge(cfg)
    # wrong channel count (5 instead of expected 3)
    bad = build_frame(5, cfg.expected_samples_per_channel, 0,
                      np.ones((5, cfg.expected_samples_per_channel), dtype="<f4"))
    assert bridge.handle_datagram(bad) is None
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m pytest tests/test_integration_lsl.py -v`
Expected: FAIL with `ImportError: cannot import name 'EEGBridge'` (or SKIPPED without liblsl).

- [ ] **Step 3: Add LSL outlet + EEGBridge to `bridge.py`**

Append to `src/polyg_lsl/bridge.py` (keep the existing pure functions). Note: `pylsl` is imported lazily inside functions so the module — and the pure `test_bridge.py` — imports cleanly even when liblsl is not installed.

```python
import argparse
import logging
import socket

from .config import Config, load_config
from .protocol import FrameError, parse_frame

log = logging.getLogger("polyg_lsl.bridge")


def build_stream_info(cfg: Config):
    from pylsl import StreamInfo, cf_float32

    info = StreamInfo(
        name=f"PolyG_{cfg.model}",
        type="EEG",
        channel_count=len(cfg.select),
        nominal_srate=cfg.sample_freq,
        channel_format=cf_float32,
        source_id=f"polyg-{cfg.model}-{cfg.port}",
    )
    channels = info.desc().append_child("channels")
    for label in cfg.labels:
        c = channels.append_child("channel")
        c.append_child_value("label", label)
        c.append_child_value("unit", "microvolts")
        c.append_child_value("type", "EEG")
    dev = info.desc().append_child("device")
    dev.append_child_value("model", cfg.model)
    dev.append_child_value("sample_freq", str(cfg.sample_freq))
    dev.append_child_value("max_channels", str(cfg.max_channels))
    dev.append_child_value("gain_idx", str(cfg.gain_idx))
    dev.append_child_value("pga_gain", str(cfg.pga_gain))
    dev.append_child_value("fixed_gain", str(cfg.fixed_gain))
    return info


class EEGBridge:
    """Owns the LSL EEG outlet and turns received datagrams into pushed chunks."""

    def __init__(self, cfg: Config) -> None:
        from pylsl import StreamOutlet

        self.cfg = cfg
        self.outlet = StreamOutlet(build_stream_info(cfg))
        self._seq = SeqTracker()

    def handle_datagram(self, buf: bytes) -> int | None:
        """Returns dropped-frame count, or None if the frame was rejected."""
        try:
            header, data = parse_frame(buf)
        except FrameError as e:
            log.warning("bad frame: %s", e)
            return None
        if (header.num_channels != self.cfg.expected_num_channels
                or header.samples_per_channel != self.cfg.expected_samples_per_channel):
            log.error(
                "frame/config mismatch: got %dch x %d, expected %dch x %d; skipping",
                header.num_channels, header.samples_per_channel,
                self.cfg.expected_num_channels, self.cfg.expected_samples_per_channel,
            )
            return None
        dropped = self._seq.update(header.seq)
        if dropped:
            log.warning("dropped %d frame(s) before seq %d", dropped, header.seq)
        from pylsl import local_clock

        chunk = frame_to_microvolts(
            data, self.cfg.select_zero_based, self.cfg.fixed_gain, self.cfg.pga_gain
        )
        self.outlet.push_chunk(chunk.tolist(), local_clock())
        return dropped

    def run(self, *, stop=None) -> None:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((self.cfg.host, self.cfg.port))
        sock.settimeout(0.5)
        log.info("bridge listening on %s:%d", self.cfg.host, self.cfg.port)
        try:
            while stop is None or not stop():
                try:
                    buf, _ = sock.recvfrom(65535)
                except socket.timeout:
                    continue
                self.handle_datagram(buf)
        finally:
            sock.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="PolyG -> LSL EEG bridge")
    parser.add_argument("--config", default="config.toml")
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    cfg = load_config(args.config)
    bridge = EEGBridge(cfg)
    try:
        bridge.run()
    except KeyboardInterrupt:
        log.info("stopped")
```

- [ ] **Step 4: Run all tests to verify they pass**

Run: `python -m pytest tests/test_integration_lsl.py tests/test_bridge.py -v`
Expected: PASS (integration tests PASS with liblsl, else SKIPPED; pure tests PASS).

- [ ] **Step 5: Commit**

```bash
git add src/polyg_lsl/bridge.py tests/test_integration_lsl.py
git commit -m "feat: add EEG LSL outlet, datagram handling, bridge CLI"
```

---

## Task 8: Fake device generator (`fake_device.py`)

**Files:**
- Create: `src/polyg_lsl/fake_device.py`
- Test: extend `tests/test_protocol.py` with a generator round-trip

- [ ] **Step 1: Write failing test**

Append to `tests/test_protocol.py`:

```python
def test_fake_device_frame_parses_with_expected_layout():
    from polyg_lsl.fake_device import generate_frame
    from polyg_lsl.protocol import parse_frame

    buf = generate_frame(seq=5, num_channels=3, samples_per_channel=4, sample_freq=512.0)
    header, arr = parse_frame(buf)
    assert header.seq == 5
    assert arr.shape == (3, 4)
    # marking channel (last row) is held at 1.0 (switch OFF)
    np.testing.assert_array_equal(arr[-1], np.ones(4, dtype="<f4"))
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m pytest tests/test_protocol.py::test_fake_device_frame_parses_with_expected_layout -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'polyg_lsl.fake_device'`.

- [ ] **Step 3: Implement `fake_device.py`**

`src/polyg_lsl/fake_device.py`:

```python
"""Synthetic LXEM frame source for tests and offline development."""
from __future__ import annotations

import argparse
import logging
import socket
import time

import numpy as np

from .config import load_config
from .protocol import build_frame

log = logging.getLogger("polyg_lsl.fake_device")


def generate_frame(seq: int, num_channels: int, samples_per_channel: int,
                   *, sample_freq: float = 512.0, amplitude: float = 1.0) -> bytes:
    """Distinct sine per data channel; last (marking) channel held at 1.0."""
    n = samples_per_channel
    t = (seq * n + np.arange(n)) / sample_freq
    data = np.zeros((num_channels, n), dtype="<f4")
    for ch in range(num_channels - 1):
        freq = 5.0 + ch
        data[ch, :] = amplitude * np.sin(2 * np.pi * freq * t)
    data[num_channels - 1, :] = 1.0  # marking channel idle = switch OFF = 1
    return build_frame(num_channels, samples_per_channel, seq, data)


def run(cfg, *, duration: float | None = None) -> None:
    nch = cfg.expected_num_channels
    spc = cfg.expected_samples_per_channel
    interval = spc / cfg.sample_freq
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    addr = (cfg.host, cfg.port)
    log.info("fake device -> %s:%d (%d ch x %d, every %.3fs)", *addr, nch, spc, interval)
    seq = 0
    start = time.perf_counter()
    try:
        while duration is None or (time.perf_counter() - start) < duration:
            sock.sendto(generate_frame(seq, nch, spc, sample_freq=cfg.sample_freq), addr)
            seq = (seq + 1) & 0xFFFFFFFF
            time.sleep(interval)
    finally:
        sock.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Synthetic PolyG frame source")
    parser.add_argument("--config", default="config.toml")
    parser.add_argument("--duration", type=float, default=None, help="seconds; default forever")
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    cfg = load_config(args.config)
    try:
        run(cfg, duration=args.duration)
    except KeyboardInterrupt:
        log.info("stopped")
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python -m pytest tests/test_protocol.py -v`
Expected: PASS (8 passed).

- [ ] **Step 5: Commit**

```bash
git add src/polyg_lsl/fake_device.py tests/test_protocol.py
git commit -m "feat: add synthetic fake-device frame generator"
```

---

## Task 9: End-to-end socket test + example scenario

**Files:**
- Create: `tests/test_end_to_end.py`
- Create: `examples/example_scenario.py`

- [ ] **Step 1: Write failing end-to-end test**

`tests/test_end_to_end.py`:

```python
import threading
import time

import numpy as np
import pytest

pytest.importorskip("pylsl")
from pylsl import StreamInlet, resolve_byprop  # noqa: E402

from polyg_lsl.bridge import EEGBridge  # noqa: E402
from polyg_lsl.config import Config  # noqa: E402
from polyg_lsl.fake_device import run as fake_run  # noqa: E402


def _cfg():
    return Config(
        model="PolyG-A", device_id=14, max_channels=2, sample_freq_idx=9,
        sample_freq=512, gain_idx=4, pga_gain=1.0, fixed_gain=1.0,
        select=(1, 2), labels=("c1", "c2"), host="127.0.0.1", port=51298,
    )


def test_fake_device_to_bridge_to_inlet():
    cfg = _cfg()
    bridge = EEGBridge(cfg)

    stop = threading.Event()
    bridge_thread = threading.Thread(target=bridge.run, kwargs={"stop": stop.is_set}, daemon=True)
    bridge_thread.start()

    dev_thread = threading.Thread(target=fake_run, args=(cfg,), kwargs={"duration": 2.0}, daemon=True)
    dev_thread.start()

    streams = resolve_byprop("type", "EEG", timeout=5.0)
    assert streams
    inlet = StreamInlet(streams[0])

    received = []
    deadline = time.time() + 4.0
    while time.time() < deadline and len(received) < 256:
        chunk, _ = inlet.pull_chunk(timeout=0.5, max_samples=512)
        received.extend(chunk)

    stop.set()
    assert len(received) > 0
    arr = np.asarray(received)
    assert arr.shape[1] == 2  # two selected channels
```

- [ ] **Step 2: Run test to verify it fails or skips**

Run: `python -m pytest tests/test_end_to_end.py -v`
Expected: FAIL (assertion) initially is not expected — this should PASS once Tasks 7 & 8 exist. If liblsl is missing it SKIPS. Run it to confirm PASS or SKIP.

- [ ] **Step 3: Create the example scenario**

`examples/example_scenario.py`:

```python
"""Minimal scenario that emits LSL markers at stimulus boundaries.

Run the bridge and (optionally) the fake device in other terminals, start
LabRecorder, then run this to see markers land alongside the EEG stream.
"""
import time

from polyg_lsl.markers import MarkerStream

STEPS = [
    ("scenario/onset", 1.0),
    ("choice1/onset", 1.5),
    ("choice2/onset", 1.5),
    ("iti/onset", 1.0),
]


def main() -> None:
    mk = MarkerStream(name="EEG_Scenario_Markers", source_id="example-scenario-1")
    print("pushing markers; start your recorder now...")
    time.sleep(2.0)
    for label, dur in STEPS:
        mk.push(label)
        print(f"marker: {label}")
        time.sleep(dur)
    print("done")


if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Run the full test suite**

Run: `python -m pytest -v`
Expected: all PASS (LSL tests SKIP only if liblsl unavailable).

- [ ] **Step 5: Commit**

```bash
git add tests/test_end_to_end.py examples/example_scenario.py
git commit -m "test: add end-to-end pipeline test and example scenario"
```

---

## Task 10: Example config + Python README

**Files:**
- Create: `config.toml`
- Create: `README.md`

- [ ] **Step 1: Create `config.toml`**

```toml
# Shared config. The Python bridge reads this file directly.
# The C++ acquisition app mirrors the [device]/[transport] values in cpp/BridgeConfig.h.

[device]
model           = "PolyG-A"   # PolyG-A (id 14) / PolyG-E (15) / PolyG-I (16) / PolyG-U (17)
max_channels    = 32          # 2 / 4 / 8 / 16 / 32
sample_freq_idx = 9           # 2^idx Hz -> 512 Hz (must be legal for max_channels)
gain_idx        = 9           # manual Table-5 -> pga_gain 4.25

[scale]
fixed_gain = 1.0              # TODO: set to the front-end amp gain from datasheet/calibration

[channels]
# device channel indices (1-based, EEG block) and matching labels (same length)
select = [1, 2, 3, 4, 5, 6, 7, 8]
labels = ["Fp1", "Fp2", "F3", "F4", "C3", "C4", "P3", "P4"]

[transport]
host = "127.0.0.1"
port = 51234
```

- [ ] **Step 2: Create `README.md`**

```markdown
# polyg-lsl-bridge

Acquire PolyG EEG and publish it as an LSL outlet in microvolts, plus a software
LSL marker stream for experiment scenarios.

## Install
```bash
python3.11 -m venv .venv && . .venv/bin/activate
pip install -e ".[dev]"
```
If `pylsl` cannot load liblsl: macOS `brew install labstreaminglayer/tap/lsl`.

## Run (offline, no device)
```bash
# terminal 1: the bridge (LSL EEG outlet)
polyg-bridge --config config.toml
# terminal 2: synthetic frames into the bridge
polyg-fake-device --config config.toml
# terminal 3: markers
python examples/example_scenario.py
```
Record both `PolyG_*` (EEG) and `EEG_Scenario_Markers` with LabRecorder; LSL
time-aligns them.

## Real device
Run the C++ acquisition app (see `cpp/README.md`) on the EEG PC instead of
`polyg-fake-device`. Keep `cpp/BridgeConfig.h` in sync with `config.toml`.

## Test
```bash
python -m pytest -v
```
```

- [ ] **Step 3: Verify the offline pipeline by hand (smoke)**

Run (three terminals, venv active):
```bash
polyg-bridge --config config.toml
polyg-fake-device --config config.toml --duration 5
python examples/example_scenario.py
```
Expected: bridge logs "listening", no "mismatch"/"dropped" floods; scenario prints markers. (Requires `labels`/`select` lengths to match — the shipped config uses 8.)

- [ ] **Step 4: Commit**

```bash
git add config.toml README.md
git commit -m "docs: add example config and Python README"
```

---

## Task 11: C++ UDP forwarder (`cpp/Forwarder.{h,cpp}`)

**Files:**
- Create: `cpp/Forwarder.h`
- Create: `cpp/Forwarder.cpp`

> C++ is built/tested on Windows in Visual Studio (Task 12). These tasks deliver exact source; there is no off-device unit test.

- [ ] **Step 1: Create `cpp/Forwarder.h`**

```cpp
#pragma once
#include <winsock2.h>
#include <cstdint>

// Sends LXEM frames (16-byte header + channel-major float payload) over UDP.
// One socket is opened in Init() and reused for every Send() (no per-frame alloc
// of the socket). Header layout matches src/polyg_lsl/protocol.py exactly.
class Forwarder {
public:
    Forwarder();
    ~Forwarder();
    bool Init(const char* host, unsigned short port);
    bool Send(uint32_t seq, const float* data,
              uint16_t num_channels, uint16_t samples_per_channel);
    void Close();
private:
    SOCKET m_sock;
    sockaddr_in m_dest;
    bool m_wsa;
};
```

- [ ] **Step 2: Create `cpp/Forwarder.cpp`**

```cpp
#include "Forwarder.h"
#include <ws2tcpip.h>
#include <cstring>
#include <vector>
#pragma comment(lib, "Ws2_32.lib")

static const uint32_t LXEM_MAGIC = 0x4C58454D; // 'LXEM'
static const uint16_t LXEM_VERSION = 1;

Forwarder::Forwarder() : m_sock(INVALID_SOCKET), m_wsa(false) {
    std::memset(&m_dest, 0, sizeof(m_dest));
}

Forwarder::~Forwarder() { Close(); }

bool Forwarder::Init(const char* host, unsigned short port) {
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return false;
    m_wsa = true;
    m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_sock == INVALID_SOCKET) return false;
    m_dest.sin_family = AF_INET;
    m_dest.sin_port = htons(port);
    inet_pton(AF_INET, host, &m_dest.sin_addr);
    return true;
}

bool Forwarder::Send(uint32_t seq, const float* data,
                     uint16_t nch, uint16_t spc) {
    if (m_sock == INVALID_SOCKET) return false;
    const size_t payload = static_cast<size_t>(nch) * spc * sizeof(float);
    std::vector<char> buf(16 + payload);
    char* p = buf.data();
    std::memcpy(p + 0, &LXEM_MAGIC, 4);
    std::memcpy(p + 4, &LXEM_VERSION, 2);
    std::memcpy(p + 6, &nch, 2);
    std::memcpy(p + 8, &spc, 2);
    uint16_t flags = 0;
    std::memcpy(p + 10, &flags, 2);
    std::memcpy(p + 12, &seq, 4);
    std::memcpy(p + 16, data, payload);
    int sent = sendto(m_sock, buf.data(), static_cast<int>(buf.size()), 0,
                      reinterpret_cast<sockaddr*>(&m_dest), sizeof(m_dest));
    return sent == static_cast<int>(buf.size());
}

void Forwarder::Close() {
    if (m_sock != INVALID_SOCKET) { closesocket(m_sock); m_sock = INVALID_SOCKET; }
    if (m_wsa) { WSACleanup(); m_wsa = false; }
}
```

- [ ] **Step 3: Create `cpp/BridgeConfig.h`**

```cpp
#pragma once
// Mirror these values with config.toml. v1 does not parse TOML in C++.
#define BRIDGE_HOST "127.0.0.1"
#define BRIDGE_PORT 51234
```

- [ ] **Step 4: Commit**

```bash
git add cpp/Forwarder.h cpp/Forwarder.cpp cpp/BridgeConfig.h
git commit -m "feat: add C++ UDP framed forwarder"
```

---

## Task 12: Wire forwarder into a copy of the Test app + smoke test

**Files:**
- Create: `cpp/README.md`
- (On Windows) Modify a **copy** of `Test_LXSM_D1WD10_VC2017` — do not edit the original.

- [ ] **Step 1: Create `cpp/README.md` with the exact edit recipe**

````markdown
# C++ acquisition app changes

Work on a COPY of `Test_LXSM_D1WD10_VC2017`. The original is untouched.

## 1. Add files to the project
Add `Forwarder.h`, `Forwarder.cpp`, `BridgeConfig.h` to the VS project.

## 2. Fix the 64-bit lib pragma block in `Test_LXSM_D1WD10View.cpp`
Replace `#if(x64)` with `#ifdef _WIN64`:

```cpp
#ifdef _WIN64
#pragma comment(lib,"LIB_64bit\\LXSM-D1WD10.lib")
#pragma comment(lib,"LIB_64bit\\ACQPLOT.lib")
#else
#pragma comment(lib,"LIB_32bit\\LXSM-D1WD10.lib")
#pragma comment(lib,"LIB_32bit\\ACQPLOT.lib")
#endif
```

## 3. Add members in `Test_LXSM_D1WD10View.h`
```cpp
#include "Forwarder.h"
#include <cstdint>
// ...inside class CTest_LXSM_D1WD10View, public section:
    Forwarder m_forwarder;
    uint32_t  m_seq;
```
Initialise `m_seq = 0;` in the constructor.

## 4. Replace the body of `OnStreamData` in `Test_LXSM_D1WD10View.cpp`
Delete the `printf` loop and the old `SendDataOverUDP(...)` call. New body:
```cpp
afx_msg LRESULT CTest_LXSM_D1WD10View::OnStreamData(WPARAM wParam, LPARAM lParam)
{
    ACQPLOT_DLL_Array_Datain_Strip((float*)(lParam), DISPMAXCH, DISPDATANUM);
    m_forwarder.Send(m_seq++, (const float*)lParam,
                     (uint16_t)DISPMAXCH, (uint16_t)DISPDATANUM);
    return 0;
}
```
(`DISPMAXCH` already includes the marking channel; `DISPDATANUM` is samples/channel.)

## 5. Open the socket + fix init timing in `OnMENUInitDevicePolyGA` (and `OnMenuInitdevicePolygI`)
After a successful `Set_ADCMaxNumChannel`, add the manual-required wait, then
init the forwarder once:
```cpp
retv = Set_ADCMaxNumChannel(DISPMAXCH-1);
Sleep(100);                                  // manual: 0.1s after Set_ADCMaxNumChannel
if (retv > 1) {
    DISPDATANUM = retv;
    OnUpdate(NULL, 0, 0);
    Set_PGA(4);
    Sleep(100);                              // manual: 0.1s after Set_PGA
    m_forwarder.Init(BRIDGE_HOST, BRIDGE_PORT);
    m_seq = 0;
}
```
Remember to `#include "BridgeConfig.h"` at the top of the .cpp.
Set the sampling frequency (a `SetSample_*` menu) after init and BEFORE
`Start_Stream` — the manual requires re-setting sample freq after channel changes.

## 6. Remove the now-unused `SendDataOverUDP` helper (optional cleanup).
````

- [ ] **Step 2: Build on Windows**

On the EEG PC in Visual Studio (x64 config): add the three files, apply edits, Build.
Expected: compiles and links against `LIB_64bit`.

- [ ] **Step 3: Smoke test against the device**

Terminal (Python venv): `polyg-bridge --config config.toml`
Then run the C++ app, Init device (PolyG-A/I), set a sample freq, `Start_Stream`.
Expected: bridge logs an EEG outlet and steady frames with NO "mismatch" and NO
"dropped" floods. Open LabRecorder and confirm a `PolyG_*` EEG stream at the
configured rate and channel count.

- [ ] **Step 4: Commit**

```bash
git add cpp/README.md
git commit -m "docs: C++ acquisition app edit recipe and smoke test"
```

---

## Self-Review notes (resolved)

- **Spec §5 wire protocol** → Tasks 2, 11 (Python + C++ build identical headers; `HEADER_FORMAT "<IHHHHI"` ⇔ C++ memcpy offsets 0/4/6/8/10/12, payload at 16).
- **Spec §6 C++ delta** → Task 12 (printf removed, socket reused via `Forwarder`, `Sleep(100)`, `#ifdef _WIN64`).
- **Spec §7 bridge** → Tasks 5, 7 (validate, de-interleave, µV, push_chunk + metadata).
- **Spec §8 markers** → Task 6 + Task 9 example.
- **Spec §9 config** → Tasks 4, 10 (validation rules all covered: model, max_channels, gain_idx, label/select length, sample_freq legality, select range).
- **Spec §10 error handling** → Task 7 (`handle_datagram` returns None on bad/mismatched frames and logs; SeqTracker drop logging; bridge `run` never dies on a bad datagram).
- **Spec §12 testing** → fake device (Task 8), unit tests (2–5), integration (7), end-to-end (9).
- **Type consistency check:** `Config.expected_num_channels/expected_samples_per_channel/select_zero_based`, `frame_to_microvolts(data, select_zero_based, fixed_gain, pga_gain)`, `EEGBridge.handle_datagram`, `SeqTracker.update`, `build_frame(num_channels, samples_per_channel, seq, data, flags)`, `generate_frame(seq, num_channels, samples_per_channel, *, sample_freq, amplitude)` — names/signatures consistent across Tasks 2/4/5/7/8.
- **Open items (Spec §13)** surfaced as config TODO (`fixed_gain`), label montage in `config.toml`, marking channel dropped by default (validation restricts `select` to 1..max_channels).
```
