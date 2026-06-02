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
