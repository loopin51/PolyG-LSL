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

    def __init__(self, cfg: Config, *, log_values: bool = False,
                 log_raw: bool = False, log_interval: float = 0.0) -> None:
        from pylsl import StreamOutlet

        self.cfg = cfg
        self.outlet = StreamOutlet(build_stream_info(cfg))
        self._seq = SeqTracker()
        self._log_values = log_values
        self._log_raw = log_raw
        self._log_interval = log_interval
        self._last_log_t: float | None = None
        self._last_raw_log_t: float | None = None

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
        ts = local_clock()
        self.outlet.push_chunk(chunk.tolist(), ts)
        if self._log_raw:
            self._log_raw_volts(data, ts)
        if self._log_values:
            self._log_channel_values(chunk, ts)
        return dropped

    # Assumed ADC full scale of the device, in volts (+-ADC_FULLSCALE_V). Values
    # pinned near this rail mean the input is saturated, not a real biopotential.
    ADC_FULLSCALE_V = 1.25

    def _log_raw_volts(self, data: np.ndarray, ts: float) -> None:
        """Log per-channel raw input voltage range (pre-scaling) to diagnose scaling.

        For each selected channel, logs the min..max raw volts over the frame and
        marks ``*`` when it is railed near +-ADC_FULLSCALE_V (e.g. a floating
        electrode), which inflates the uV output. ``data`` is (num_channels,
        samples) channel-major volts. Throttled by ``log_interval``.
        """
        if (self._last_raw_log_t is not None
                and (ts - self._last_raw_log_t) < self._log_interval):
            return
        self._last_raw_log_t = ts
        sel = np.asarray(data, dtype=np.float64)[list(self.cfg.select_zero_based), :]
        vmin = sel.min(axis=1)
        vmax = sel.max(axis=1)
        rail = 0.99 * self.ADC_FULLSCALE_V
        parts = []
        for label, lo, hi in zip(self.cfg.labels, vmin, vmax):
            mark = "*" if max(abs(lo), abs(hi)) >= rail else ""
            parts.append(f"{label}=[{lo:+.3f}..{hi:+.3f}]{mark}")
        log.info("raw V (*=railed near +-%.2f V) %s", self.ADC_FULLSCALE_V, " ".join(parts))

    def _log_channel_values(self, chunk: np.ndarray, ts: float) -> None:
        """Log the most recent per-channel uV value just pushed to the outlet.

        Throttled to at most one line per ``log_interval`` seconds (0 = every chunk).
        ``chunk`` is (samples, n_selected); the last row is the newest sample.
        """
        if (self._last_log_t is not None
                and (ts - self._last_log_t) < self._log_interval):
            return
        self._last_log_t = ts
        latest = chunk[-1]
        pairs = " ".join(
            f"{label}={value:+.2f}"
            for label, value in zip(self.cfg.labels, latest)
        )
        log.info("uV %s", pairs)

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
    parser.add_argument(
        "--log-values", action="store_true",
        help="log the per-channel uV value pushed to the LSL outlet in real time",
    )
    parser.add_argument(
        "--log-raw", action="store_true",
        help="log per-channel raw input voltage range (pre-scaling) to diagnose "
             "scaling/saturation; '*' marks a channel railed near +-1.25 V",
    )
    parser.add_argument(
        "--log-interval", type=float, default=0.0, metavar="SECONDS",
        help="minimum seconds between value/raw log lines (0 = every frame; "
             "raise it to throttle at high sample rates)",
    )
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    cfg = load_config(args.config)
    bridge = EEGBridge(
        cfg, log_values=args.log_values, log_raw=args.log_raw,
        log_interval=args.log_interval,
    )
    try:
        bridge.run()
    except KeyboardInterrupt:
        log.info("stopped")
