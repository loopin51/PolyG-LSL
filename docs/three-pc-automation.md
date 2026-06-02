# 3-PC 자동화 시스템 모듈 레퍼런스

세 대의 물리적으로 분리된 PC가 동일한 LAN에서 UDP 포트 `4210`을 공유하며 동작하는 EEG 실험 자동화 시스템의 **모듈/함수 단위 레퍼런스**다.
**A = EEG PC**(`A_eeg_client.py`)는 Telescan 녹화 소프트웨어를 미리 추출한 화면 좌표 클릭으로 자동화하고, **B = 슬라이드 PC**(`B_slide_client.py`)는 PowerPoint 슬라이드쇼를 방향키로 넘기며, **C = 컨트롤러 PC**(`C_controller.py`)는 시나리오 타이머·Pygame 상태 UI·UDP 브로드캐스트를 실행하는 운영자 머신이다.
중요한 점: 이 3-PC 시스템은 **EEG 신호를 스트리밍하지 않는다.** C는 시나리오 타임라인에 맞춰 단지 녹화 시작/정지 버튼 자동화의 타이밍만 잡아 주고, 실제 EEG 기록·저장은 Telescan이 담당한다.

> **공통(cross-cutting) 규약** — UDP CMD 프로토콜, `scenario.yaml` 문법, EEG 파일명 유도 규칙, CSV 로그 스키마 등은 본 문서에서 상세 중복하지 않고 [protocols-and-formats.md](./protocols-and-formats.md)로 링크한다. 본 문서는 각 모듈의 **코드 구조와 함수 동작**에 집중한다.

---

## config.py

전 스크립트가 공유하는 배포 상수 모음. 함수는 없고 모듈 상수만 정의한다. A/B/C가 모두 `from config import ...` 로 읽으므로, **세 PC 모두 동일한 `config.py`**를 두어야 IP·포트가 일치한다.

| 이름 | 값 | 의미 | 배포 시 수정 |
| --- | --- | --- | --- |
| `A_IP` | `"192.168.1.100"` | EEG PC(Telescan)의 고정 IP | ✅ 랩 네트워크에 맞게 |
| `B_IP` | `"192.168.1.102"` | 슬라이드 PC(PowerPoint)의 고정 IP | ✅ 랩 네트워크에 맞게 |
| `PORT` | `4210` | A/B/C 공용 UDP 포트(단일 포트) | 보통 그대로 |
| `SCENARIO_FILE` | `"scenario.yaml"` | C가 로드하는 시나리오 경로 | 시나리오 교체 시 |
| `LOG_DIR` | `"logs"` | 각 스크립트의 CSV 로그 저장 폴더 | 보통 그대로 |

> `PORT`/`SCENARIO_FILE`/`LOG_DIR`은 세 스크립트가 동일하게 임포트한다. `A_IP`/`B_IP`는 C(송신)와, C의 `rx_loop`에서 PONG 출처를 식별하는 데 쓰인다.

---

## A_eeg_client.py

**책임** — UDP로 들어온 명령(`REC_ON`/`REC_OFF`/`END`/`SUBJECT`/`PING`)을 받아, `pyautogui` 좌표 클릭으로 Telescan의 녹화 시작·정지·파일 저장 대화상자를 자동 조작한다. EEG 파일명은 로컬 `scenario.yaml`을 다시 파싱해 **유도(derive)**한다(상세 규칙은 [protocols-and-formats.md](./protocols-and-formats.md) 참조).

**전역 상태(globals)**
- `subject_id: str` — 피험자 ID. 기본 `"subj001"`, 컨트롤러의 `SUBJECT:` 메시지로 갱신.
- `coords: dict` — `telescan_coords.json`에서 로드한 `{키: [x, y]}` 좌표 맵.
- `pos` — `lambda key: coords[key]`. `pos("REC_STOP")` 형태로 좌표를 꺼내는 헬퍼.
- `cur_label: str | None` — 현재 진행 중인 `REC_ON` 라벨.
- `current_step_idx: int` — `find_step_idx_by_label`로 해석한 현재 시나리오 인덱스.
- `trial: int` — 회차 카운터(1부터). `record_off`에서 저장 후 +1.
- `first_trial: bool` — 첫 저장 여부 플래그. 저장 대화상자 경로 분기에 사용.
- `scenario: list[dict]` — 모듈 로드 시 `load_scenario()`로 파싱한 시나리오 스텝 목록.
- `sock` — `0.0.0.0:PORT`에 바인딩된 UDP 소켓.

