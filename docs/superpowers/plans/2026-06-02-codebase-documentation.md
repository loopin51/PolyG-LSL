# 코드베이스 모듈 레퍼런스 문서화 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 저장소 루트에 `docs/` 트리를 만들어 세 서브시스템을 모듈/함수 단위로 한국어(영어 기술용어 유지) 레퍼런스 문서화하고, 오래된 `readme.md`의 사실 오류를 실제 코드와 일치하도록 정정한다.

**Architecture:** Docs-only(소스 코드 미변경). 서브시스템별 파일 1개 + 모듈별 `##` 섹션. 미문서 영역(3-PC, C++)은 함수 단위 deep, 이미 문서가 있는 LSL 브리지는 요약 표 + 링크. 교차 규약(UDP/포맷/로그)은 별도 파일로 모아 중복 제거.

**Tech Stack:** Markdown만. 검증은 `grep`/`iconv`로 문서화한 시그니처가 실제 소스와 일치하는지 대조 + 상대 링크 존재 확인.

> **저장소 주의:** 프로젝트 루트는 git 저장소가 **아니다**. 따라서 각 Task의 종료 게이트는 "commit"이 아니라 **검증(verification)**이다. `git add/commit` 단계는 없다.

> **인코딩 주의:** `PolyG_DLL_API/*.cpp`·`*.h`는 **CP949** 인코딩이다. 내용 확인은 반드시 `iconv -f CP949 -t UTF-8 <파일>`로 디코드해서 읽는다. 단, 문서(`docs/*.md`)와 루트 `readme.md`는 **UTF-8**로 작성한다.

---

## 문서화 대상 인벤토리 (실측, 시그니처 원문)

이 절은 작업자가 정확한 시그니처/동작을 옮겨 적을 수 있도록 소스에서 추출한 사실이다.
프로즈는 작업자가 한국어로 작성하되, 시그니처/식별자는 아래 원문을 그대로 쓴다.

### 3-PC 시스템 (Python)

`config.py` — 모듈 상수(함수 없음):
- `A_IP = "192.168.1.100"` (EEG PC), `B_IP = "192.168.1.102"` (슬라이드 PC)
- `PORT = 4210` (단일 포트), `SCENARIO_FILE = "scenario.yaml"`, `LOG_DIR = "logs"`

`A_eeg_client.py` (188줄) — 모듈 전역: `subject_id`, `coords`, `pos`(lambda), `cur_label`, `current_step_idx`, `trial`, `first_trial`, `scenario`, `sock`. 함수:
- `log(msg: str) -> None` — `logs/A_eeg_{YYYY-MM-DD}.csv`에 `ISO타임스탬프,msg` append.
- `load_scenario(yaml_path: str = "scenario.yaml") -> list[dict]` — YAML의 `scenario:` 리스트 반환.
- `safe_filename(s: str) -> str` — `[\/:*?"<>|]`를 `_`로 치환.
- `extract_trial_and_keyword(label: str) -> tuple[int, str]` — `'1. 선택지1' → (1,'선택지1')`; 숫자 없으면 `(1, label)`.
- `find_step_idx_by_label(label: str) -> int` — (trial,keyword) 완전일치 우선, 없으면 keyword만 일치, 그래도 없으면 0.
- `type_korean(text: str) -> None` — `pyperclip.copy` 후 `Ctrl+V` 붙여넣기(한글 입력용).
- `record_on(label: str = "noLabel") -> None` — `cur_label`/`trial`/`current_step_idx` 갱신 후 `REC_START` 좌표 클릭. 부작용: 전역 3개 변경 + 마우스 클릭 + 로그.
- `record_off() -> None` — `REC_STOP` 클릭 → **직전 스텝**(`max(current_step_idx-1,0)`) keyword로 파일명 `{safe_id}_{trial:02d}회차_{safe_step}_{YYYYmmdd_HHMMSS}.eeg` 조립 → `first_trial`이면 전체 저장 다이얼로그 워크(바탕화면→새 폴더(피험자명)→더블클릭→파일명), 아니면 파일명만 입력. 끝에 `cur_label=None`, `trial+=1`, `first_trial=False`.
- `handle_udp_message(msg: str, addr) -> None` — `PING`→`PONG` 응답; `SUBJECT:<id>`→`subject_id` 갱신; `REC_ON[:label]`→`record_on`; `REC_OFF`→`record_off`; `END`→`raise SystemExit`.
- `preview_filenames(topic: str = "주제A") -> None` — 시나리오를 돌며 `A:REC_OFF` 스텝마다 예상 파일명 출력(디버그용, 기본 호출 안 함).
- ⚠️ gotcha: `telescan_coords.json`을 **import 시점**에 `json.load`(없으면 즉시 크래시); `pyautogui.FAILSAFE=True`(모서리로 마우스→중단); 파일명이 *현재*가 아니라 *직전* 스텝 keyword 기준.

