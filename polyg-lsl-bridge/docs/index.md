# polyg_lsl 브리지 문서 허브

이 페이지는 `polyg_lsl` 브리지 패키지(version 0.1.0)의 문서 navigation 허브이자
architecture 개요입니다. 브리지는 PolyG EEG 장비의 신호를 localhost UDP feed(별도의 C++
acquisition 앱이 보내는 LXEM frame)로 수신해 파싱·검증·de-interleave·µV 변환한 뒤, 채널들을
하나의 LSL EEG StreamOutlet으로 재발행(republish)하고, 자극 onset 이벤트는 별도의 LSL
Markers outlet으로 내보냅니다. LabRecorder 같은 recorder가 두 stream을 동시에 구독하면 LSL이
clock-align해 함께 기록합니다. 이 브리지는 더 큰 EEG 자동화 repo의 한 subsystem이며, repo
전체 관점은 [repo 루트 문서 허브](../../docs/index.md)를 참고하세요.

## 문서 navigation

| 문서 | 내용 | 링크 |
| --- | --- | --- |
| README | 설치 · 실행 (전체 setup) | [../README.md](../README.md) |
| API 레퍼런스 | 모듈 · 함수 · 클래스 reference | [./api-reference.md](./api-reference.md) |
| 테스트 | pure 테스트 + offline 통합 테스트 실행 | [./testing.md](./testing.md) |
| 설계 spec | EEG–LSL 브리지 design spec | [./superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md](./superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md) |
| 구현 plan | 단계별 구현 plan | [./superpowers/plans/2026-06-02-eeg-lsl-bridge.md](./superpowers/plans/2026-06-02-eeg-lsl-bridge.md) |

## 패키지 구조 & 데이터 흐름

```
                                              ┌──────────────────────────┐
 PolyG 장비 ── analog EEG ──▶ C++ acquisition │ EEGBridge (bridge.py)    │
                              앱 (별도 프로세스)│  parse_frame ─ 검증      │
                                   │           │  de-interleave ─ µV 변환 │──▶ LSL EEG
                                   │ localhost │  push_chunk              │    StreamOutlet
                                   └── UDP ───▶│                          │        │
                                   (LXEM frame)└──────────────────────────┘        │
                                                                                   ▼
   실험 시나리오 ─ stimulus onset ──▶ MarkerStream (markers.py) ──▶ LSL Markers ──▶ recorder
                                                                  StreamOutlet   (LabRecorder)
                                                                                  ↑ clock-align
```

| 모듈 | 책임 | pure / LSL-IO |
| --- | --- | --- |
| `protocol.py` | LXEM wire 포맷(byte layout), frame parse/build, 장비 상수 테이블 | pure |
| `scaling.py` | raw ADC volts → 물리 µV 변환 | pure |
| `config.py` | 공유 `config.toml` 로드 · 검증 → `Config` dataclass | pure |
| `bridge.py` | `frame_to_microvolts` · `SeqTracker`(순수) + `build_stream_info` · `EEGBridge`(LSL outlet) | pure + LSL-IO |
| `markers.py` | `MarkerStream` — 단일 채널 string Markers outlet | LSL-IO |
| `fake_device.py` | 합성 LXEM frame source(offline 개발 · 통합 테스트용) | pure |

## 설계 포인트

- **pure 로직과 LSL I/O 분리.** `protocol.py` / `scaling.py` / `config.py`와 `bridge.py`의
  `frame_to_microvolts` · `SeqTracker`는 `pylsl`을 import하지 않는 순수 함수/클래스라
  liblsl 없이 import·테스트할 수 있습니다. LSL I/O를 하는 부분(`build_stream_info` ·
  `EEGBridge` in `bridge.py`)은 `pylsl`을 함수/메서드 안에서 **lazy import** 합니다. 덕분에
  liblsl이 없는 macOS에서도 pure 테스트가 돌고, fake device로 full offline 통합 테스트가
  가능합니다. (단, `markers.py`는 `pylsl`을 모듈 최상단에서 import 하므로 liblsl이 필요합니다.)
- **src layout + console scripts.** 패키지는 `src/polyg_lsl/`에 있고, 설치 시 두 entry point가
  생깁니다: `polyg-bridge`( `bridge:main` )와 `polyg-fake-device`( `fake_device:main` ).
- **C++ 앱과 공유하는 `config.toml`.** 브리지와 C++ acquisition 앱은 같은 설정을 읽습니다.
  특히 `gain_idx`(PGA gain 테이블 인덱스)가 양쪽에서 **일치**해야 µV 변환 scale이 올바릅니다.
- **모듈 의존 방향.** `config.py → protocol.py`(상수/테이블), `bridge.py → scaling.py · config.py
  · protocol.py`, `fake_device.py → config.py · protocol.py`, `markers.py → pylsl`,
  `scaling.py → numpy`.

## 빠른 시작

전체 설치·배포 절차는 [README](../README.md)를 따르세요. 핵심만 요약하면:

1. 편집 가능 설치: `pip install -e .` (테스트까지 하려면 `pip install -e ".[dev]"`).
2. offline loopback 테스트 — 두 터미널에서:
   `polyg-fake-device --config config.toml` (합성 frame 송신) +
   `polyg-bridge --config config.toml` (LSL EEG outlet 발행).
3. recorder(LabRecorder 등)를 열어 `PolyG_*` EEG stream과 Markers stream을 구독·기록.

세부 옵션과 `config.toml` 필드 설명은 README와 [API 레퍼런스](./api-reference.md)에 있습니다.