**엔트리 포인트** — 모듈 하단의 `try/while True` 메인 루프. `sock.recvfrom`으로 수신 → `handle_udp_message`로 디스패치. `SystemExit`(=`END`) 시 루프 종료, `finally`에서 소켓 close.

> ⚠️ **모듈 레벨 게차(gotcha) 1** — `coords = json.load(open("telescan_coords.json", ...))`가 **import 시점에 즉시 실행**된다. 파일이 없으면 다른 함수 정의 이전에 `FileNotFoundError`로 즉사한다. 그래서 A를 띄우기 전 반드시 `pick_coords.py`로 좌표 파일을 만들어야 한다.
> ⚠️ **모듈 레벨 게차 2** — `pyautogui.FAILSAFE = True`(그리고 `pyautogui.PAUSE = 0.07`). 마우스를 화면 모서리로 밀면 `pyautogui`가 예외를 던져 스크립트가 중단된다. 운영자의 비상 정지 수단이다.

### `log(msg: str) -> None`
- **목적** — 동작/이벤트를 일자별 CSV(`logs/A_eeg_YYYY-MM-DD.csv`)에 한 줄 append 한다.
- **인자 / 반환** — `msg`: 기록할 문자열 / 반환 없음.
- **부작용** — 파일 I/O(append). 매 호출마다 ISO 밀리초 타임스탬프를 생성하고 `"{ts},{msg}\n"`을 쓴다.
- **의존성** — `datetime`, `LOG_PATH`(전역 `pathlib.Path`).

### `load_scenario(yaml_path: str = "scenario.yaml") -> list[dict]`
- **목적** — YAML 시나리오 파일을 열어 최상위 `scenario:` 리스트를 반환한다.
- **인자 / 반환** — `yaml_path`: 시나리오 경로(기본 `"scenario.yaml"`) / `list[dict]`(스텝 목록).
- **부작용** — 파일 I/O(읽기).
- **의존성** — `yaml.safe_load`. 모듈 로드 시 `scenario = load_scenario()`로 한 번 호출된다.
- **⚠️ 비자명한 동작** — `config.SCENARIO_FILE`을 **쓰지 않고** 기본 인자 `"scenario.yaml"`을 사용한다(C는 `SCENARIO_FILE` 사용). 따라서 A의 작업 디렉터리에 `scenario.yaml`이 있어야 하며, 그 내용은 C의 시나리오와 일치해야 파일명 유도가 맞는다.

### `safe_filename(s: str) -> str`
- **목적** — 파일명에 쓸 수 없는 문자(`\ / : * ? " < > |`)를 `_`로 치환한다.
- **인자 / 반환** — `s`: 원본 문자열 / 정제된 문자열.
- **부작용** — 없음(순수 함수).
- **의존성** — `re.sub`.

### `extract_trial_and_keyword(label: str) -> tuple[int, str]`
- **목적** — `"1. 선택지1"` 같은 라벨을 `(회차, 키워드)`로 분리한다.
- **인자 / 반환** — `label`: 라벨/스텝명 / `(int, str)`.
- **부작용** — 없음(순수 함수).
- **의존성** — `re.match(r"\s*(\d+)\.\s*(.*)", label)`.
- **⚠️ 비자명한 동작** — 앞에 `숫자.` 패턴이 없으면 `(1, label.strip())`로 **trial=1을 가정**한다. `record_on`이 받는 `REC_ON` 라벨은 보통 선행 숫자가 없으므로(예: `choice1`) 이 함수만으로는 회차가 항상 1로 나온다 — 실제 회차 증가는 `record_off`의 `trial += 1`이 담당한다.

