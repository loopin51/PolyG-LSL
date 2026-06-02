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

    inlet.open_stream(timeout=5.0)
    deadline = time.time() + 5.0
    while time.time() < deadline and not bridge.outlet.have_consumers():
        time.sleep(0.05)

    # frame: 3 channels (2 data + marking) x 256 samples, all data = 1.0 V
    nch, spc = cfg.expected_num_channels, cfg.expected_samples_per_channel
    data = np.ones((nch, spc), dtype="<f4")
    dropped = bridge.handle_datagram(build_frame(nch, spc, 0, data))
    assert dropped == 0

    chunk = []
    deadline = time.time() + 5.0
    while time.time() < deadline and len(chunk) < spc:
        c, _ = inlet.pull_chunk(timeout=0.5, max_samples=spc)
        chunk.extend(c)
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
