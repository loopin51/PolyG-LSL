# PolyG-LSL

LAXTHA **PolyG 시리즈** 뇌파 장비에서 EEG를 실시간으로 취득해 **µV 단위 LSL 스트림**으로
내보내고, 실험 시나리오의 자극 시점을 **별도의 LSL 마커 스트림**으로 함께 송출하는 시스템입니다.
두 스트림은 LabRecorder 같은 LSL 레코더가 **시간 동기화**해서 하나의 `.xdf` 파일로 기록합니다.

장비-측 프레임 취득은 C++ 앱(`PolyG_DLL_API`)이, 파싱·µV 변환·LSL 송출은
Python 브리지(`polyg-lsl-bridge`)가 담당합니다. 둘 사이의 경계는 **고정된 바이너리 프레임
포맷(LXEM)** 이라, 향후 취득부를 다른 플랫폼으로 교체해도 Python 쪽은 그대로 재사용됩니다.

> 📦 기존의 **3-PC Telescan/PowerPoint 자동화 스크립트**(A/B/C 클라이언트)는
> 이 시스템과 무관하므로 [`legacy/`](legacy/)로 이동했습니다. 본 README는 그 레거시가 아닌
> 현재의 LSL 브리지 시스템을 설명합니다.

---

## 📂 리포지터리 구조

```text
PolyG-LSL/
├─ polyg-lsl-bridge/      # ★ Python LSL 브리지 (메인) — 프레임 파싱·µV 변환·LSL outlet·마커
│  ├─ src/polyg_lsl/      #   protocol / scaling / config / bridge / markers
│  ├─ cpp/                #   C++ 취득 앱에 끼워 넣는 Forwarder·BridgeConfig + 연동 README
│  ├─ examples/           #   example_scenario.py (마커 송출 예제)
│  ├─ tests/              #   pytest (24개)
│  ├─ config.toml         #   장비·채널·스케일·전송 설정 (C++과 값 공유)
│  └─ README.md           #   ★ 설치·실행·설정 상세 가이드
│
├─ PolyG_DLL_API/         # ★ 제조사 C++ MFC 취득 앱 (장비-측 front-end)
│  ├─ LXSM-D1WD10.h/.dll  #   PolyG 장비 구동 DLL + API 헤더
│  ├─ Forwarder.*         #   LXEM 프레임을 localhost UDP로 forward
│  ├─ BridgeConfig.h      #   host/port/gain_idx를 config.toml과 미러링
│  └─ BUILD_ko.md         #   Visual Studio 2017 x64 빌드 절차
│
├─ docs/                  # 서브시스템별 상세 레퍼런스 (index.md에서 출발)
└─ legacy/                # 구 3-PC 자동화 스크립트 (보관용, 비활성)
```

---

## 🧭 전체 데이터 흐름

```
EEG PC (Windows 64bit)
  PolyG 장비 ──USB──► LXSM-D1WD10.dll
                          │ WM_AcqUnitData (float* 프레임)
                          ▼
     [C++ 취득 앱]  PolyG_DLL_API
        · 프레임마다 16바이트 LXEM 헤더를 붙여 localhost로 UDP 전송
                          │ UDP 127.0.0.1:51234
                          ▼
     [Python 브리지]  polyg-bridge
        · 프레임 파싱/검증 → 채널 선택 → µV 변환 → LSL EEG outlet

시나리오 PC (자극 제시 코드, 같은 PC여도 됨)
     [Python 마커]  markers.MarkerStream
        mk.push("scenario/choice1/onset")  → LSL 마커 outlet

레코더 (LabRecorder 등): EEG outlet + 마커 outlet 둘 다 구독 →
LSL이 PC 간 시계 차이를 보정해 시간 정렬하여 한 파일(.xdf)로 저장
```

**핵심 설계:** C++은 "장비에서 받은 프레임을 localhost로 던지는" 일만 합니다.
채널 선택·µV 변환·LSL 송출·설정은 전부 Python이 담당합니다.

---

## 🔧 요구 사항

| 항목 | 버전/비고 |
|---|---|
| Python | **3.11 이상** (stdlib `tomllib` 사용) |
| Python 라이브러리 | `numpy`, `pylsl` (+ 개발 시 `pytest`) — `pip install -e ".[dev]"`로 설치 |
| liblsl | `pylsl`이 의존하는 네이티브 라이브러리 (대개 휠에 포함, 없으면 별도 설치) |
| C++ (실장비 시) | Windows + Visual Studio 2017, **x64 구성** |
| 장비 | LAXTHA PolyG-A(32ch) / PolyG-E / PolyG-I(16ch) / PolyG-U(8ch) |
| 레코더 | LabRecorder 등 LSL 레코더 (선택) |