### `find_step_idx_by_label(label: str) -> int`
- **목적** — 주어진 라벨과 가장 잘 맞는 시나리오 스텝의 인덱스를 찾는다.
- **인자 / 반환** — `label`: REC_ON 라벨 / 매칭 인덱스(없으면 `0`).
- **부작용** — 없음(전역 `scenario`만 읽음).
- **의존성** — `extract_trial_and_keyword`, 전역 `scenario`.
- **⚠️ 비자명한 동작** — 2단계 매칭이다. ① `(회차, 키워드)`가 모두 일치하는 스텝을 먼저 찾고, 없으면 ② **키워드만** 일치하는 첫 스텝을 반환한다. 둘 다 실패하면 `0`(첫 스텝)을 반환하므로, 라벨 오타 시 조용히 첫 스텝으로 폴백한다.

### `type_korean(text: str) -> None`
- **목적** — Telescan 파일명/폴더명 입력 칸에 한글을 입력한다.
- **인자 / 반환** — `text`: 입력할 문자열 / 반환 없음.
- **부작용** — 클립보드 변경(`pyperclip.copy`) + GUI 입력(Ctrl+V 붙여넣기).
- **의존성** — `pyperclip.copy`, `pyautogui.hotkey("ctrl","v")`.
- **⚠️ 비자명한 동작** — `pyautogui.typewrite`는 한글을 칠 수 없어 클립보드 경유로 붙여넣는다. 그래서 `pyperclip`이 하드 의존성이며, 시스템 클립보드 내용을 덮어쓴다.

### `record_on(label: str = "noLabel") -> None`
- **목적** — Telescan 녹화 시작 버튼을 클릭하고 현재 라벨·시나리오 위치·회차 상태를 갱신한다.
- **인자 / 반환** — `label`: REC_ON 라벨(기본 `"noLabel"`) / 반환 없음.
- **부작용** — 전역 `cur_label`/`current_step_idx`/`trial` 변경, GUI 클릭(`pos("REC_START")`), CSV 로그(`REC_START:{label}`).
- **의존성** — `extract_trial_and_keyword`, `find_step_idx_by_label`, `pyautogui.click`, `pos`, `log`.
- **⚠️ 비자명한 동작** — `trial, _ = extract_trial_and_keyword(label)`로 회차를 "동기화"하지만, 라벨에 숫자가 없으면 매번 `trial=1`로 덮어쓴다(위 `extract_trial_and_keyword` 게차 참조). ACK가 없으므로 클릭이 실제로 먹혔는지 확인하지 않는다.

### `record_off() -> None`
- **목적** — Telescan 녹화 정지 버튼을 클릭하고, 파일명을 유도해 저장 대화상자를 자동 조작한 뒤 회차를 +1 한다.
- **인자 / 반환** — 없음 / 반환 없음.
- **부작용** — GUI 클릭·키 입력 다수, 클립보드 변경(`type_korean` 경유), 전역 `cur_label`/`trial`/`first_trial` 변경, CSV 로그(`REC_SAVED:{fname}`). `time.sleep`로 대화상자 대기.
- **의존성** — 전역 `current_step_idx`/`scenario`/`subject_id`/`trial`/`first_trial`, `extract_trial_and_keyword`, `safe_filename`, `type_korean`, `pyautogui`, `pos`, `datetime`, `log`.
- **⚠️ 비자명한 동작 1 (파일명은 직전 스텝 기준)** — 저장명은 `prev_idx = max(current_step_idx - 1, 0)`, 즉 **현재 스텝이 아니라 직전(PREVIOUS) 스텝**의 키워드로 만든다. 파일명 포맷은 `{safe_id}_{trial:02d}회차_{safe_step}_{timestamp}.eeg`. 따라서 녹화는 자신의 정지를 유발한 스텝의 *앞 스텝* 이름을 갖는다. (포맷·근거 상세는 [protocols-and-formats.md](./protocols-and-formats.md).)
- **⚠️ 비자명한 동작 2 (`first_trial` 분기 — 가장 깨지기 쉬운 부분)** —
  - **첫 저장(`first_trial == True`)**: 전체 Telescan 저장 대화상자를 좌표 클릭으로 끝까지 걷는다. 파일명 입력 → Enter ×2 → `ARROW_DOWN` 클릭 → `DESKTOP_BTN`(바탕화면) 클릭 → `NEW_FOLDER_BTN`(새 폴더) 클릭 → 폴더명으로 `safe_id`(피험자명) 입력 → Enter ×2 → `FOLDER_DOUBLECLICK` 더블클릭으로 폴더 진입 → 파일명 입력 → Enter ×2. 끝나면 `first_trial = False`.
  - **이후 저장(`first_trial == False`)**: 위 폴더 생성 과정을 모두 생략하고 **파일명만 입력 → Enter ×2**.
  - 이 시퀀스는 좌표·Enter 횟수가 Telescan 대화상자 레이아웃에 강하게 결합되어 있어, 대화상자나 화면 배치가 바뀌면 즉시 깨진다.
