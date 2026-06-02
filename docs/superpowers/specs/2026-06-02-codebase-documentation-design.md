# 코드베이스 모듈 레퍼런스 문서화 — Design Spec

**Date:** 2026-06-02
**Status:** Draft (design); pending user review
**산출물:** 저장소 루트 `docs/` 트리 (Docs-only, 소스 미변경)

## 1. 목적 (Purpose)

이 저장소 전체를 **모듈별 상세 레퍼런스(module-by-module reference)** 관점에서
문서화한다. 대상 독자는 **코드 유지보수자/개발자**이며, 각 모듈의 함수·클래스가
무엇을 하고, 어떤 인자/반환/부작용을 가지며, 어떤 비자명한 동작이 있는지를
`docs/` 트리에서 바로 찾을 수 있게 한다.

이 저장소는 세 개의 독립적이지만 관련된 서브시스템으로 구성된다:

1. **3-PC Telescan 자동화** (`A_eeg_client.py`, `B_slide_client.py`,
   `C_controller.py`, `config.py`, `pick_coords.py`, `scenario.yaml`) — UDP로
   연결된 세 대의 PC가 GUI 소프트웨어를 자동 제어. **거의 미문서.**
2. **PolyG → Python LSL 브리지** (`polyg-lsl-bridge/`) — PolyG EEG 장비 프레임을
   Python LSL 스트림으로 outlet. **이미 충실한 README/spec/plan 존재.**
3. **제조사 C++ MFC 취득 앱** (`Test_LXSM_D1WD10_VC2017/`) — `LXSM-D1WD10.dll`로
   장비를 제어하고 프레임을 localhost UDP로 브리지에 전달. **내부 미문서**
   (빌드/수정 가이드만 존재).

## 2. 결정 사항 (Decisions, locked)

| 항목 | 결정 |
|---|---|
| 문서 목적 | 모듈별 상세 레퍼런스 (API 레퍼런스 스타일) |
| 언어 | 한국어 본문 + 영어 기술 용어 (함수명/타입/필드명은 원문 유지) |
| 출력 구조 | 저장소 루트 `docs/` 트리, 서브시스템별 파일 + `index.md` 네비게이션 |
| 커버리지 깊이 | 미문서 영역(3-PC, C++) **함수 단위 deep**; LSL 브리지 **모듈 요약 + 링크** |
| 침습성 | **Docs-only** — `.py`/`.cpp` 소스에 docstring 추가하지 않음 |
| root readme.md | 사실 오류를 **실제 코드와 일치하도록 직접 수정** + `docs/`로 링크 |
| 커밋 | 프로젝트 루트는 git 저장소가 아니므로 커밋 없음 (파일 저장만) |

## 3. 산출물 구조 (`docs/` 트리)

서브시스템별 파일 1개, 그 안에서 모듈(파일)별 `##` 섹션으로 깊이를 제공한다.
깊은 서브디렉터리 중첩은 피한다(YAGNI).

```
docs/
├─ index.md                   # 네비게이션 허브 + 서브시스템 관계도 + root readme 차이 안내
├─ three-pc-automation.md     # ★deep — 3-PC Telescan 자동화 (함수 단위 레퍼런스)
├─ cpp-acquisition-app.md     # ★deep — 제조사 C++ MFC 앱 (DLL API/핸들러/Forwarder)
├─ polyg-lsl-bridge.md        # light — LSL 브리지 모듈 요약 표 + 기존 문서 링크
└─ protocols-and-formats.md   # 교차 참조 — UDP CMD 프로토콜/scenario/파일명/CSV 로그/LXEM
```

`docs/superpowers/specs/`·`docs/superpowers/plans/`에는 본 설계/계획 문서가 들어간다
(브리지의 superpowers 문서와는 별개의 루트 docs 트리).

## 4. 모듈 레퍼런스 섹션 형식 (deep 영역 공통 틀)

각 함수/클래스마다 일관된 구조로 기술한다:

- **시그니처** — English 원문 유지 (예: `record_off() -> None`,
  `Forwarder::Send(uint32_t seq, const float* data, uint16_t num_channels, uint16_t samples_per_channel)`)
- **목적** — 한국어 1–2문장
- **인자 / 반환** — 각 인자의 의미·단위, 반환값
- **부작용(Side effects)** — 전역 상태 변경, 파일 I/O, UDP 송수신, GUI 입력 등
- **의존성** — 호출하는 다른 함수, 참조하는 전역 변수, 외부 라이브러리/DLL
- **⚠️ 비자명한 동작(Gotchas)** — 암묵적 전제, 순서 의존성, 흔한 오해

파일(모듈) 단위로는 상단에 **모듈 요약**(책임 1–2문장, 전역 상태 목록, 진입점)을 둔다.

## 5. `docs/index.md` (네비게이션 허브)