`B_slide_client.py` (48줄) — 함수: `log(msg)`, `next_slide()`(→`pyautogui.press("right")`), `prev_slide()`(→`left`). 메인 루프에서 `PING`→`PONG`, `NEXT`/`PREV`/`END` 처리. 로그 파일 `logs/B_slide_{date}.csv`.

`C_controller.py` (156줄) — 모듈 전역: `_state`(step/remain/running), `peer`(A_IP/B_IP→bool), `sock`, `subject_id`, `FONT`/`SMALL`, `screen`/`clock`, `btn_rect`. 함수:
- `send(ip, msg)` — `sock.sendto` + `TX→` 로그; `OSError`면 로그+`peer[ip]=False`(크래시 안 함).
- `rx_loop()` — 데몬 스레드; `PONG` 수신 시 `peer[addr[0]]=True`.
- `log(txt)` — `logs/controller_{date}.csv` append.
- `input_subject_id()` — Pygame 텍스트 입력 박스로 피험자명 입력(Enter 확정).
- `scenario_worker()` — `SUBJECT:` 먼저 전송 → `SCENARIO_FILE` 로드 → 각 스텝 `send` 발사 후 `time.perf_counter` 기반 `dur` 카운트다운 → 끝에 `END` 2회 전송. (데몬 스레드로 START 시 기동)
- `get_korean_font(size=46, bold=False)` — AppleGothic→Malgun Gothic→Nanum 패밀리→Pygame 기본 순으로 폰트 탐색.
- `ping_peers()` — `peer` 리셋 후 A/B에 `PING`(메인 루프에서 1초 주기 호출).
- ⚠️ gotcha: open-loop 타이밍(C가 유일한 시간 소스, ACK 없음); UDP 재전송 없음.

`pick_coords.py` (35줄) — 스크립트(함수 없음). 5초 대기 후 `KEYS`(`REC_START`,`REC_STOP`,`ARROW_DOWN`,`DESKTOP_BTN`,`NEW_FOLDER_BTN`,`FOLDER_NAME_BOX`,`FOLDER_DOUBLECLICK`,`FILENAME_BOX`)를 각 3초 호버로 `pyautogui.position()` 수집 → `telescan_coords.json` 저장. A 시작 전 1회 필요, 해상도 종속.

### C++ MFC 앱 (`PolyG_DLL_API/`, CP949)

`LXSM-D1WD10.h` — DLL API(전부 `extern "C" __declspec(dllimport) short`):
- `#define WM_AcqUnitData WM_USER+1`
- `Init_Device(HWND msgtarget_window, int pid)` — 장치 초기화, 메시지 받을 HWND + 장치 ID(PolyG-A=14, PolyG-I=16).
- `Close_Device()`, `Start_Stream()`(선행 Init, 1회), `Stop_Stream()`.
- `Set_SampleFreq(unsigned char samplefreq_idx)` — `2^idx` Hz.
- `Set_PGA(unsigned char gain_idx)` — 전채널 동일 게인.
- `Set_PGA_SourceGroup(unsigned char sourcegroup_idx, unsigned char gain_idx)`.
- `Set_PGA_EachChannel(unsigned char channel_idx, unsigned char gain_idx)`.
- `Set_ADCMaxNumChannel(unsigned char maxnum_channel)` — `2^n` 채널; 정상 시 반환값 = 채널당 데이터 수.
- `Set_ConfigChannel(unsigned char *Is_Select_Channel)` — 채널 선택.