- **⚠️ 비자명한 동작 3 (회차 증가의 진짜 출처)** — 함수 끝의 `trial += 1`이 실제 회차 카운터를 올린다. `record_on`의 라벨 기반 동기화는 보통 숫자가 없어 효과가 없으므로, 정확한 `NN회차` 값은 이 누적 증가에 의존한다.

### `handle_udp_message(msg: str, addr) -> None`
- **목적** — 수신한 UDP 문자열을 파싱해 적절한 동작으로 디스패치한다.
- **인자 / 반환** — `msg`: 디코드·strip된 명령 문자열, `addr`: 송신자 `(ip, port)` / 반환 없음.
- **부작용** — `PING`이면 `addr`로 `PONG` 송신, `SUBJECT:`면 전역 `subject_id` 변경 + 콘솔 출력, 그 외 `REC_ON`/`REC_OFF`/`END` 디스패치. `END`는 `SystemExit`을 raise.
- **의존성** — 전역 `sock`/`subject_id`, `record_on`, `record_off`.
- **⚠️ 비자명한 동작** — `PING`과 `SUBJECT:`를 먼저 가로채고, 나머지는 `msg.split(":", 1)`로 첫 콜론 기준 1회 분리한다(`CMD[:ARG]`). 알 수 없는 명령은 조용히 무시된다(어떤 분기에도 안 걸림). 프로토콜 문법은 [protocols-and-formats.md](./protocols-and-formats.md) 참조.

### `preview_filenames(topic: str = "주제A") -> None`
- **목적** — 시나리오를 훑어 `A:REC_OFF`가 있는 스텝마다 예상 파일명을 콘솔에 출력하는 디버그/점검용 헬퍼.
- **인자 / 반환** — `topic`: 파일명에 끼울 주제 문자열(기본 `"주제A"`) / 반환 없음.
- **부작용** — 콘솔 출력만(파일 저장·GUI 없음).
- **의존성** — 전역 `scenario`/`subject_id`, `extract_trial_and_keyword`, `safe_filename`.
- **⚠️ 비자명한 동작** — 기본 호출이 주석 처리(`# preview_filenames()`)되어 평소엔 실행되지 않는다. 또한 출력 포맷(`{subject}_{topic}_{NN}회차_{step}.eeg`)은 실제 `record_off`의 저장 포맷(`{subject}_{NN}회차_{step}_{timestamp}.eeg`)과 **다르다** — 어디까지나 미리보기 용도라 timestamp는 빠지고 topic이 끼어 있다. 실제 저장명과 1:1로 신뢰하면 안 된다.

---

## B_slide_client.py

**책임** — UDP `NEXT`/`PREV`로 PowerPoint 슬라이드쇼를 방향키로 넘기고, `PING`에 `PONG`으로 응답하며, `END`로 종료한다. A보다 훨씬 단순하다.

**전역/설정** — `pyautogui.PAUSE = 0.05`, `pyautogui.FAILSAFE = True`(A와 동일하게 모서리 비상정지). `LOG_DIR`은 `config.LOG_DIR`을 `LOG_DIR_STR`로 임포트해 `pathlib.Path`로 감싼 것. `sock`은 `0.0.0.0:PORT` 바인딩 UDP 소켓.

