# polyg-lsl-bridge

LAXTHA **PolyG 시리즈** 뇌파 장비에서 EEG를 받아 **µV 단위 LSL 스트림**으로 내보내고,
실험 시나리오의 자극 시점을 **별도의 LSL 마커 스트림**으로 함께 송출하는 시스템입니다.
두 스트림은 LabRecorder 같은 LSL 레코더가 **시간 동기화**해서 같이 기록합니다.

이 프로젝트는 기존 3-PC Telescan 자동화 스크립트와는 **완전히 독립적인 standalone 시스템**입니다.

---

## 1. 전체 구조

```
EEG PC (Windows 64bit)
┌───────────────────────────────────────────────────────────────┐
│  PolyG 장비 ──USB──► LXSM-D1WD10.dll                            │
│                          │ WM_AcqUnitData (float* 프레임)        │
│                          ▼                                      │
│   [C++ 취득 앱]  (제조사 Test 앱을 수정한 사본)                  │
│     · 프레임마다 16바이트 헤더를 붙여 localhost로 UDP 전송        │
│                          │ UDP 127.0.0.1:51234                 │
│                          ▼                                      │
│   [Python 브리지]  polyg-bridge                                 │
│     · 프레임 파싱/검증 → 채널 선택 → µV 변환 → LSL EEG outlet     │
└───────────────────────────────────────────────────────────────┘

시나리오 PC (자극 제시 코드가 도는 곳, 같은 PC여도 됨)
┌───────────────────────────────────────────────────────────────┐
│   [Python 마커 모듈]  markers.MarkerStream                      │
│     mk.push("scenario/choice1/onset")  → LSL 마커 outlet         │
└───────────────────────────────────────────────────────────────┘

레코더 (LabRecorder 등): EEG outlet + 마커 outlet 둘 다 구독 →
LSL이 PC 간 시계 차이를 추정해 시간 정렬하여 한 파일(.xdf)로 저장
```

**핵심 설계:** C++은 "장비에서 받은 프레임을 localhost로 던지는" 일만 합니다. 채널 선택,
µV 변환, LSL 송출, 설정은 전부 Python이 담당합니다. C++과 Python 사이의 경계는
**고정된 바이너리 프레임 포맷**이며, 나중에 Mac 네이티브 취득부로 교체하더라도 Python
쪽은 그대로 재사용됩니다.

---

## 2. 설치

### 2-1. Python 환경 (Mac/Windows/Linux 공통)

Python **3.11 이상**이 필요합니다(stdlib `tomllib` 사용). 이 저장소 폴더에서:

```bash
cd polyg-lsl-bridge
python3 -m venv .venv
. .venv/bin/activate          # Windows PowerShell: .venv\Scripts\Activate.ps1
pip install -e ".[dev]"
```

`numpy`, `pylsl`, `pytest`가 설치됩니다. `pylsl`은 네이티브 `liblsl`이 필요한데,
최신 휠에는 보통 포함되어 있습니다. 만약 `pylsl` import 시 liblsl을 못 찾는다면:

