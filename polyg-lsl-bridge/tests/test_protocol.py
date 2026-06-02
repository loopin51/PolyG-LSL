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


def test_fake_device_frame_parses_with_expected_layout():
    from polyg_lsl.fake_device import generate_frame
    from polyg_lsl.protocol import parse_frame

    buf = generate_frame(seq=5, num_channels=3, samples_per_channel=4, sample_freq=512.0)
    header, arr = parse_frame(buf)
    assert header.seq == 5
    assert arr.shape == (3, 4)
    # marking channel (last row) is held at 1.0 (switch OFF)
    np.testing.assert_array_equal(arr[-1], np.ones(4, dtype="<f4"))
