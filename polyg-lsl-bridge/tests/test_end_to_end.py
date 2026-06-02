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