- **macOS:** `brew install labstreaminglayer/tap/lsl`
- **Windows/Linux:** [liblsl 릴리스](https://github.com/sccn/liblsl/releases)에서 바이너리 설치

설치 확인:
```bash
python -c "from polyg_lsl.config import load_config; print('ok')"
python -m pytest -q          # 24 passed 가 나오면 정상
```

### 2-2. C++ 취득 앱 (실제 장비 사용 시 EEG PC에서만 필요)

실제 PolyG 장비를 쓰려면 Windows의 Visual Studio가 필요합니다. 자세한 빌드/수정
절차는 **6절**과 `cpp/README.md`를 참고하세요. (오프라인 테스트만 할 거라면 C++ 없이도 됩니다.)

---

## 3. 설정 — `config.toml`

Python 브리지와 C++ 앱이 **같은 설정값**을 공유합니다. Python은 `config.toml`을 직접
읽고, C++은 `cpp/BridgeConfig.h`에 같은 값을 적어서 맞춥니다(v1은 C++에서 TOML을
파싱하지 않습니다).

```toml
[device]
model           = "PolyG-A"   # PolyG-A(32ch) / PolyG-E / PolyG-I(16ch) / PolyG-U(8ch)
max_channels    = 32          # 2 / 4 / 8 / 16 / 32 중 하나
sample_freq_idx = 9           # 샘플링 주파수 = 2^idx Hz  (9 → 512 Hz)
gain_idx        = 9           # PGA 게인 인덱스 (Table-5), 9 → 게인 4.25배

[scale]
fixed_gain = 1.0              # ★ 전치증폭기 고정 게인 — 장비 사양/캘리브레이션 값으로 직접 설정

[channels]
select = [1, 2, 3, 4, 5, 6, 7, 8]                            # outlet으로 내보낼 장비 채널 번호(1-based)
labels = ["Fp1", "Fp2", "F3", "F4", "C3", "C4", "P3", "P4"]  # 각 채널 라벨 (select와 길이 동일)

[transport]
host = "127.0.0.1"
port = 51234                  # C++ → Python UDP 포트
```

### 각 항목 설명

| 항목 | 의미 / 주의 |
|---|---|
| `model` | 장비 모델. C++ `Init_Device`에 넘기는 device ID로 변환됨 (PolyG-A=14, E=15, I=16, U=17). |
| `max_channels` | 장비 ADC 최대 채널 수. 채널당 데이터 수 = `512/max_channels`. |
| `sample_freq_idx` | 샘플링 = `2^idx`. **채널 수에 따라 상한이 있음**(32ch는 최대 512Hz=idx 9). 초과 시 로딩 에러. |
| `gain_idx` | PGA 게인 인덱스(0~15). 아래 게인 표 참고. **µV 변환에 사용됨.** |
| `fixed_gain` | ★ **직접 채워야 하는 값.** 전치증폭기 고정 게인. 매뉴얼에 수치가 없으므로 사양서/캘리브레이션으로 결정. 기본 1.0은 임시값. |
| `select` | 내보낼 장비 채널 번호(1부터). 예: PolyG-A의 EEG는 1~21번. |
| `labels` | 각 채널 이름. `select`와 **반드시 같은 길이**. |
| `port` | C++이 보내고 Python이 받는 localhost UDP 포트. C++ `BridgeConfig.h`와 일치해야 함. |

### µV 변환 공식

```
µV = raw_volts / (fixed_gain × pga_gain) × 1e6
```
- `raw_volts` : 장비가 주는 ADC 입력 전압 (−1.25 ~ +1.25 V)
- `pga_gain`  : `gain_idx`로 결정 (아래 표)
- `fixed_gain`: `[scale].fixed_gain` (직접 입력)

### gain_idx → PGA 게인 표 (매뉴얼 Table-5)

| idx | 게인 | idx | 게인 | idx | 게인 | idx | 게인 |
|----|------|----|------|----|------|----|------|
| 0 | 0.1 | 4 | 1.00 | 8 | 3.40 | 12 | 8.50 |
| 1 | 0.2 | 5 | 1.36 | 9 | 4.25 | 13 | 10.20 |
| 2 | 0.4 | 6 | 1.70 | 10 | 5.67 | 14 | 11.90 |
| 3 | 0.7 | 7 | 2.55 | 11 | 6.80 | 15 | 17.00 |

> ⚠️ **중요:** `config.toml`의 `gain_idx`를 바꾸면, C++ `cpp/BridgeConfig.h`의
> `BRIDGE_PGA_GAIN_IDX`도 **반드시 같은 값**으로 바꿔야 합니다. 안 그러면 장비가 실제로
> 적용한 게인과 Python의 µV 변환 게인이 달라져 **µV 값이 틀립니다.**

---

## 4. 실행 — 오프라인 (장비/C++ 없이, Mac에서도 가능)

먼저 이 방식으로 파이프라인을 검증하길 추천합니다. `fake_device`가 가짜 프레임을
생성해 브리지로 보냅니다. 터미널 3개를 엽니다(각 터미널에서
`cd polyg-lsl-bridge && . .venv/bin/activate` 먼저 실행):

```bash
# 터미널 1 — LSL EEG outlet (브리지)
polyg-bridge --config config.toml
#  → "bridge listening on 127.0.0.1:51234" 로그가 떠야 정상
#  채널별 전위(µV)를 실시간으로 보려면 --log-values 추가:
#    polyg-bridge --config config.toml --log-values
#    예) "uV Fp1=+12.34 Fp2=-5.67 ..." (매 프레임). 너무 빠르면 --log-interval 0.5 로 조절

# 터미널 2 — 가짜 장치 (합성 프레임 송신)
polyg-fake-device --config config.toml
#  → "fake device -> 127.0.0.1:51234 (...)" 로그

# 터미널 3 — 마커 (예제 시나리오)
python examples/example_scenario.py
#  → "marker: scenario/onset" 등이 출력됨
```

이제 LabRecorder를 열면 두 개의 LSL 스트림이 보입니다:
- `PolyG_PolyG-A` (type EEG) — 뇌파
- `EEG_Scenario_Markers` (type Markers) — 자극 마커

둘 다 체크하고 Start하면 시간 정렬된 `.xdf` 파일로 저장됩니다.

> 끝낼 때는 각 터미널에서 `Ctrl+C`. 가짜 장치는 `--duration 10`을 주면 10초 후 자동 종료됩니다.

---

## 5. 실행 — 실제 장비 (EEG PC, Windows 64bit)

오프라인 흐름에서 **터미널 2(가짜 장치)만 C++ 취득 앱으로 교체**하면 됩니다.

순서:

1. **C++ 앱 빌드** (6절 / `cpp/README.md` 참고) — 제조사 `PolyG_DLL_API`
   사본에 `cpp/Forwarder.*`, `BridgeConfig.h`를 넣고 x64로 빌드.
2. **`BridgeConfig.h`를 `config.toml`과 일치**시킵니다:
   - `BRIDGE_HOST` / `BRIDGE_PORT` = `[transport].host` / `port`
   - `BRIDGE_PGA_GAIN_IDX` = `[device].gain_idx`
3. **브리지 실행** (터미널 1):
   ```bash
   polyg-bridge --config config.toml
   ```
4. **C++ 앱 실행** → 메뉴에서 순서대로:
   - `Init Device` (PolyG-A 또는 PolyG-I)
   - 샘플링 주파수 메뉴(`Set Sample ...`)를 **반드시** 선택 (채널 설정 후 필수)
   - `Start Stream`
5. **시나리오/마커 실행** (터미널 2 또는 시나리오 PC): 본인 실험 코드에서
   `MarkerStream`을 import해 자극 시점마다 `push()` 호출 (7절 참고).
6. **LabRecorder**로 두 스트림을 같이 기록.

정상이라면 브리지 로그에 EEG outlet이 잡히고, `mismatch`/`dropped` 경고가 거의
없어야 합니다.

---

## 6. C++ 앱 수정 요약 (자세한 건 `cpp/README.md`)

제조사 `PolyG_DLL_API`의 **사본**에 아래만 적용합니다(원본은 건드리지 않음):

1. 프로젝트에 `Forwarder.h`, `Forwarder.cpp`, `BridgeConfig.h` 추가
2. `#if(x64)` → `#ifdef _WIN64` 로 lib 경로 분기 수정
3. View 클래스에 `Forwarder m_forwarder; uint32_t m_seq;` 멤버 추가 (생성자에서 `m_seq = 0;`)
4. `OnStreamData`에서 `printf` 루프와 기존 UDP 코드를 제거하고
   `m_forwarder.Send(m_seq++, (const float*)lParam, DISPMAXCH, DISPDATANUM)` 호출
5. 초기화 시 `Set_ADCMaxNumChannel`·`Set_PGA` 뒤에 `Sleep(100)` 추가,
   `m_forwarder.Init(BRIDGE_HOST, BRIDGE_PORT)` 호출, `Set_PGA(BRIDGE_PGA_GAIN_IDX)` 사용

장비와 대화하는 DLL 호출부·메시지 맵·실시간 파형 플로팅은 그대로 보존됩니다.

---

## 7. 시나리오에서 마커 보내기

본인의 자극 제시 코드(파이썬)에서 다음과 같이 사용합니다:

```python
from polyg_lsl.markers import MarkerStream

mk = MarkerStream(name="EEG_Scenario_Markers", source_id="scn-01")

# ... 실험 루프 안에서, 자극을 실제로 띄우는 그 시점에 최대한 가깝게 호출 ...
mk.push("scenario/choice1/onset")   # 호출 즉시 local_clock() 타임스탬프로 송출
```

- 마커는 문자열이며 원하는 라벨 체계를 쓰면 됩니다(예: `"trial1/stim_on"`, `"resp/left"`).
- **레코더를 먼저 켜고** 시나리오를 시작하세요. 그래야 첫 마커부터 기록됩니다.
- EEG outlet과 마커 outlet이 다른 PC에 있어도 LSL이 시계 차이를 보정해 정렬합니다.

실행 가능한 예시는 `examples/example_scenario.py`에 있습니다.

---

## 8. 테스트

```bash
python -m pytest -v        # 전체 (24개)
python -m pytest -q        # 요약
```

- 프로토콜/스케일링/설정 테스트는 liblsl 없이도 돌아갑니다.
- LSL 라운드트립 테스트(`test_markers`, `test_integration_lsl`, `test_end_to_end`)는
  liblsl이 있어야 실행되며, 없으면 자동으로 skip됩니다.

---

## 9. 문제 해결

| 증상 | 원인 / 해결 |
|---|---|
| 브리지에 `frame/config mismatch` 경고 폭주 | `config.toml`의 `max_channels`/`model`이 C++ 장비 설정과 불일치. 채널 수 일치 확인. |
| `dropped N frame(s)` 경고 | UDP 프레임 유실(고샘플링에서 가끔 정상). 지속되면 CPU 부하/네트워크 확인. |
| µV 값이 이상하게 큼/작음 | `fixed_gain` 또는 `gain_idx`가 실제 장비와 불일치. **`BRIDGE_PGA_GAIN_IDX` == `gain_idx`** 확인. |
| LabRecorder에 스트림이 안 보임 | 같은 서브넷인지, 방화벽(특히 LSL 멀티캐스트) 확인. 브리지/레코더 실행 순서는 무관. |
| `pylsl` import 에러 | liblsl 미설치. 2-1절의 liblsl 설치 안내 참고. |
| C++ `Start_Stream`이 -4 반환 | 채널 수 대비 샘플링 주파수가 너무 높음. `sample_freq_idx`를 낮추거나 채널 수 줄이기. |

---

## 10. 로드맵 — macOS 네이티브 취득 (v1 범위 밖)

`LXSM-D1WD10.dll`은 Windows 전용이라 Mac에서 직접 못 돌립니다. 향후 Mac에서 PolyG를
쓰려면 (1) 제조사의 Mac/크로스플랫폼 드라이버를 받거나, (2) 장비 USB 칩에 `libusb`로
직접 붙어 프로토콜을 재구현해야 합니다. 이때 **localhost 와이어 프로토콜이 이식성의
경계선**이므로, Mac 취득부가 같은 프레임 포맷만 내보내면 이 저장소의 Python 브리지·
LSL outlet·마커 모듈·설정은 **그대로 재사용**됩니다.
