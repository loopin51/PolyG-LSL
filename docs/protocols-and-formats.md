# 프로토콜 및 포맷 레퍼런스

이 문서는 3-PC EEG 실험 자동화 시스템의 **교차 참조(cross-cutting) 레퍼런스**입니다.
UDP 명령 프로토콜, `scenario.yaml` 문법, EEG 파일명 유도 규칙, CSV 로그 스키마,
그리고 별도 프로젝트(`polyg-lsl-bridge`)의 LXEM 와이어 포맷 요약을 한곳에 모았습니다.
다른 문서들은 이 파일의 각 섹션을 링크해서 사용합니다.

시스템 구성은 다음과 같습니다.

- **A — EEG PC** (`A_eeg_client.py`): Telescan 녹화 소프트웨어를 좌표 클릭으로 자동화.
- **B — Slide PC** (`B_slide_client.py`): PowerPoint 슬라이드를 화살표 키로 넘김.
- **C — Controller PC** (`C_controller.py`): 시나리오 타이머, Pygame 상태창, UDP 송신 담당(운영자 PC).

세 스크립트는 같은 서브넷의 **서로 다른 물리 머신**에서 실행되며, 모두 UDP 포트 `4210`을 사용합니다.

---

## UDP CMD 프로토콜 (port 4210)

C(컨트롤러)가 평문 ASCII 명령 문자열을 워커(A/B)로 전송합니다. 메시지 문법은
`CMD[:ARG]`이며, **첫 번째 `:`를 기준으로 한 번만 분할**합니다(`msg.split(":", 1)`).
따라서 `ARG` 안에 `:`가 더 들어 있어도 그대로 보존됩니다(예: `SUBJECT:abc:def` →
cmd=`SUBJECT`, arg=`abc:def`).

| 대상 | 명령 | 의미 | 구현 위치 |
|---|---|---|---|
| A | `SUBJECT:<id>` | 피험자 ID 갱신. 시나리오 시작 시 가장 먼저 전송됨. | `handle_udp_message` (`A_eeg_client.py`) |
| A | `REC_ON[:label]` | 녹화 시작. `label` 생략 시 `"noLabel"`. label로 trial·시나리오 인덱스 동기화. | `record_on` (`A_eeg_client.py`) |
| A | `REC_OFF` | 녹화 정지 후 파일 저장. 파일명 유도 및 `trial += 1`. | `record_off` (`A_eeg_client.py`) |
| A | `END` | 실험 종료. `raise SystemExit` → 메인 루프 종료. | `handle_udp_message` (`A_eeg_client.py`) |
| B | `NEXT` | 다음 슬라이드(`right` 키). | `next_slide` (`B_slide_client.py`) |
| B | `PREV` | 이전 슬라이드(`left` 키). | `prev_slide` (`B_slide_client.py`) |
| B | `END` | 실험 종료. 메인 `while` 루프 `break`. | 메인 루프 (`B_slide_client.py`) |
| A·B | `PING` → `PONG` | 연결 확인(liveness). C가 1초마다 `PING` 브로드캐스트, A·B는 `PONG` 응답. | A: `handle_udp_message`, B: 메인 루프 |

### Liveness (PING/PONG)

- C는 `ping_peers()`에서 1초마다 A_IP·B_IP로 `PING`을 보냅니다(`C_controller.py`).
- A·B는 `PING` 수신 시 즉시 `sock.sendto(b"PONG", addr)`로 응답합니다.
- C의 `rx_loop` 스레드가 `PONG`을 수집해 `peer[ip]` 플래그를 `True`로 만들고, Pygame UI에
  "연결됨 / 대기.." 상태로 표시합니다.

### 미구현 확장 지점 (NOT implemented)

`MARK_RESP`, `SHOW_START`, `SHOW_END`는 일부 상위 문서에 언급되어 있으나 **클라이언트 3종
(`A_eeg_client.py`, `B_slide_client.py`, `C_controller.py`) 어디에도 구현되어 있지 않습니다.**
향후 추가할 수 있는 확장 지점일 뿐, 현재 코드에는 해당 토큰이 존재하지 않습니다(grep 결과 0건으로 검증).

> 확인 방법:
> ```
> grep -n "MARK_RESP\|SHOW_START\|SHOW_END" A_eeg_client.py B_slide_client.py C_controller.py
> # 출력 없음
> ```

### 명령 추가 가이드

- A 명령 추가: `A_eeg_client.py`의 `handle_udp_message`에서 분기 처리 후 `scenario.yaml`에서
  `A:<CMD>`로 참조.
- B 명령 추가: `B_slide_client.py` 메인 루프에서 동일하게 분기 처리 후 `B:<CMD>`로 참조.

---

## scenario.yaml 문법

최상위 키는 `scenario:`이며, 그 값은 단계(step) 리스트입니다. 각 단계는
`{ name, dur, send }` 형태의 매핑입니다.

