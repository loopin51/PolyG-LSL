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