> 🛈 오프라인 테스트(가짜 장치)는 C++/장비 없이 Mac/Linux/Windows 어디서나 가능합니다.

---

## 🚀 설치 (Python 브리지)

```bash
cd polyg-lsl-bridge
python3 -m venv .venv
. .venv/bin/activate          # Windows PowerShell: .venv\Scripts\Activate.ps1
pip install -e ".[dev]"
```

설치 확인:

```bash
python -c "from polyg_lsl.config import load_config; print('ok')"
python -m pytest -q           # 24 passed 가 나오면 정상
```

`pylsl` import 시 liblsl을 못 찾으면:

- **macOS:** `brew install labstreaminglayer/tap/lsl`
- **Windows/Linux:** [liblsl 릴리스](https://github.com/sccn/liblsl/releases)에서 바이너리 설치

---

## ▶️ 실행 — 오프라인 (장비/C++ 없이)

먼저 이 방식으로 파이프라인을 검증하길 권장합니다. 터미널 3개를 열고 각각
`cd polyg-lsl-bridge && . .venv/bin/activate` 후:

```bash
# 터미널 1 — LSL EEG outlet (브리지)
polyg-bridge --config config.toml

# 터미널 2 — 가짜 장치 (합성 프레임 송신)
polyg-fake-device --config config.toml

# 터미널 3 — 마커 (예제 시나리오)
python examples/example_scenario.py
```

LabRecorder를 열면 두 스트림(`PolyG_PolyG-A` / EEG, `EEG_Scenario_Markers` / Markers)이
보이며, 둘 다 체크하고 Start하면 시간 정렬된 `.xdf`로 저장됩니다.

## ▶️ 실행 — 실제 장비 (EEG PC, Windows 64bit)

오프라인 흐름에서 **터미널 2(가짜 장치)만 C++ 취득 앱으로 교체**하면 됩니다.

1. **C++ 앱 빌드** — [`PolyG_DLL_API/BUILD_ko.md`](PolyG_DLL_API/BUILD_ko.md),
   [`polyg-lsl-bridge/cpp/README.md`](polyg-lsl-bridge/cpp/README.md) 참고 (x64 빌드).
2. **`BridgeConfig.h`를 `config.toml`과 일치**시킵니다
   (`BRIDGE_HOST`/`BRIDGE_PORT` ↔ `[transport]`, `BRIDGE_PGA_GAIN_IDX` ↔ `[device].gain_idx`).
3. **브리지 실행:** `polyg-bridge --config config.toml`
4. **C++ 앱 실행** → 메뉴에서 `Init Device` → 샘플링 주파수 메뉴 선택(필수) → `Start Stream`.
5. **시나리오/마커 실행** → 본인 실험 코드에서 `MarkerStream.push()` 호출.
6. **LabRecorder**로 두 스트림을 함께 기록.

자세한 절차는 [`polyg-lsl-bridge/README.md`](polyg-lsl-bridge/README.md) 5절을 참고하세요.

---

## ⚙️ 설정 — `polyg-lsl-bridge/config.toml`

Python 브리지와 C++ 앱이 **같은 설정값**을 공유합니다(Python은 TOML을 직접 읽고, C++은
`BridgeConfig.h`에 같은 값을 적어 맞춥니다).

```toml
[device]
model           = "PolyG-A"   # PolyG-A(32ch) / PolyG-E / PolyG-I(16ch) / PolyG-U(8ch)
max_channels    = 32          # 2 / 4 / 8 / 16 / 32 중 하나
sample_freq_idx = 9           # 샘플링 = 2^idx Hz (9 → 512 Hz)
gain_idx        = 9           # PGA 게인 인덱스, µV 변환에 사용

[scale]
fixed_gain = 1.0              # ★ 전치증폭기 고정 게인 — 사양/캘리브레이션 값으로 직접 설정

[channels]
select = [1, 2, 3, 4, 5, 6, 7, 8]
labels = ["Fp1", "Fp2", "F3", "F4", "C3", "C4", "P3", "P4"]   # select와 길이 동일

[transport]
host = "127.0.0.1"
port = 51234                  # C++ → Python UDP 포트 (BridgeConfig.h와 일치)
```

> ⚠️ `gain_idx`를 바꾸면 C++ `BridgeConfig.h`의 `BRIDGE_PGA_GAIN_IDX`도 **반드시 같은 값**으로
> 바꿔야 합니다. 다르면 장비 게인과 Python의 µV 변환이 어긋나 µV 값이 틀립니다.
> µV 변환 공식과 게인 표(Table-5)는 [`polyg-lsl-bridge/README.md`](polyg-lsl-bridge/README.md) 3절 참고.

---

## 📝 시나리오에서 마커 보내기

본인의 자극 제시 코드(파이썬)에서:

```python
from polyg_lsl.markers import MarkerStream

mk = MarkerStream(name="EEG_Scenario_Markers", source_id="scn-01")
# ... 자극을 실제로 띄우는 그 시점에 최대한 가깝게 호출 ...
mk.push("scenario/choice1/onset")   # 호출 즉시 local_clock() 타임스탬프로 송출
```

- 마커 문자열은 원하는 라벨 체계를 쓰면 됩니다(예: `"trial1/stim_on"`, `"resp/left"`).
- **레코더를 먼저 켠 뒤** 시나리오를 시작해야 첫 마커부터 기록됩니다.
- 실행 가능한 예시는 [`polyg-lsl-bridge/examples/example_scenario.py`](polyg-lsl-bridge/examples/example_scenario.py).

---

## 🔌 LXEM 와이어 프로토콜 (C++ ↔ Python 경계)

C++ `Forwarder`가 프레임마다 16바이트 little-endian 헤더 + 채널-major float 페이로드를
localhost UDP로 보냅니다. 이 포맷은 `src/polyg_lsl/protocol.py`와 정확히 일치합니다.

| offset | size | field | 값/비고 |
|--------|------|-------|---------|
| 0 | 4 | magic | `0x4C58454D` ('LXEM') |
| 4 | 2 | version | `1` |
| 6 | 2 | num_channels | `DISPMAXCH` |
| 8 | 2 | samples_per_channel | `DISPDATANUM` |
| 10 | 2 | flags | `0` |
| 12 | 4 | seq | 프레임 시퀀스 카운터 |
| 16 | payload | float 데이터 | `num_channels × samples_per_channel × 4` 바이트 |

C++ DLL API·View 핸들러·Forwarder의 상세는 [`docs/cpp-acquisition-app.md`](docs/cpp-acquisition-app.md),
패킷 ↔ LSL 스트림 흐름은 [`docs/network-and-streams.md`](docs/network-and-streams.md)를 참고하세요.

---

## 🧪 테스트

```bash
cd polyg-lsl-bridge
python -m pytest -v        # 전체 (24개)
```

프로토콜/스케일링/설정 테스트는 liblsl 없이 돌아가고, LSL 라운드트립 테스트는 liblsl이
없으면 자동으로 skip됩니다.

---

## 🛠️ 문제 해결

| 증상 | 원인 / 해결 |
|---|---|
| 브리지에 `frame/config mismatch` 경고 폭주 | `config.toml`의 `max_channels`/`model`이 C++ 장비 설정과 불일치 |
| `dropped N frame(s)` 경고 | UDP 프레임 유실(고샘플링에서 가끔 정상). 지속되면 CPU/네트워크 확인 |
| µV 값이 이상하게 큼/작음 | `BRIDGE_PGA_GAIN_IDX` == `gain_idx`, `fixed_gain` 값 확인 |
| LabRecorder에 스트림이 안 보임 | 같은 서브넷·방화벽(LSL 멀티캐스트) 확인 (실행 순서는 무관) |
| `pylsl` import 에러 | liblsl 미설치 — 위 설치 안내 참고 |
| C++ `Start_Stream`이 -4 반환 | 채널 수 대비 샘플링 주파수가 너무 높음 — `sample_freq_idx`/채널 수 낮추기 |

더 자세한 항목은 [`polyg-lsl-bridge/README.md`](polyg-lsl-bridge/README.md) 9절을 참고하세요.

---

## 📚 문서

| 문서 | 내용 |
|---|---|
| [polyg-lsl-bridge/README.md](polyg-lsl-bridge/README.md) | 브리지 설치·실행·설정·마커·테스트 전체 가이드 |
| [docs/index.md](docs/index.md) | 서브시스템 지도와 문서 허브 |
| [docs/polyg-lsl-bridge.md](docs/polyg-lsl-bridge.md) | LSL 브리지 모듈 레퍼런스 |
| [docs/cpp-acquisition-app.md](docs/cpp-acquisition-app.md) | C++ MFC 앱 (DLL API·핸들러·Forwarder) |
| [docs/network-and-streams.md](docs/network-and-streams.md) | UDP 패킷 ↔ LSL 스트림 네트워크/데이터 흐름 |
| [docs/protocols-and-formats.md](docs/protocols-and-formats.md) | 프로토콜·포맷 교차 참조 |
| [PolyG_DLL_API/BUILD_ko.md](PolyG_DLL_API/BUILD_ko.md) | C++ 앱 Visual Studio 빌드 절차 |