**엔트리 포인트** — 모듈 하단의 `while True` 루프. `recvfrom` → 디코드/strip → `PING`이면 `PONG` 응답 후 continue, `NEXT`→`next_slide()`, `PREV`→`prev_slide()`, `END`→`break`(루프 탈출로 종료). 알 수 없는 메시지는 무시.

### `log(msg)`
- **목적** — 이벤트를 일자별 CSV(`logs/B_slide_YYYY-MM-DD.csv`)에 한 줄 append.
- **인자 / 반환** — `msg`: 기록 문자열 / 반환 없음.
- **부작용** — 파일 I/O(append), 타임스탬프 생성.
- **의존성** — `datetime`, `LOG_DIR`(전역 `pathlib.Path`).

### `next_slide()`
- **목적** — 다음 슬라이드로 넘긴다.
- **인자 / 반환** — 없음 / 반환 없음.
- **부작용** — GUI 키 입력 `pyautogui.press("right")`, CSV 로그(`NEXT`).
- **의존성** — `pyautogui`, `log`.

### `prev_slide()`
- **목적** — 이전 슬라이드로 되돌린다.
- **인자 / 반환** — 없음 / 반환 없음.
- **부작용** — GUI 키 입력 `pyautogui.press("left")`, CSV 로그(`PREV`).
- **의존성** — `pyautogui`, `log`.

---

## C_controller.py

**책임** — 운영자 머신. 피험자 이름을 입력받고(Pygame), START 시 시나리오 스레드를 띄워 스텝별로 `dur` 만큼 대기하며 스텝 경계에서 A/B로 UDP 명령을 송신한다. 동시에 1초마다 `PING`을 쏘고 `PONG`을 수집해 피어 연결 상태를 UI에 표시한다.

**전역 상태(globals)**
- `_state: dict` — `{"step": 표시 단계명, "remain": 남은 초, "running": 진행 여부}`. 시나리오 스레드가 갱신하고 메인 루프가 그려 읽는다(락 없는 공유 dict).
- `peer: dict` — `{A_IP: bool, B_IP: bool}`. PONG 수신/송신 결과로 연결 여부 표시.
- `sock` — `("", PORT)` 바인딩 UDP 소켓(송수신 공용).
- `subject_id: str` — `input_subject_id()`가 채우는 피험자 ID.
- UI 핸들: `screen`, `FONT`, `SMALL`, `clock`, `btn_rect`(START 버튼 영역).

**스레딩 모델** — ① `rx_loop`를 **데몬 스레드**로 즉시 시작(항상 PONG 수집). ② START(버튼 클릭 또는 Space) 시 `scenario_worker`를 **데몬 스레드**로 시작. 메인 스레드는 Pygame 이벤트/렌더 루프를 돈다.

**엔트리 포인트** — 모듈 하단: `pygame.init()` → 화면/폰트 생성 → `input_subject_id()`(이름 입력 블로킹) → `while True` 메인 루프(이벤트 처리·1초 PING·UI 그리기).

> ⚠️ **타이밍은 open-loop** — C가 **유일한 시계**다. 스텝마다 `time.perf_counter` 기준으로 `dur`만큼 자고 경계에서 명령을 한 번 송신한다. A/B에는 타이머가 없고 즉시 반응한다. 명령 실행 여부에 대한 **ACK가 없고**(PING/PONG 생존성만 확인), **UDP 재전송도 없다**. 패킷이 유실되면 그 스텝 명령은 그냥 누락된다.

### `send(ip, msg)`
- **목적** — 지정 IP로 UDP 명령 문자열을 보내고 TX 로그를 남긴다.
- **인자 / 반환** — `ip`: 대상 IP, `msg`: 명령 문자열 / 반환 없음.
- **부작용** — UDP 송신, CSV 로그(`TX→{ip}:{msg}`). 실패 시 `OSError`를 잡아 `ERR:` 로그 + `peer[ip]=False` 표시 후 계속 진행.
- **의존성** — 전역 `sock`/`peer`, `log`.
- **⚠️ 비자명한 동작** — `OSError`만 삼키고 프로그램은 죽지 않는다. 송신 성공이 곧 수신/실행 성공을 뜻하지 않는다(UDP).