- 세 서브시스템의 **한 줄 소개 + 관계도**(ASCII): 3-PC 시스템은 Telescan을
  *버튼 자동화*만 하고 EEG를 스트리밍하지 않음; LSL 브리지는 별개 신규 시스템으로
  PolyG 장비에서 실시간 EEG를 outlet; C++ 앱은 브리지의 장비측 프런트엔드.
- 각 `docs/*.md`로의 **빠른 링크 + 한 줄 설명**.
- **"기존 root readme.md와의 차이"** 안내: 본 문서화로 root readme를 코드와 일치하도록
  수정했음을 명시하고, 과거 오류 목록을 짧게 남긴다(아래 9절).
- 실행 진입점 요약(어느 스크립트를 어느 PC에서 띄우는지)과 상세는 각 서브시스템
  문서로 위임.

## 6. `docs/three-pc-automation.md` (deep, 미문서 영역)

함수 단위로 문서화할 대상(실측 인벤토리):

- **`config.py`** — 상수 레퍼런스: `A_IP`, `B_IP`, `PORT`(4210), `SCENARIO_FILE`,
  `LOG_DIR`. 배포 시 반드시 수정해야 하는 값.
- **`A_eeg_client.py`** — `log`, `load_scenario`, `safe_filename`,
  `extract_trial_and_keyword`, `find_step_idx_by_label`, `type_korean`,
  `record_on`, `record_off`, `handle_udp_message`, `preview_filenames`.
  핵심 gotcha: `record_off`의 `first_trial` 분기(전체 저장 다이얼로그 워크 vs
  파일명만 입력), 파일명이 *이전* 스텝 keyword 기준으로 유도됨, `type_korean`의
  클립보드 경유 한글 입력, `telescan_coords.json` import-time 로드(없으면 크래시).
- **`B_slide_client.py`** — `log`, `next_slide`, `prev_slide`. UDP `NEXT`/`PREV`/`END`
  처리와 `pyautogui` 화살표 키 매핑.
- **`C_controller.py`** — `send`, `rx_loop`, `log`, `input_subject_id`,
  `scenario_worker`, `get_korean_font`, `ping_peers`. 핵심: open-loop 타이밍
  (C가 유일한 시간 소스, `time.perf_counter`), PING/PONG 라이브니스, Pygame UI 흐름.
- **`pick_coords.py`** — `telescan_coords.json` 생성 스크립트(해상도 종속, A 시작 전 필수).
- **`telescan_coords.json`** — 저장소에 없음(생성물). 구조·재생성 조건 설명.

UDP CMD 프로토콜과 scenario.yaml 문법, 파일명 유도 규칙, CSV 로그 스키마는
중복을 피해 `protocols-and-formats.md`로 위임하고 여기서는 링크.

## 7. `docs/cpp-acquisition-app.md` (deep, 미문서 영역)

- **DLL API (`LXSM-D1WD10.h`)** — `Init_Device(HWND, pid)`, `Start_Stream`,
  `Stop_Stream`, `Set_SampleFreq(idx)`, `Set_PGA(gain_idx)`,
  `Set_PGA_SourceGroup(group, gain)`, `Set_PGA_EachChannel(ch, gain)`,
  `Set_ADCMaxNumChannel(max)`, `Set_ConfigChannel(sel[])`. 각 호출의 선행조건과
  매뉴얼상 `Sleep(100)` 요구를 명시.
- **View 핸들러 (`Test_LXSM_D1WD10View.cpp`)** — `OnStreamData`(WM_AcqUnitData 처리,
  ACQPLOT 플롯 + Forwarder 전송), `OnMENUInitDevicePolyGA`, `OnMenuInitdevicePolygI`,
  `OnMENUStartStream`, `OnMENUStopStream`, `OnMENUCloseDevice`,
  `OnMENUSetPGASourceGroup`, `OnDraw`. 초기화 호출 순서와 게인 인덱스 일치 제약.
- **`Forwarder` 클래스 (`Forwarder.h`/`.cpp`)** — `Init(host, port)`, `Send(seq,
  data, num_channels, samples_per_channel)`, `Close()`. 소켓 1회 생성·재사용,
  winsock 헤더 순서(포인터 멤버 회피), LXEM 헤더 레이아웃이 `protocol.py`와 일치.
- **`BridgeConfig.h`** — `BRIDGE_HOST`, `BRIDGE_PORT`, `BRIDGE_PGA_GAIN_IDX` 매크로와
  Python `config.toml`의 `gain_idx`와 반드시 일치해야 하는 제약.
- 빌드/수정 절차는 기존 `BUILD_ko.md`·`polyg-lsl-bridge/cpp/README.md`로 링크
  (중복 금지).

## 8. `docs/polyg-lsl-bridge.md` (light, 기존 문서 존재)

각 Python 모듈을 **요약 표**로만 정리하고 상세는 기존 문서로 링크한다:

