# 테스트 가이드 (Testing)

`polyg-lsl` 패키지는 7개 파일에 걸쳐 총 **24개의 테스트**를 갖추고 있습니다. 핵심 설계 속성은 **Python 측 전체가 실제 장비/C++ 포워더/Windows 없이 검증 가능**하다는 점입니다. 프레임 합성기인 `fake_device`가 PolyG 장비를 대신해 실제 와이어 포맷의 UDP 프레임을 생성하므로, `fake_device → bridge → pylsl inlet` 경로 전체를 오프라인에서 통합 테스트할 수 있습니다. 또한 순수 로직 테스트 20개는 `liblsl`이 없는 환경(예: macOS)에서도 그대로 실행됩니다.

## 테스트 레이아웃

| 파일 | 테스트 수 | 대상 모듈 | pylsl 필요? |
| --- | --- | --- | --- |
| `tests/test_protocol.py` | 8 | `polyg_lsl.protocol` (+ `fake_device.generate_frame`) | 아니오 |
| `tests/test_scaling.py` | 2 | `polyg_lsl.scaling` | 아니오 |
| `tests/test_config.py` | 6 | `polyg_lsl.config` | 아니오 |
| `tests/test_bridge.py` | 4 | `polyg_lsl.bridge` (순수 로직) | 아니오 |
| `tests/test_markers.py` | 1 | `polyg_lsl.markers` | 예 |
| `tests/test_integration_lsl.py` | 2 | `polyg_lsl.bridge` (LSL outlet) | 예 |
| `tests/test_end_to_end.py` | 1 | `fake_device` + `bridge` 통합 | 예 |

`pylsl 필요?` = `pytest.importorskip("pylsl")` 가드가 모듈 상단에 걸려 있는지 여부. `pylsl`을 설치하면(`liblsl` 동봉) 4개 LSL 테스트도 모두 실행되어 **24개 전부 통과, 0개 skip**입니다.

### 파일별 테스트 함수

**`test_protocol.py`** (와이어 포맷: 헤더/페이로드 파싱, 상수, 손상 프레임 처리)
- `test_header_size_is_16`
- `test_gain_table_known_values`
- `test_device_ids`
- `test_parse_header_roundtrip`
- `test_parse_frame_shape_and_values`
- `test_bad_magic_raises`
- `test_truncated_payload_raises`
- `test_fake_device_frame_parses_with_expected_layout`

**`test_scaling.py`** (raw 볼트 → 마이크로볼트 환산)
- `test_known_conversion`
- `test_unity_gain_is_microvolts_of_volts`

**`test_config.py`** (TOML 설정 로딩 및 검증 — `tmp_path` 사용)
- `test_load_valid`
- `test_label_select_length_mismatch`
- `test_bad_model`
- `test_gain_idx_out_of_range`
- `test_sample_freq_too_high_for_channels`
- `test_select_index_out_of_range`

**`test_bridge.py`** (브리지 순수 로직: 채널 선택/스케일/전치, 시퀀스 추적)
- `test_frame_to_microvolts_selects_scales_and_transposes`
- `test_seqtracker_first_and_consecutive_have_no_drops`
- `test_seqtracker_counts_gap`
- `test_seqtracker_handles_u32_wraparound`

**`test_markers.py`** (마커 스트림 push/수신 — LSL 필요)
- `test_marker_push_is_received`

**`test_integration_lsl.py`** (브리지 LSL outlet 라운드트립 — LSL 필요)
- `test_bridge_outlet_roundtrip_and_metadata`
- `test_handle_datagram_rejects_mismatched_frame`

**`test_end_to_end.py`** (가짜 장비 → 브리지 → inlet 전 구간 — LSL 필요)
- `test_fake_device_to_bridge_to_inlet`

## 실행 방법

### 설치

테스트 의존성(`pytest`)은 `dev` extra로 정의되어 있습니다(`pyproject.toml`의 `[project.optional-dependencies]`).

```bash
# 런타임 의존성만 (numpy, pylsl)
pip install -e .

# 테스트 포함 — pytest까지 설치
pip install -e .[dev]
```

`pylsl>=1.16`이 런타임 의존성이며 `liblsl`을 동봉(pylsl 1.18.2)하므로, 일반 설치 환경에서는 LSL 테스트도 그대로 실행됩니다.

### 실행

```bash
pytest                 # 전체 24개 실행 (pyproject의 testpaths = ["tests"])
pytest -v              # 테스트 함수별 결과를 자세히 출력

# 단일 파일
pytest tests/test_protocol.py

# 단일 테스트 함수
pytest tests/test_config.py::test_load_valid
```

### 순수 테스트만 vs 전체