`Test_LXSM_D1WD10View.cpp` — 핸들러:
- `OnStreamData(WPARAM, LPARAM)` — `lParam`=float 프레임 포인터. `ACQPLOT_DLL_Array_Datain_Strip((float*)lParam, DISPMAXCH, DISPDATANUM)` 플롯 + `m_forwarder->Send(m_seq++, (const float*)lParam, (uint16_t)DISPMAXCH, (uint16_t)DISPDATANUM)`.
- `OnMENUInitDevicePolyGA()` — `Init_Device(m_hWnd,14)` → 성공 시 `Sleep(1000)` → `DISPMAXCH=33`(32+marking) → `Set_ADCMaxNumChannel(DISPMAXCH-1)` → `Sleep(100)` → `DISPDATANUM=retv` → `Set_PGA(BRIDGE_PGA_GAIN_IDX)` → `Sleep(100)` → `m_forwarder->Init(BRIDGE_HOST,BRIDGE_PORT)` → `m_seq=0`. 반환코드: 1성공/-1실패/-2이미초기화/-3전송실패.
- `OnMenuInitdevicePolygI()` — 동일 패턴, `Init_Device(...,16)`, `DISPMAXCH=17`(16+marking).
- 샘플링 주파수 메뉴 그룹: `OnMENUSetSample128/256/512/1024/2048/4096` → 각각 `Set_SampleFreq(7/8/9/10/11/12)`. 반환코드 -4(현재 최대채널에서 미지원 주파수)/-10(인덱스 0~14).
- ADC 최대채널 메뉴 그룹: `OnMENUSetADCMaxNumCh32/16/8/4/2`.
- `OnMENUStartStream()`, `OnMENUStopStream()`, `OnMENUCloseDevice()`, `OnMENUSetPGASourceGroup()`, `OnDraw(CDC*)`.

`Forwarder.h`/`Forwarder.cpp` — 클래스 `Forwarder`:
- `Forwarder()` / `~Forwarder()`(→`Close()`).
- `bool Init(const char* host, unsigned short port)` — `WSAStartup(2,2)` + UDP 소켓 1회 생성 + `inet_pton`로 dest 설정.
- `bool Send(uint32_t seq, const float* data, uint16_t num_channels, uint16_t samples_per_channel)` — 16바이트 LE 헤더(`magic 0x4C58454D`, `version 1`, nch, spc, `flags 0`, seq) + payload `memcpy` → `sendto`. 헤더 레이아웃은 `protocol.py`와 일치.
- `void Close()` — 소켓 닫고 `WSACleanup`.
- private: `SOCKET m_sock`, `sockaddr_in m_dest`, `bool m_wsa`.
- ⚠️ gotcha: `<winsock2.h>`가 `<windows.h>`보다 먼저 와야 하므로 View.h에는 전방선언+포인터 멤버만 두고 `Forwarder.h`는 `.cpp`에서만 include.

`BridgeConfig.h` — 매크로: `BRIDGE_HOST "127.0.0.1"`, `BRIDGE_PORT 51234`, `BRIDGE_PGA_GAIN_IDX 9`. ⚠️ `BRIDGE_PGA_GAIN_IDX`는 반드시 `config.toml`의 `[device].gain_idx`와 동일해야 µV 일치.

### LSL 브리지 (Python, light — 요약만)

`polyg-lsl-bridge/src/polyg_lsl/`: `protocol.py`, `scaling.py`, `config.py`, `bridge.py`, `markers.py`, `fake_device.py`. 상세는 `polyg-lsl-bridge/README.md`와 `polyg-lsl-bridge/docs/superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md` 참조.

---

## File Structure (생성/수정)

```
docs/
├─ index.md                   # Task 5 — 네비게이션 허브
├─ three-pc-automation.md     # Task 2 — deep
├─ cpp-acquisition-app.md     # Task 3 — deep
├─ polyg-lsl-bridge.md        # Task 4 — light
└─ protocols-and-formats.md   # Task 1 — 교차 참조
readme.md                     # Task 6 — 사실 오류 정정 (기존 파일 수정)
```

작업 순서 근거: 교차 참조(Task 1)를 먼저 만들어 deep 문서가 링크할 수 있게 하고, deep/light 문서를 채운 뒤, 모든 링크가 확정된 상태에서 index(Task 5)와 readme(Task 6)를 작성, 마지막에 전체 검증(Task 7).

---

### Task 1: `docs/protocols-and-formats.md` (교차 참조)

**Files:**
- Create: `docs/protocols-and-formats.md`
- Read for facts: `A_eeg_client.py`, `B_slide_client.py`, `C_controller.py`, `scenario.yaml`, `polyg-lsl-bridge/docs/superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md` (§5)

