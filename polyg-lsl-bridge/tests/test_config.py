import textwrap

import pytest

from polyg_lsl.config import Config, ConfigError, load_config

VALID = textwrap.dedent("""
    [device]
    model = "PolyG-A"
    max_channels = 32
    sample_freq_idx = 9
    gain_idx = 9

    [scale]
    fixed_gain = 1000.0

    [channels]
    select = [1, 2, 3]
    labels = ["Fp1", "Fp2", "F3"]

    [transport]
    host = "127.0.0.1"
    port = 51234
""")


def _write(tmp_path, text):
    p = tmp_path / "config.toml"
    p.write_text(text)
    return p


def test_load_valid(tmp_path):
    cfg = load_config(_write(tmp_path, VALID))
    assert isinstance(cfg, Config)
    assert cfg.device_id == 14
    assert cfg.sample_freq == 512          # 2**9
    assert cfg.pga_gain == 4.25            # GAIN_TABLE[9]
    assert cfg.expected_num_channels == 33  # 32 + marking
    assert cfg.expected_samples_per_channel == 16  # 512 // 32
    assert cfg.select_zero_based == (0, 1, 2)


def test_label_select_length_mismatch(tmp_path):
    bad = VALID.replace('labels = ["Fp1", "Fp2", "F3"]', 'labels = ["Fp1", "Fp2"]')
    with pytest.raises(ConfigError, match="labels"):
        load_config(_write(tmp_path, bad))


def test_bad_model(tmp_path):
    bad = VALID.replace('model = "PolyG-A"', 'model = "PolyG-Z"')
    with pytest.raises(ConfigError, match="model"):
        load_config(_write(tmp_path, bad))


def test_gain_idx_out_of_range(tmp_path):
    bad = VALID.replace("gain_idx = 9", "gain_idx = 16")
    with pytest.raises(ConfigError, match="gain_idx"):
        load_config(_write(tmp_path, bad))


def test_sample_freq_too_high_for_channels(tmp_path):
    # 32 channels -> max 512 Hz (idx 9). idx 12 = 4096 Hz is illegal.
    bad = VALID.replace("sample_freq_idx = 9", "sample_freq_idx = 12")
    with pytest.raises(ConfigError, match="sample_freq"):
        load_config(_write(tmp_path, bad))


def test_select_index_out_of_range(tmp_path):
    bad = VALID.replace("select = [1, 2, 3]", "select = [1, 2, 99]")
    bad = bad.replace('labels = ["Fp1", "Fp2", "F3"]', 'labels = ["a", "b", "c"]')
    with pytest.raises(ConfigError, match="select"):
        load_config(_write(tmp_path, bad))
