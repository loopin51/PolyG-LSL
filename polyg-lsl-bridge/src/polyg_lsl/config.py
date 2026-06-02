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