- [ ] **Step 1: 파일 작성**

다음 5개 `##` 섹션을 가진 UTF-8 마크다운을 작성한다. 각 섹션 내용은 위 인벤토리 사실에 근거한다.

1. `## UDP CMD 프로토콜 (port 4210)` — 문법 `CMD[:ARG]`(첫 `:`에서 split). 표:

   | 대상 | 명령 | 의미 | 구현 위치 |
   |---|---|---|---|
   | A | `SUBJECT:<id>` | 피험자 ID 갱신 | `A_eeg_client.handle_udp_message` |
   | A | `REC_ON[:label]` | 녹화 시작 | `record_on` |
   | A | `REC_OFF` | 녹화 정지·저장 | `record_off` |
   | A | `END` | 종료(`SystemExit`) | `handle_udp_message` |
   | B | `NEXT` / `PREV` | 슬라이드 이동 | `next_slide`/`prev_slide` |
   | B | `END` | 루프 종료 | `B_slide_client` 메인 |
   | A·B | `PING`→`PONG` | 라이브니스 | 각 스크립트 |

   ⚠️ 문서/코드 불일치 명시: `MARK_RESP`, `SHOW_START`, `SHOW_END`는 **클라이언트에 미구현**(확장 지점).

2. `## scenario.yaml 문법` — `scenario:` 리스트, 각 스텝 `{ name, dur, send }`. `name`의 회차+keyword 추출 정규식 `\s*(\d+)\.\s*(.*)`. `dur`(float, `time.perf_counter` 카운트다운). `send`=`[<A|B>:<CMD>, ...]`(스텝 시작 시 1회 발사, `send` 없으면 순수 지연).

3. `## EEG 파일명 유도 규칙` — `{safe_id}_{trial:02d}회차_{safe_step}_{YYYYmmdd_HHMMSS}.eeg`. **직전 스텝**(`current_step_idx-1`) keyword 사용. `trial`은 `REC_ON` 라벨에서 동기화, `REC_OFF` 후 +1. C와 A의 `scenario.yaml`이 동일해야 파일명 정확.

4. `## CSV 로그 스키마` — 역할·일자별 파일: `controller_{date}.csv`, `A_eeg_{date}.csv`, `B_slide_{date}.csv`. 컬럼: `ISO타임스탬프(ms),메시지`. 메시지 예: `TX→<ip>:<msg>`, `REC_START:<label>`, `REC_SAVED:<fname>`, `NEXT`.

5. `## LXEM 와이어 포맷 (요약)` — 16바이트 LE 헤더(`magic 0x4C58454D`, `version 1`, `num_channels`, `samples_per_channel`, `flags`, `seq u32`) + channel-major float32 payload. **정식 정의 링크**: `../polyg-lsl-bridge/docs/superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md` (§5).

- [ ] **Step 2: 검증 — 사실 일치 확인**

Run:
```bash
cd "$(git rev-parse --show-toplevel)"
grep -n "MARK_RESP\|SHOW_START\|SHOW_END" A_eeg_client.py B_slide_client.py C_controller.py
```
Expected: **출력 없음**(미구현이 사실임을 확인). 출력이 있으면 문서의 "미구현" 표기를 수정.

```bash
grep -n "회차_" A_eeg_client.py
```
Expected: `record_off`의 `fname = ...{trial:02d}회차_...` 라인이 보여 파일명 규칙이 문서와 일치.

- [ ] **Step 3: 링크 존재 확인**

Run:
```bash
ls polyg-lsl-bridge/docs/superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md
```
Expected: 파일 존재(LXEM 링크 유효).

---

### Task 2: `docs/three-pc-automation.md` (deep)

**Files:**
- Create: `docs/three-pc-automation.md`
- Read for facts: `config.py`, `A_eeg_client.py`, `B_slide_client.py`, `C_controller.py`, `pick_coords.py`

- [ ] **Step 1: 파일 작성 — 모듈별 섹션**

UTF-8 마크다운. 상단에 한 줄 개요(세 스크립트는 별도 PC에서 실행, UDP 4210 공유) + "교차 규약(프로토콜/파일명/로그)은 [protocols-and-formats.md](./protocols-and-formats.md) 참조" 링크. 이어 모듈별 `##` 섹션. 각 함수는 본 계획 상단 인벤토리의 **시그니처 원문**을 헤더로 쓰고, 4절 spec 틀(목적/인자·반환/부작용/의존성/⚠️gotcha)로 기술한다.