### `rx_loop()`
- **목적** — 백그라운드에서 계속 수신하며 `PONG`이 오면 그 출처 IP를 연결됨으로 표시한다.
- **인자 / 반환** — 없음 / 반환 없음(무한 루프).
- **부작용** — 전역 `peer` 변경. 블로킹 `recvfrom`.
- **의존성** — 전역 `sock`/`peer`.
- **⚠️ 비자명한 동작** — `PONG`만 처리하고 다른 수신은 버린다. 데몬 스레드로 돌아 메인 종료 시 함께 죽는다. `peer[addr[0]]`로 출처 IP를 키 삼으므로 A/B IP가 `config`와 일치해야 표시가 맞는다.

### `log(txt)`
- **목적** — 이벤트를 일자별 CSV(`logs/controller_YYYY-MM-DD.csv`)에 append.
- **인자 / 반환** — `txt`: 기록 문자열 / 반환 없음.
- **부작용** — 파일 I/O(append), 타임스탬프 생성.
- **의존성** — `datetime`, `LOG_PATH`(전역 `pathlib.Path`).

### `input_subject_id()`
- **목적** — Pygame 입력 박스로 피험자 이름을 받아 전역 `subject_id`에 저장한다(Enter로 확정).
- **인자 / 반환** — 없음 / 반환 없음.
- **부작용** — 전역 `subject_id` 변경, Pygame 이벤트 소비·렌더(블로킹 루프). 창 닫기(`QUIT`) 시 `pygame.quit(); sys.exit()`로 프로그램 종료.
- **의존성** — `pygame`, 전역 `screen`/`FONT`/`clock`.
- **⚠️ 비자명한 동작** — 입력 박스를 클릭해 `active` 상태가 되어야만 키 입력을 받는다. 메인 루프 진입 전에 한 번 호출되어 이름이 확정될 때까지 블로킹한다. `subject_id`는 나중에 `scenario_worker`가 `SUBJECT:`로 A에 전송한다.

### `scenario_worker()`
- **목적** — START 시 시나리오를 순차 실행한다. 먼저 A에 피험자 ID를 보내고, 스텝마다 명령을 송신한 뒤 `dur`만큼 카운트다운하고, 끝나면 A/B에 `END`를 보낸다.
- **인자 / 반환** — 없음 / 반환 없음.
- **부작용** — UDP 송신 다수(`SUBJECT:` → 스텝별 명령 → `END`), 전역 `_state` 갱신, 파일 I/O(시나리오 읽기).
- **의존성** — `send`, `yaml.safe_load`, `time.perf_counter`/`sleep`, 전역 `_state`/`subject_id`, `config.SCENARIO_FILE`/`A_IP`/`B_IP`.
- **⚠️ 비자명한 동작** — ① 시나리오를 `SCENARIO_FILE`로 직접 다시 읽는다(전역 캐시 없음). ② `send`의 타깃 판정은 `A_IP if tgt.strip()=="A" else B_IP` — 즉 `"A"`가 아니면 무조건 B로 간다(오타 타깃은 B로 샘). ③ 카운트다운은 `while (rem:=dur-(...))>0`로 `_state["remain"]`만 갱신하며 0.05초 간격으로 잔다. 데몬 스레드라 메인이 죽으면 중단된다. 명령은 스텝 시작 시 1회만 송신(재시도 없음).

### `get_korean_font(size=46, bold=False)`
- **목적** — 시스템에서 한글 폰트를 우선순위대로 찾아 Pygame 폰트 객체를 반환한다.
- **인자 / 반환** — `size`: 폰트 크기(기본 46), `bold`: 굵게 여부 / `pygame.font.Font`(또는 폴백 `SysFont`).
- **부작용** — 없음(폰트 조회만). `pygame.font` 초기화 상태를 전제로 함.
- **의존성** — `pygame.font.match_font`/`Font`/`SysFont`.
- **⚠️ 비자명한 동작** — 우선순위 `AppleGothic`(macOS) → `Malgun Gothic`(Windows) → `NanumGothic`/`NanumBarunGothic`/`NanumSquare` 순으로 시도하고, 모두 없으면 기본 Sans(`SysFont(None, ...)`)로 폴백한다. 폴백이 걸리면 한글이 깨져 보인다(글자가 사라지거나 □로 표시).