| 모듈 | 책임 | 상세 링크 |
|---|---|---|
| `protocol.py` | LXEM 프레임 parse/build, 상수 테이블 | `polyg-lsl-bridge/README.md` |
| `scaling.py` | raw→µV 변환 | 〃 |
| `config.py` | `config.toml` 로드·검증 | 〃 |
| `bridge.py` | 프레임→µV→LSL outlet | 〃 |
| `markers.py` | 시나리오 마커 LSL outlet | 〃 |
| `fake_device.py` | 합성 프레임 생성(테스트) | 〃 |

설계/계획 원문 링크: `polyg-lsl-bridge/docs/superpowers/specs/...`,
`polyg-lsl-bridge/docs/superpowers/plans/...`. LXEM 와이어 포맷의 정식 정의는
브리지 spec 5절이며, 본 트리의 `protocols-and-formats.md`는 요약만 둔다.

## 9. `docs/protocols-and-formats.md` (교차 참조)

서브시스템을 가로지르는 규약을 한 곳에 모은다(파일 단위 문서의 중복 제거):

- **UDP CMD 프로토콜 (port 4210)** — `CMD[:ARG]` 문법, A 대상
  (`SUBJECT`/`REC_ON`/`REC_OFF`/`END`), B 대상(`NEXT`/`PREV`/`END`), PING/PONG
  라이브니스. 문서/코드 불일치(미구현 `SHOW_START`/`SHOW_END`/`MARK_RESP`) 명시.
- **`scenario.yaml` 문법** — `scenario:` 리스트, `name`(회차+keyword 추출 규칙
  `\d+\.\s*...`), `dur`, `send` 형식.
- **EEG 파일명 유도 규칙** — `{subject}_{NN}회차_{keyword}_{YYYYmmdd_HHMMSS}.eeg`,
  *이전* 스텝 기준 유도, C/A의 scenario.yaml 동일성 요구.
- **CSV 로그 스키마** — `logs/`의 역할별·일자별 파일
  (`controller_*.csv`, `A_eeg_*.csv`, `B_slide_*.csv`)과 컬럼 의미.
- **LXEM 와이어 포맷 (요약)** — 16바이트 LE 헤더 + channel-major float payload;
  정식 정의는 브리지 spec 링크.

## 10. root readme.md 수정 범위 (9절 연동)

실제 코드와 어긋나는 부분만 정확히 바로잡고, `docs/`로 가는 링크를 추가한다:

- 디렉터리 구조의 파일명: `eeg_client.py`→`A_eeg_client.py`,
  `slide_client.py`→`B_slide_client.py`, `controller.py`→`C_controller.py`.
- 실행 명령 표의 스크립트명도 동일하게 수정.
- 존재하지 않는 `python controller.py --file custom.yaml` 옵션 설명 제거/정정
  (실제로는 `config.py`의 `SCENARIO_FILE` 편집).
- 미구현 명령(`MARK_RESP`, `SHOW_START`, `SHOW_END`)을 "미구현(확장 지점)"으로
  표기하거나 제거.
- 상단 또는 하단에 `docs/index.md` 링크 추가.

내용 톤·라이선스·저자 표기는 그대로 둔다(사실 오류만 정정).

## 11. 범위 밖 (Out of scope)

- 소스 코드 docstring/주석 추가 (Docs-only 결정).
- LSL 브리지 Python 모듈의 함수 단위 재문서화 (기존 문서로 충분).
- 운영자용 단계별 실행 매뉴얼 신규 작성 (기존 README들이 담당; 본 문서는 레퍼런스).
- 다이어그램 이미지/렌더링 도구 (ASCII 관계도로 충분).
- macOS 네이티브 취득(브리지 로드맵 항목) 관련 신규 문서.

## 12. 검증 방법 (How we verify the docs are correct)

- 문서화된 모든 함수/클래스 시그니처가 실제 소스와 **문자 단위로 일치**하는지
  대조(grep로 인벤토리 재확인).
- 문서가 주장하는 동작이 코드와 모순되지 않는지 스폿 체크(특히 gotcha 항목).
- 상호 링크(상대 경로)가 실제 파일을 가리키는지 확인.
- root readme.md 수정 후 남은 파일명/명령이 실제 스크립트와 일치하는지 확인.

## 13. 컴포넌트 요약 (Component summary)

| 산출물 | 깊이 | 책임 | 소스 근거 |
|---|---|---|---|
| `docs/index.md` | — | 네비게이션·관계도·readme 차이 | 전체 |
| `docs/three-pc-automation.md` | deep | 3-PC Python 함수 레퍼런스 | `A/B/C_*.py`, `config.py`, `pick_coords.py` |
| `docs/cpp-acquisition-app.md` | deep | DLL API·View 핸들러·Forwarder | `Test_LXSM_D1WD10_VC2017/*` |
| `docs/polyg-lsl-bridge.md` | light | 모듈 요약 표 + 링크 | `polyg-lsl-bridge/src/polyg_lsl/*` |
| `docs/protocols-and-formats.md` | — | UDP/scenario/파일명/CSV/LXEM 교차 참조 | 전체 |
| `readme.md` (수정) | — | 사실 오류 정정 + docs 링크 | 실제 스크립트 |