섹션 구성:
- `## config.py` — 상수 5개를 표로(이름/값/의미/배포 시 수정 여부).
- `## A_eeg_client.py` — 모듈 요약(전역 상태 목록: `subject_id`,`coords`,`cur_label`,`current_step_idx`,`trial`,`first_trial`,`scenario`,`sock`) + 함수 10개(`log`,`load_scenario`,`safe_filename`,`extract_trial_and_keyword`,`find_step_idx_by_label`,`type_korean`,`record_on`,`record_off`,`handle_udp_message`,`preview_filenames`). `record_off`에는 first_trial 분기와 "직전 스텝 기준 파일명"을 ⚠️로 강조. import-time `telescan_coords.json` 로드와 `FAILSAFE`도 모듈 요약 gotcha로.
- `## B_slide_client.py` — `log`,`next_slide`,`prev_slide` + 메인 루프 명령 처리.
- `## C_controller.py` — 모듈 요약(`_state`,`peer`,스레드 모델) + 함수 7개(`send`,`rx_loop`,`log`,`input_subject_id`,`scenario_worker`,`get_korean_font`,`ping_peers`). open-loop 타이밍·UDP 무재전송을 ⚠️로.
- `## pick_coords.py` — 좌표 수집 절차, `KEYS` 목록, `telescan_coords.json` 산출. 해상도 종속·A 선행조건 ⚠️.

- [ ] **Step 2: 검증 — 문서화한 모든 함수가 실제로 존재**

Run:
```bash
cd "$(git rev-parse --show-toplevel)"
for fn in log load_scenario safe_filename extract_trial_and_keyword find_step_idx_by_label type_korean record_on record_off handle_udp_message preview_filenames; do grep -q "def $fn" A_eeg_client.py || echo "MISSING in A: $fn"; done
for fn in log next_slide prev_slide; do grep -q "def $fn" B_slide_client.py || echo "MISSING in B: $fn"; done
for fn in send rx_loop log input_subject_id scenario_worker get_korean_font ping_peers; do grep -q "def $fn" C_controller.py || echo "MISSING in C: $fn"; done
```
Expected: **출력 없음**(모든 함수 존재). `MISSING` 출력이 있으면 해당 섹션을 실제 코드에 맞게 수정.

- [ ] **Step 3: 검증 — 링크 해석**

Run: `grep -o "protocols-and-formats.md" docs/three-pc-automation.md && ls docs/protocols-and-formats.md`
Expected: 매치 + 파일 존재.

---

### Task 3: `docs/cpp-acquisition-app.md` (deep)

**Files:**
- Create: `docs/cpp-acquisition-app.md`
- Read for facts (CP949): `PolyG_DLL_API/LXSM-D1WD10.h`, `Test_LXSM_D1WD10View.cpp`, `Forwarder.h`, `Forwarder.cpp`, `BridgeConfig.h`

- [ ] **Step 1: 파일 작성 — 모듈별 섹션**

UTF-8 마크다운. 상단 개요(이 앱은 LSL 브리지의 장비측 프런트엔드; DLL이 `WM_AcqUnitData`로 프레임을 HWND에 post → `OnStreamData`가 받아 Forwarder로 localhost UDP 전송). 빌드/수정 절차는 새로 쓰지 말고 링크: `../PolyG_DLL_API/BUILD_ko.md`, `../polyg-lsl-bridge/cpp/README.md`.

섹션 구성:
- `## DLL API (LXSM-D1WD10.h)` — 위 인벤토리 9개 함수 + `WM_AcqUnitData` 매크로를 표/목록으로(시그니처 원문 + 선행조건 + 반환코드 의미).
- `## View 핸들러 (Test_LXSM_D1WD10View.cpp)` — `OnStreamData`(핫패스: ACQPLOT + Forwarder.Send), `OnMENUInitDevicePolyGA`/`OnMenuInitdevicePolygI`(초기화 순서: Init→Sleep→Set_ADCMaxNumChannel→Sleep(100)→Set_PGA→Sleep(100)→Forwarder.Init; DISPMAXCH 33/17), 샘플링 주파수 메뉴 그룹(`OnMENUSetSampleNNN`→`Set_SampleFreq(idx)` 매핑 표 128→7…4096→12), ADC 최대채널 메뉴 그룹, `OnMENUStartStream`/`OnMENUStopStream`/`OnMENUCloseDevice`/`OnMENUSetPGASourceGroup`/`OnDraw`. ⚠️ Start_Stream 전 샘플링 주파수 메뉴 1회 선택 필요.
- `## Forwarder 클래스 (Forwarder.h/.cpp)` — `Init`/`Send`/`Close` 시그니처 원문 + 16바이트 헤더 레이아웃 표(offset/size/field) + "헤더가 protocol.py와 일치" + winsock 헤더 순서 ⚠️(포인터 멤버 회피).
- `## BridgeConfig.h` — 매크로 3개 표 + `BRIDGE_PGA_GAIN_IDX == config.toml [device].gain_idx` 제약 ⚠️.

