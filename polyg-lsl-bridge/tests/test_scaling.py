import numpy as np

from polyg_lsl.scaling import raw_to_microvolts


def test_known_conversion():
    # volts / (fixed_gain * pga_gain) * 1e6
    volts = np.array([1.0, 0.5], dtype=np.float64)
    out = raw_to_microvolts(volts, fixed_gain=1000.0, pga_gain=4.0)
    # 1.0 / 4000 * 1e6 = 250 ; 0.5 -> 125
    np.testing.assert_allclose(out, [250.0, 125.0])


def test_unity_gain_is_microvolts_of_volts():
    volts = np.array([1.0], dtype=np.float64)
    out = raw_to_microvolts(volts, fixed_gain=1.0, pga_gain=1.0)
    np.testing.assert_allclose(out, [1e6])