### `ping_peers()`
- **목적** — 1초마다 A/B에 `PING`을 보내 생존성을 확인한다.
- **인자 / 반환** — 없음 / 반환 없음.
- **부작용** — 전역 `peer[A_IP]`/`peer[B_IP]`를 먼저 `False`로 초기화한 뒤 두 IP에 `PING` 송신.
- **의존성** — `send`, 전역 `peer`/`A_IP`/`B_IP`.
- **⚠️ 비자명한 동작** — 매 호출 시 `peer`를 `False`로 리셋하므로, 응답이 끊긴 피어는 `rx_loop`가 다음 PONG을 받기 전까지 "대기.." 로 보인다. 즉 표시 상태는 "지난 1초 안에 PONG을 받았는가"에 가깝다. 호출은 메인 루프의 `time.time()-t_ping>1.0` 가드로 이뤄진다.

---

## pick_coords.py

**책임** — Telescan 자동화에 필요한 화면 좌표를 운영자가 마우스를 올려 둔 위치에서 수집해 `telescan_coords.json`으로 저장하는 **일회성 셋업 스크립트**다. 함수 정의 없이 톱레벨에서 순차 실행된다.

**절차**
1. 시작 시 `print` 안내 후 `time.sleep(5)` — 5초 안에 Telescan 창으로 포커스를 옮긴다.
2. `KEYS` 리스트의 각 키에 대해: 안내 출력 → `time.sleep(3)`(해당 위치로 마우스를 옮길 시간) → `pyautogui.position()`으로 현재 커서 좌표 `(x, y)`를 읽어 `coords[key]`에 저장 → 기록 출력.
3. 모든 키를 마치면 `coords`를 `json.dumps(..., indent=2)`로 직렬화해 `telescan_coords.json`에 쓰고, 절대경로를 출력한다.

**수집하는 좌표 키(`KEYS`, 순서대로)**

| 키 | 의미 | 사용처 |
| --- | --- | --- |
| `REC_START` | 녹화 시작 버튼 | `record_on` |
| `REC_STOP` | 녹화 정지 버튼 | `record_off` |
| `ARROW_DOWN` | 폴더 경로 이동용 아래 화살표 | `record_off` 첫 저장 |
| `DESKTOP_BTN` | 바탕화면 버튼 | `record_off` 첫 저장 |
| `NEW_FOLDER_BTN` | 새 폴더 버튼 | `record_off` 첫 저장 |
| `FOLDER_NAME_BOX` | 새 폴더 이름 입력 칸 | (수집만; `record_off`는 직접 클릭 대신 키 입력으로 처리) |
| `FOLDER_DOUBLECLICK` | 폴더 진입용 더블클릭 위치 | `record_off` 첫 저장 |
| `FILENAME_BOX` | 파일명 입력 칸(2회차부터) | (수집만; 이후 저장은 바로 이름 입력) |

> ⚠️ **해상도 의존** — 좌표는 픽셀 절대값이라 EEG PC의 해상도/창 배치가 바뀌면 다시 수집해야 한다.
> ⚠️ **A의 선행 조건** — `A_eeg_client.py`가 import 시점에 `telescan_coords.json`을 읽으므로(위 A 모듈 게차 1), **A 실행 전에 반드시 이 스크립트를 먼저 돌려야** 한다. 파일이 없으면 A는 즉시 크래시한다.
> 참고: `KEYS`의 주석에 따르면 `FOLDER_NAME_BOX`/`FILENAME_BOX`는 입력 흐름상 키보드 입력 단계와 짝지어진 보조 좌표로, `record_off`의 실제 코드는 이 두 좌표를 `pos()`로 직접 클릭하지는 않는다(파일명/폴더명은 `type_korean` + Enter로 처리). 수집은 흐름 이해와 향후 확장을 위해 남겨 둔 것이다.