- [ ] **Step 2: 검증 — DLL API/핸들러/클래스 멤버 실존 (CP949 디코드)**

Run:
```bash
cd "$(git rev-parse --show-toplevel)/PolyG_DLL_API"
for fn in Init_Device Close_Device Start_Stream Stop_Stream Set_SampleFreq Set_PGA Set_PGA_SourceGroup Set_PGA_EachChannel Set_ADCMaxNumChannel Set_ConfigChannel; do iconv -f CP949 -t UTF-8 LXSM-D1WD10.h | grep -q "$fn" || echo "MISSING DLL: $fn"; done
for h in OnStreamData OnMENUInitDevicePolyGA OnMenuInitdevicePolygI OnMENUStartStream OnMENUStopStream OnMENUCloseDevice OnMENUSetPGASourceGroup; do iconv -f CP949 -t UTF-8 Test_LXSM_D1WD10View.cpp | grep -q "$h" || echo "MISSING handler: $h"; done
for m in "bool Init" "bool Send" "void Close"; do grep -q "$m" Forwarder.h || echo "MISSING Forwarder member: $m"; done
grep -q "BRIDGE_PGA_GAIN_IDX" BridgeConfig.h || echo "MISSING macro"
```
Expected: **출력 없음**. `MISSING` 출력이 있으면 해당 항목을 실제 코드에 맞게 정정.

- [ ] **Step 3: 검증 — 링크 해석**

Run:
```bash
cd "$(git rev-parse --show-toplevel)"
ls PolyG_DLL_API/BUILD_ko.md polyg-lsl-bridge/cpp/README.md
```
Expected: 두 파일 모두 존재(링크 유효).

---

### Task 4: `docs/polyg-lsl-bridge.md` (light)

**Files:**
- Create: `docs/polyg-lsl-bridge.md`
- Read for facts: `polyg-lsl-bridge/src/polyg_lsl/` 파일 목록, `polyg-lsl-bridge/README.md`

- [ ] **Step 1: 파일 작성 — 요약 표 + 링크**

UTF-8 마크다운. 상단 1문단: 이 서브시스템은 별개 신규 시스템으로 이미 충실한 문서가 있으므로 여기서는 **요약 + 링크**만 제공. 본문은 표 1개:

| 모듈 | 책임 | 상세 |
|---|---|---|
| `protocol.py` | LXEM 프레임 parse/build, 상수 테이블 | [README](../polyg-lsl-bridge/README.md) |
| `scaling.py` | raw→µV 변환 | 〃 |
| `config.py` | `config.toml` 로드·검증 | 〃 |
| `bridge.py` | 프레임→µV→LSL EEG outlet | 〃 |
| `markers.py` | 시나리오 마커 LSL outlet | 〃 |
| `fake_device.py` | 합성 프레임 생성(테스트) | 〃 |

하단: 설계/계획 원문 링크 2개(`../polyg-lsl-bridge/docs/superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md`, `../polyg-lsl-bridge/docs/superpowers/plans/2026-06-02-eeg-lsl-bridge.md`)와 C++ 프런트엔드는 [cpp-acquisition-app.md](./cpp-acquisition-app.md) 참조.

- [ ] **Step 2: 검증 — 모듈 파일 실존 + 링크**

Run:
```bash
cd "$(git rev-parse --show-toplevel)"
for m in protocol scaling config bridge markers fake_device; do ls polyg-lsl-bridge/src/polyg_lsl/$m.py || echo "MISSING module: $m"; done
ls polyg-lsl-bridge/README.md polyg-lsl-bridge/docs/superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md polyg-lsl-bridge/docs/superpowers/plans/2026-06-02-eeg-lsl-bridge.md
```
Expected: 모든 파일 존재, `MISSING` 없음.

