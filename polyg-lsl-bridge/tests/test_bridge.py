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