```yaml
scenario:
  - { name: "1. 고정주시(시작)", dur: 2 }
  - { name: "1. 시나리오", dur: 30, send: [A:REC_ON:scenario, B:NEXT] }
  - { name: "1. 선택지1",  dur: 15, send: [A:REC_ON:choice1, B:NEXT] }
  - { name: "1. 고정주시1", dur: 2,  send: [A:REC_OFF, B:NEXT] }
```

### `name` — 회차 + 키워드

`name`은 `"<N>. <키워드>"` 형태입니다. **앞의 숫자는 trial(회차), 나머지는 키워드**이며,
정규식 `\s*(\d+)\.\s*(.*)`로 분해됩니다(`extract_trial_and_keyword`, `A_eeg_client.py`).

- 예: `"1. 선택지1"` → trial=1, keyword=`선택지1`
- 숫자 없이 들어오면 trial=1로 가정하고 전체 문자열을 키워드로 사용합니다.

이 회차·키워드는 EEG 출력 파일명에 사용되므로 `"<N>. <키워드>"` 형태를 유지해야 합니다
(아래 "EEG 파일명 유도 규칙" 참조).

### `dur` — 지속 시간

- 초 단위(float 허용).
- C는 `time.perf_counter`로 카운트다운합니다(`scenario_worker`, `C_controller.py`).
  단계 시작 시각 `t0`을 기록하고, `dur - (perf_counter - t0) > 0` 동안 0.05초 간격으로
  잔여 시간을 갱신합니다.

### `send` — 명령 전송

- 형식: `[<A|B>:<CMD>, ...]`. 각 항목을 `:` 기준으로 한 번 분할해 target과 cmd로 나누고,
  target이 `A`이면 A_IP로, 아니면 B_IP로 전송합니다.
- **단계 시작 시점에 한 번** 발사됩니다(중간 재전송·재시도 없음).
- `send`가 없는 단계는 순수 지연(delay)입니다(예: 고정주시 십자 표시).

> 타이밍은 open-loop입니다. C가 타이밍의 단일 기준이며, A·B는 타이머 없이 각 UDP 메시지에
> 즉시 반응합니다. 명령 실행에 대한 ACK는 없고(liveness용 PING/PONG만 존재), UDP 전달은
> 재시도되지 않습니다.

---

## EEG 파일명 유도 규칙

EEG 파일명은 **전송되지 않고 A_eeg PC에서 로컬로 유도**됩니다(`record_off`, `A_eeg_client.py`).

### 파일명 포맷

```
{safe_id}_{trial:02d}회차_{safe_step}_{YYYYmmdd_HHMMSS}.eeg
```

- `safe_id` — `safe_filename(subject_id)`. 파일명 불가 문자(`\ / : * ? " < > |`)를 `_`로 치환.
- `trial:02d` — 회차를 2자리 0-패딩(예: `01회차`, `12회차`).
- `safe_step` — **직전 단계의 키워드**를 `safe_filename`으로 정제한 값.
- `YYYYmmdd_HHMMSS` — 저장 시각 타임스탬프(`datetime.now().strftime("%Y%m%d_%H%M%S")`).
- 확장자 `.eeg`.

### 직전(PREVIOUS) 단계 기준

`record_off`는 키워드를 **현재 단계가 아니라 직전 단계**에서 가져옵니다.

```python
prev_idx   = max(current_step_idx - 1, 0)
step_raw   = scenario[prev_idx]["name"]
_, step_nm = extract_trial_and_keyword(step_raw)
```

`current_step_idx`는 직전 `REC_ON`의 label로 `find_step_idx_by_label`을 통해 해석된 값입니다.
일반적으로 시나리오는 `REC_ON:<keyword>` 단계 직후의 고정주시 단계에서 `REC_OFF`를 보내므로,
한 녹화 파일은 **자신의 정지(REC_OFF)를 보낸 단계의 바로 앞 단계** 이름을 따라 명명됩니다.

### trial 동기화

- `record_on(label)`에서 label의 숫자로 `trial`을 동기화합니다
  (`trial, _ = extract_trial_and_keyword(label)`).

  다만 `scenario.yaml`의 `A:REC_ON:<keyword>` 형태 label에는 회차 숫자가 없으므로 보통
  trial=1로 해석되고, 실질적인 회차 증가는 아래 `REC_OFF`의 `+1` 누적으로 이루어집니다.
- `record_off` 마지막에 `trial += 1`로 다음 회차를 준비합니다.

### C와 A의 scenario.yaml 일치 요구

파일명은 A_eeg PC가 자신의 로컬 `scenario.yaml`을 다시 파싱해 유도합니다. 따라서 **C와 A의
`scenario.yaml`이 동일해야** 올바른 파일명이 만들어집니다. 한쪽만 바뀌면 인덱스·키워드가
어긋나 잘못된 이름으로 저장될 수 있습니다.

### 첫 회차 vs 이후 회차 저장 경로 (참고)