---

### Task 5: `docs/index.md` (네비게이션 허브)

**Files:**
- Create: `docs/index.md`
- 선행: Task 1–4 완료(링크 대상 존재)

- [ ] **Step 1: 파일 작성**

UTF-8 마크다운. 구성:
1. 제목 + 1문단 소개(이 저장소는 EEG 실험 인프라; 세 서브시스템).
2. `## 서브시스템 지도` — ASCII 관계도:
   ```
   3-PC 자동화 (Telescan 버튼 자동화, EEG 스트리밍 아님)   ── 독립
   PolyG LSL 브리지 (실시간 EEG → LSL outlet)  ◄── C++ 취득 앱(장비 프런트엔드)
   ```
   한 줄 설명: 3-PC는 Telescan을 좌표 클릭으로 녹화/정지만 자동화하고 뇌파를 직접 스트리밍하지 않음; LSL 브리지는 PolyG 장비에서 실시간 µV를 outlet하는 별개 신규 시스템.
3. `## 문서 navigation` — 표:

   | 문서 | 내용 |
   |---|---|
   | [three-pc-automation.md](./three-pc-automation.md) | 3-PC Telescan 자동화 함수 레퍼런스 |
   | [cpp-acquisition-app.md](./cpp-acquisition-app.md) | 제조사 C++ MFC 앱(DLL API·핸들러·Forwarder) |
   | [polyg-lsl-bridge.md](./polyg-lsl-bridge.md) | LSL 브리지 모듈 요약 + 링크 |
   | [protocols-and-formats.md](./protocols-and-formats.md) | UDP/scenario/파일명/로그/LXEM 교차 참조 |

4. `## 실행 진입점` — 어느 스크립트를 어느 PC에서(A=`A_eeg_client.py`, B=`B_slide_client.py`, C=`C_controller.py`; 워커 A·B 먼저, C 나중) — 상세는 root `../readme.md`로 위임.
5. `## 기존 readme.md와의 차이` — Task 6에서 root readme의 사실 오류(파일명 `eeg_client.py`→`A_eeg_client.py` 등, 미구현 `--file`/`MARK_RESP`/`SHOW_START`)를 정정했음을 기록.

- [ ] **Step 2: 검증 — 모든 내부 링크 해석**

Run:
```bash
cd "$(git rev-parse --show-toplevel)/docs"
for f in three-pc-automation cpp-acquisition-app polyg-lsl-bridge protocols-and-formats; do grep -q "$f.md" index.md && ls $f.md || echo "BAD LINK: $f"; done
```
Expected: 4개 모두 매치 + 존재, `BAD LINK` 없음.

---

### Task 6: root `readme.md` 사실 오류 정정

**Files:**
- Modify: `readme.md` (UTF-8)

- [ ] **Step 1: 파일명 정정 (디렉터리 구조 블록 + 실행 순서 표)**

`readme.md`에서 다음 문자열 치환:
- `eeg_client.py` → `A_eeg_client.py`
- `slide_client.py` → `B_slide_client.py`
- `controller.py` → `C_controller.py`

해당 위치: 디렉터리 구조 코드블록(`├─ controller.py` 등)과 "실행 순서" 표의 명령(`python eeg_client.py` 등). 단, 아래 Step 2의 `--file` 문장은 별도 처리.

- [ ] **Step 2: 존재하지 않는 CLI 옵션 정정**

다음 문장을 교체:
- 기존: `> ... 다른 시나리오 파일을 쓰려면 python controller.py --file custom.yaml 과 같이 옵션을 주면 됩니다.`
- 변경: `> 다른 시나리오 파일을 쓰려면 config.py의 SCENARIO_FILE 상수를 수정하세요. (현재 C_controller.py에는 --file 같은 CLI 인자 파싱이 없습니다.)`

- [ ] **Step 3: 미구현 명령 표기 정정**

"명령 목록" 블록에서 미구현 명령을 표기:
- `A : REC_ON, REC_OFF, MARK_RESP (직접 추가 가능)` → `A : REC_ON, REC_OFF, SUBJECT, END  (MARK_RESP는 미구현 — 확장 지점)`
- `B : SHOW_START, SHOW_END, NEXT, PREV` → `B : NEXT, PREV, END  (SHOW_START/SHOW_END는 미구현 — 확장 지점)`
- 문제 해결/커스터마이징 표에 `MARK_XXX` 언급이 있으면 "미구현 확장 지점"임을 한 줄 덧붙임.

