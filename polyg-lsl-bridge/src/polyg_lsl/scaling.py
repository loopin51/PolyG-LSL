"""Raw ADC volts (-1.25..+1.25 V) to physical microvolts."""
from __future__ import annotations

import numpy as np


def raw_to_microvolts(volts: np.ndarray, fixed_gain: float, pga_gain: float) -> np.ndarray:
    """uV = volts / (fixed_gain * pga_gain) * 1e6.

    fixed_gain is the user-supplied front-end amp gain; pga_gain comes from the
    device gain_idx (manual Table-5).
    """
    total_gain = fixed_gain * pga_gain
    return np.asarray(volts, dtype=np.float64) / total_gain * 1e6
