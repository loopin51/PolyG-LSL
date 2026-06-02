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
    inlet.open_stream(timeout=5.0)  # force the inlet transport to connect before we push

    # wait until the outlet actually sees the consumer, so the marker isn't dropped
    deadline = time.time() + 5.0
    while time.time() < deadline and not mk.outlet.have_consumers():
        time.sleep(0.05)

    mk.push("scenario/choice1/onset")

    sample, ts = None, None
    deadline = time.time() + 5.0
    while time.time() < deadline:
        sample, ts = inlet.pull_sample(timeout=0.5)
        if sample:
            break
    assert sample == ["scenario/choice1/onset"]
    assert ts is not None