`liblsl`을 사용할 수 없거나 LSL 네트워크 동작을 건너뛰고 싶을 때, 순수 로직 20개만 골라 실행할 수 있습니다.

```bash
# 순수 테스트만 (LSL 불필요)
pytest tests/test_protocol.py tests/test_scaling.py tests/test_config.py tests/test_bridge.py
```

### 기대 결과

- **`pylsl` 설치됨(기본):** `24 passed, 0 skipped`
- **`pylsl`/`liblsl` 없음:** 순수 20개 통과, LSL 4개는 `pytest.importorskip`에 의해 skip (`20 passed, 4 skipped`)

## 패턴 & 설계

### 1. `pytest.importorskip("pylsl")` 가드

`test_markers.py`, `test_integration_lsl.py`, `test_end_to_end.py` 세 파일은 모듈 최상단에서 다음을 호출합니다.

```python
import pytest
pytest.importorskip("pylsl")
from pylsl import StreamInlet, resolve_byprop  # noqa: E402
```

`pylsl`(및 동봉 `liblsl`)을 임포트할 수 없으면 **해당 파일의 테스트만** skip 처리되고 수집/실행이 중단되지 않습니다. 덕분에 순수 로직 테스트 20개는 `liblsl`이 없는 환경(예: liblsl 미설치 macOS)에서도 항상 실행됩니다.

### 2. 합성 프레임 소스로서의 `fake_device`

`polyg_lsl.fake_device`는 실제 PolyG 장비와 동일한 와이어 포맷의 프레임을 생성합니다.

- `generate_frame(...)`는 단일 프레임 바이트열을 만들며, `test_protocol.py`에서 직접 파싱 검증에 쓰입니다(마킹 채널인 마지막 행이 스위치 OFF 상태인 1.0으로 유지되는지 확인).
- `run(cfg, duration=...)`는 설정에 맞춰 UDP로 프레임을 송신하는 가짜 장비 루프이며, `test_end_to_end.py`에서 별도 스레드로 구동됩니다.

이 합성 소스 덕분에 **실제 장비, C++ 포워더, Windows 없이도** 전체 통합 경로를 오프라인에서 테스트할 수 있습니다.

### 3. `tmp_path`를 이용한 임시 TOML

`test_config.py`는 pytest 내장 `tmp_path` 픽스처로 임시 TOML 파일을 만들고 `load_config()`에 넘깁니다.

```python
def _write(tmp_path, text):
    p = tmp_path / "config.toml"
    p.write_text(text)
    return p
```

기준이 되는 `VALID` 설정 문자열을 `str.replace(...)`로 한 값만 바꿔 실패 케이스를 만들고, `pytest.raises(ConfigError, match=...)`로 검증 메시지를 확인합니다(예: 잘못된 모델, 범위 밖 `gain_idx`, 채널 수 대비 과도한 샘플링 주파수, `labels`/`select` 길이 불일치, 범위 밖 채널 선택).

### 4. 통합 테스트가 검증하는 것

**`test_integration_lsl.py`**
- `test_bridge_outlet_roundtrip_and_metadata`: `EEGBridge(cfg)`가 LSL outlet을 생성하고, inlet 쪽에서 스트림 메타데이터를 확인합니다 — 채널 수(2), `nominal_srate`(512), 채널 라벨(`c1`), 단위(`microvolts`). 이어서 `handle_datagram`으로 1.0 V 프레임을 주입하면 inlet에서 `1e6 µV`로 환산된 샘플이 `(samples, channels)` 형태로 수신되는지(라운드트립) 검증합니다.
- `test_handle_datagram_rejects_mismatched_frame`: 설정에서 기대하는 채널 수(3)와 다른 프레임(5채널)을 주면 `handle_datagram`이 `None`을 반환하며 거부하는지 확인합니다.

**`test_end_to_end.py`**
- `test_fake_device_to_bridge_to_inlet`: `EEGBridge`를 스레드로 실행하고 `fake_device.run`을 또 다른 스레드로 구동한 뒤, `pylsl` inlet에서 샘플을 끌어옵니다. 수신 샘플이 0보다 많고, 채널 축이 선택된 2채널(`arr.shape[1] == 2`)인지 검증하여 `fake_device → bridge → inlet` 전 구간을 확인합니다.

**`test_markers.py`**
- `test_marker_push_is_received`: `MarkerStream`을 만들고 inlet으로 발견·연결한 뒤(소비자 연결 대기 후) 마커를 push하면, inlet에서 동일 문자열(`"scenario/choice1/onset"`)과 유효한 타임스탬프를 수신하는지 검증합니다.

## 관련 문서

- [api-reference.md](./api-reference.md) — 모듈/함수 API 레퍼런스
- [../README.md](../README.md) — 설치·실행·전체 개요
- 설계 명세: `docs/superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md`