`record_off`는 `first_trial`일 때 Telescan 저장 대화상자 전체 절차(바탕화면 → 피험자명 새 폴더
생성 → 열기 → 파일명 입력)를 좌표 클릭으로 수행하고, 이후 회차는 파일명만 입력합니다. 이
시퀀스는 화면 좌표에 의존하므로 대화상자/레이아웃 변경에 취약합니다.

---

## CSV 로그 스키마

각 스크립트는 **역할별·날짜별** CSV 파일에 모든 TX/RX 및 동작을 append 합니다.
디렉터리는 `config.LOG_DIR`(예: `./logs`)입니다.

### 파일명 규칙

| 역할 | 파일명 | 작성 위치 |
|---|---|---|
| C (컨트롤러) | `controller_{YYYY-MM-DD}.csv` | `log` (`C_controller.py`) |
| A (EEG) | `A_eeg_{YYYY-MM-DD}.csv` | `log` (`A_eeg_client.py`) |
| B (슬라이드) | `B_slide_{YYYY-MM-DD}.csv` | `log` (`B_slide_client.py`) |

날짜는 ISO 타임스탬프의 앞 10자(`ts[:10]`)에서 얻습니다.

### 컬럼

CSV 헤더는 없으며, 각 줄은 2개 필드입니다.

```
ISO타임스탬프(ms),메시지
```

- 첫 필드: `datetime.now().isoformat(timespec="milliseconds")` (밀리초 정밀도 ISO 타임스탬프).
- 둘째 필드: 자유 형식 메시지 문자열.

> 주의: 메시지 안에 `:`가 들어가도 한 필드로 기록됩니다. CSV이지만 콤마는 타임스탬프와 메시지
> 사이 1개만 의미가 있으며, 메시지 자체에 콤마가 들어가는 경우는 현재 코드 경로에 없습니다.

### 예시 메시지

| 역할 | 메시지 예 | 의미 |
|---|---|---|
| C | `TX→<ip>:<msg>` | 해당 IP로 명령 송신 (`send`) |
| C | `ERR:<ip>:<error>` | 송신 실패 시 오류 기록 (`send`의 `OSError` 처리) |
| A | `REC_START:<label>` | 녹화 시작 (`record_on`) |
| A | `REC_SAVED:<fname>` | 녹화 파일 저장 완료, 유도된 파일명 기록 (`record_off`) |
| B | `NEXT` | 다음 슬라이드로 이동 (`next_slide`) |
| B | `PREV` | 이전 슬라이드로 이동 (`prev_slide`) |

---

## LXEM 와이어 포맷 (요약)

> 이 섹션은 **요약**입니다. 정식 정의는 별도 프로젝트 `polyg-lsl-bridge`의 설계 문서를 참조하세요:
> [`../polyg-lsl-bridge/docs/superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md`](../polyg-lsl-bridge/docs/superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md) (§5 localhost wire protocol).
>
> 참고: LXEM/LSL 브리지는 **이 3-PC Telescan 자동화 시스템과는 별개의 독립 프로젝트**이며,
> A/B/C 스크립트와 직접 연결되지 않습니다. 여기서는 교차 참조 편의를 위해 와이어 포맷만 요약합니다.

C++ 수집 앱과 Python 브리지 사이 localhost UDP 전송은 **16바이트 little-endian 헤더 +
channel-major float32 페이로드**로 구성됩니다.

### 16바이트 헤더 (little-endian)

| offset | size | 필드 | 값/설명 |
|---|---|---|---|
| 0 | u32 | `magic` | `0x4C58454D` (`'LXEM'`) |
| 4 | u16 | `version` | `1` |
| 6 | u16 | `num_channels` | 채널 수 (예: 33 = 32 + marking, 또는 17) |
| 8 | u16 | `samples_per_channel` | `512 / max_channels` (예: 16) |
| 10 | u16 | `flags` | 예약(0) |
| 12 | u32 | `seq` | 프레임 카운터, `WM_AcqUnitData`마다 +1 |
| 16 | f32[num_channels × samples_per_channel] | payload | channel-major (`ch1×N, ch2×N, …, marking×N`) |

### 요점

- 페이로드는 **channel-major** 순서로, DLL 스트림 메모리 레이아웃과 정확히 일치합니다.
  C++ 측은 DLL 버퍼에 헤더만 prepend 해서 그대로 전달합니다.
- 최대 패킷 ≈ 33×16×4 + 16 = **2128 bytes** → 루프백에서 단일 UDP 데이터그램.
- `seq`로 브리지가 드롭된 프레임을 감지합니다.
- `num_channels` / `samples_per_channel`로 모든 패킷을 config와 대조해 검증합니다.

전체 스케일링(µV 변환), LSL outlet 메타데이터, 마커 스트림, config 등 나머지 세부는 위 정식
설계 문서에 정의되어 있으며 여기서 중복하지 않습니다.