- [ ] **Step 4: docs 링크 추가**

`readme.md` 최상단 소개 문단 바로 아래에 한 줄 추가:
```markdown
> 📚 모듈/함수 단위 상세 레퍼런스는 [docs/index.md](docs/index.md) 참조.
```

- [ ] **Step 5: 검증 — 잔존 오류 없음**

Run:
```bash
cd "$(git rev-parse --show-toplevel)"
grep -nE "eeg_client\.py|slide_client\.py|[^_]controller\.py|--file custom" readme.md
```
Expected: **출력 없음**(옛 파일명·옛 CLI 옵션이 모두 사라짐). 출력이 있으면 해당 줄을 마저 정정.

```bash
grep -q "docs/index.md" readme.md && echo "LINK OK"
```
Expected: `LINK OK`.

---

### Task 7: 전체 일관성·링크 최종 검증

**Files:**
- Read: `docs/*.md`, `readme.md`

- [ ] **Step 1: 모든 docs 파일 생성 확인**

Run:
```bash
cd "$(git rev-parse --show-toplevel)"
ls docs/index.md docs/three-pc-automation.md docs/cpp-acquisition-app.md docs/polyg-lsl-bridge.md docs/protocols-and-formats.md
```
Expected: 5개 모두 존재.

- [ ] **Step 2: docs 내 모든 상대 링크가 실제 파일을 가리키는지 확인**

Run:
```bash
cd "$(git rev-parse --show-toplevel)/docs"
grep -rhoE "\]\(([^)]+\.md)[^)]*\)" *.md | sed -E 's/.*\(([^)#]+).*/\1/' | sort -u | while read p; do [ -e "$p" ] && echo "OK   $p" || echo "BAD  $p"; done
```
Expected: 모든 줄이 `OK` (상대경로 기준 파일 존재). `BAD` 가 있으면 해당 링크 수정.

- [ ] **Step 3: UTF-8 인코딩 확인 (docs는 UTF-8이어야)**

Run:
```bash
cd "$(git rev-parse --show-toplevel)"
file docs/*.md readme.md
```
Expected: 각 파일이 `UTF-8`(또는 ASCII) 텍스트로 표시. CP949/ISO-8859로 나오면 UTF-8로 재저장.

- [ ] **Step 4: 스폿 체크 — deep 문서 시그니처 무작위 대조**

Run:
```bash
cd "$(git rev-parse --show-toplevel)"
grep -q "record_off() -> None" docs/three-pc-automation.md && echo "A sig OK"
grep -q "Set_ADCMaxNumChannel" docs/cpp-acquisition-app.md && echo "C++ sig OK"
```
Expected: `A sig OK` 와 `C++ sig OK` 모두 출력.

---

## Self-Review (작성 후 점검 결과)

**1. Spec coverage:** spec §3 산출물 5개 = Task 1–5; §10 readme 정정 = Task 6; §12 검증 방법 = 각 Task의 검증 단계 + Task 7. §6 3-PC deep = Task 2; §7 C++ deep = Task 3; §8 브리지 light = Task 4; §9 교차 참조 = Task 1. §5 index 구성요소(관계도/링크/readme 차이) = Task 5. 누락 없음.

**2. Placeholder scan:** "TBD/TODO/적절히" 없음. 각 문서 작성 단계는 섹션 구성과 채울 사실(시그니처·gotcha)을 명시. 검증은 실제 grep 명령 + 기대 출력 제시.

**3. Type consistency:** 함수명·매크로명·파일명은 인벤토리 원문과 Task 전반에서 동일 표기(`record_off`, `Set_ADCMaxNumChannel`, `BRIDGE_PGA_GAIN_IDX`, `A_eeg_client.py`). 링크 경로는 docs/ 기준 상대경로로 통일(`./` docs 내부, `../` 저장소 루트/형제 디렉터리).

**참고:** 프로젝트 루트가 git 저장소가 아니므로 commit 단계는 의도적으로 제외했고, 각 Task의 종료 게이트는 grep/ls/iconv/file 기반 검증으로 대체했다.
