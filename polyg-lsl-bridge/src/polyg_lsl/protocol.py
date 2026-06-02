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
