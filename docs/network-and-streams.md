# 네트워크 & 데이터 흐름: UDP(LXEM) ↔ LSL 스트림

이 문서는 PolyG LSL 브리지 pipeline의 **네트워크·데이터 흐름**을 end-to-end 하나의 이야기로 정리한다. 다른 문서들이 따로 다루는 두 네트워크 artifact, 즉 (1) C++ 취득 앱이 내보내는 **UDP 패킷(LXEM 프레임)** 과 (2) Python 브리지가 송출하는 **LSL stream(EEG outlet + Markers outlet)** 을 한 자리에서 연결해 본다. PolyG 장비에서 raw volts 프레임이 어떻게 localhost UDP로 전달되고, Python에서 어떻게 검증·de-interleave·µV 변환을 거쳐 LSL EEG outlet으로 push되는지, 그리고 시나리오 코드가 별도의 LSL Markers outlet으로 어떻게 stimulus 마커를 내보내 LabRecorder가 두 stream을 clock-align하는지를 다룬다. (3-PC ABC Telescan 자동화 system과는 무관하다.)

```
PolyG 장비
   │ USB
   ▼
LXSM-D1WD10.dll  ──WM_AcqUnitData(float 프레임, lParam)──►  C++ OnStreamData (View)
                                                              │  (ACQPLOT 파형 그리기)
                                                              ▼
                                                         Forwarder::Send(seq, data, nch, spc)
                                                              │  16B LXEM 헤더 + raw-volts 페이로드
                                                              ▼
                                            localhost UDP  127.0.0.1:51234
                                                              │
                                                              ▼
                                  Python EEGBridge  (recvfrom 65535)
                                    parse_frame → 검증 → de-interleave
                                    → select 행 추출 → µV 변환 → transpose
                                                              │  push_chunk(local_clock())
                                                              ▼
                                              ┌──────────────────────────────┐
   시나리오 코드 ──MarkerStream.push(label)──►│  LSL Markers outlet  (cf_string)│
                                              │  LSL EEG outlet (cf_float32)    │
                                              └──────────────┬─────────────────┘
                                                             │  (둘 다 local_clock() timestamp)
                                                             ▼
                                              LabRecorder (두 outlet 동시 구독)
                                              cross-stream/cross-PC clock offset 추정 → 정렬 후 기록
```

EEG outlet과 Markers outlet은 서로 다른 PC에 있을 수 있으며, LSL이 record time에 stream 간/PC 간 clock offset을 추정해 마커를 EEG sample에 정렬한다. 각 outlet은 consumer가 없어도 송출하므로, 브리지와 recorder는 어느 순서로 켜도 된다.

---

## 1. C++ → Python UDP 패킷 (LXEM 프레임)

### Transport (host/port, 반드시 일치)

전송은 **localhost UDP**다. C++ 측은 `Test_LXSM_D1WD10_VC2017/BridgeConfig.h`의 매크로로 목적지를 고정한다: `BRIDGE_HOST "127.0.0.1"`, `BRIDGE_PORT 51234`. Python 측 `EEGBridge`는 `config.toml`의 `[transport].host` / `[transport].port`(기본 `127.0.0.1` / `51234`)로 socket을 `bind`한다. **이 두 host/port 값은 반드시 일치해야** 패킷이 도달한다.

### 16바이트 헤더 (little-endian)

`protocol.py`의 `HEADER_FORMAT = "<IHHHHI"`, `HEADER_SIZE = 16`. C++ `Forwarder::Send`가 `memcpy`로 byte-for-byte 동일하게 조립한다.

| offset | size | type | field | 값 |
|---|---|---|---|---|
| 0 | 4 | u32 | magic | `0x4C58454D` ('LXEM') |
| 4 | 2 | u16 | version | `1` |
| 6 | 2 | u16 | num_channels | 예: `33` |
| 8 | 2 | u16 | samples_per_channel | 예: `16` |
| 10 | 2 | u16 | flags | `0` (reserved) |
| 12 | 4 | u32 | seq | 프레임 카운터, `WM_AcqUnitData`당 +1 |
| 16 | … | f32[] | payload | channel-major float32 |

### 페이로드 (channel-major raw volts, marking 채널 포함)

페이로드는 `num_channels * samples_per_channel` 개의 float32이며 **CHANNEL-MAJOR**로 배열된다 (ch1×N, ch2×N, …, 마지막 marking×N). 값은 **raw VOLTS**다(µV 아님). `num_channels`에는 marking 채널이 포함된다. C++는 DLL이 넘긴 버퍼를 헤더만 앞에 붙여 **그대로(verbatim)** forward하며, µV 변환은 전혀 하지 않는다.

> ⚠️ 페이로드는 raw volts다. µV 변환은 Python 측에서만 일어난다(§2 참조). C++는 장치 raw 프레임의 중계만 담당한다.

### 패킷 크기 + 2128바이트 예시

패킷 크기 = `16 + num_channels * samples_per_channel * 4` 바이트.

PolyG-A 기본 config 예시: `num_channels = 33`(아날로그 32 + marking 1), `samples_per_channel = sample_freq / max_channels = 512 / 32 = 16`(여기서 `512`는 `sample_freq`). 따라서 패킷 크기 = `16 + 33 * 16 * 4 = ` **2128 바이트**이며, loopback에서 datagram 하나로 전송된다.

### 검증 (magic/version/length + config mismatch skip)

Python `parse_header` / `parse_frame`가 다음을 거부한다(`FrameError` 발생 → 브리지가 logging 후 skip):

- **bad magic**: `magic != 0x4C58454D`.
- **unsupported version**: `version != 1`.
- **wrong total length**: `len(buf) != HEADER_SIZE + num_channels * samples_per_channel * 4`.

추가로 `EEGBridge.handle_datagram`은 헤더의 `num_channels` / `samples_per_channel`이 config의 기대값과 다르면 거부한다:

- `expected_num_channels = max_channels + 1` (device가 marking 채널을 append).
- `expected_samples_per_channel = 512 // max_channels`.

불일치 시 mismatch를 logging하고 해당 프레임을 skip한다(crash 없음).

### seq / drop 감지

`SeqTracker`가 dropped-frame 수를 `(seq - last - 1) & 0xFFFFFFFF`로 계산한다(u32 wrap-safe). 값이 0보다 크면 warning을 남긴다. UDP 손실은 허용되며 절대 crash로 이어지지 않는다.

C++ `Forwarder::Send(seq, data, num_channels, samples_per_channel)`은 위 헤더(magic/version/nch/spc/flags=0/seq를 `memcpy`)와 페이로드를 정확히 이 레이아웃대로 조립해 `sendto`한다. 헤더 byte 배치는 `protocol.py`와 byte-for-byte 일치한다.

---

## 2. Python 처리 파이프라인 (UDP → µV)

`EEGBridge.run`이 `recvfrom(65535)`로 datagram을 수신한 뒤(timeout 0.5s, timeout 시 continue), `handle_datagram`에서 다음 순서로 처리한다.

1. **parse_frame**: §1의 frame 검증(magic/version/length)을 거친 뒤 페이로드를 `np.frombuffer(dtype="<f4")` → `reshape(num_channels, samples_per_channel)`로 **de-interleave**한다. 결과는 channel-major `(num_channels, samples_per_channel)` 배열.
2. **config mismatch 검사**: 헤더의 `num_channels` / `samples_per_channel`이 config 기대값(`expected_num_channels` / `expected_samples_per_channel`)과 다르면 skip(§1 참조). frame 검증과 별개의 단계다.
3. **select 행 추출**: `frame_to_microvolts`가 `select_zero_based`(1-based config `select`를 0-based로 변환) 행만 골라낸다 → `(n_selected, samples_per_channel)`.
4. **µV 변환** (`scaling.py`): `µV = raw_volts / (fixed_gain × pga_gain) × 1e6`.
   - `pga_gain`은 `GAIN_TABLE[gain_idx]`에서 온다(예: `gain_idx 9 → 4.25`).
   - `fixed_gain`은 사용자 제공값(config `[scale].fixed_gain`, 기본값 `1.0`은 placeholder).
5. **transpose**: `frame_to_microvolts`는 `uv.T`를 반환해 shape `(samples_per_channel, n_selected)`(samples × channels) float32, C-contiguous로 만든다.

이 `(samples, n_selected)` chunk가 §3의 LSL EEG outlet으로 push된다.

---

## 3. LSL EEG outlet

`build_stream_info(cfg)`가 만드는 `StreamInfo`:

| field | 값 |
|---|---|
| name | `PolyG_{model}` (예: `PolyG_PolyG-A`) |
| type | `"EEG"` |
| channel_count | `len(select)` (예: 8) |
| nominal_srate | `sample_freq` (예: 512) |
| channel_format | `cf_float32` |
| source_id | `polyg-{model}-{port}` (예: `polyg-PolyG-A-51234`) |

### desc() 메타데이터 트리

- `channels` → label마다 `channel` 하나. 각 `channel`의 child value: `label`(config의 labels), `unit` = `"microvolts"`, `type` = `"EEG"`.
- `device` → child value: `model`, `sample_freq`, `max_channels`, `gain_idx`, `pga_gain`, `fixed_gain`. 이는 downstream에서 µV 값을 audit할 수 있도록 전체 provenance(게인 출처)를 기록한다.

### push 및 timestamp

push는 `outlet.push_chunk(chunk.tolist(), local_clock())`. chunk 전체에 timestamp 하나만 주지만, `nominal_srate`가 설정되어 있어 LSL이 그 하나의 chunk timestamp로부터 per-sample timestamp를 back-fill한다.

> marking 채널은 EEG outlet에 포함되지 않는다(config의 `select` 인덱스만 전송). 마커는 §4의 별도 Markers stream으로 나간다.

---

## 4. LSL Markers outlet (시나리오 마커)

`markers.py`의 `MarkerStream`이 stimulus-onset 이벤트용 단일 채널 string outlet을 소유한다. 생성자: `MarkerStream(name, source_id, *, stream_type="Markers")`.

| field | 값 |
|---|---|
| name | 생성자 인자 |
| type | `"Markers"` (default) |
| channel_count | `1` |
| nominal_srate | `IRREGULAR_RATE` |
| channel_format | `cf_string` |
| source_id | 생성자 인자 |

`push(label, timestamp=None)` → `outlet.push_sample([label], local_clock() if timestamp is None else timestamp)`. timestamp는 **실제 stimulus 호출 시점에 가능한 한 가깝게** 찍어야 한다.

### 별도 outlet인 이유

마커는 불규칙(irregular)하게 발생하는 string 이벤트라 EEG의 규칙적 float32 stream과 rate·format이 다르다. 그래서 EEG outlet(고정 srate, float32)과 분리된 독립 outlet으로 둔다. 또한 EEG와 마커 outlet은 서로 다른 PC에서 송출될 수 있는데, LSL이 record time에 둘을 clock-align하므로(§5) 분리해도 시간 정렬이 보존된다.

---

## 5. 클럭 동기화 & 레코더

두 outlet 모두 `local_clock()`으로 timestamp를 찍는다. EEG outlet과 Markers outlet은 서로 다른 PC에 있을 수 있으나, LSL이 record time에 cross-stream / cross-PC clock offset을 추정해(LabRecorder가 기본으로 수행) 마커를 EEG sample에 정렬한다.

각 outlet은 consumer가 없어도 계속 송출한다. 따라서 브리지와 recorder(LabRecorder)는 **어느 순서로 시작해도 무방**하다. LabRecorder는 EEG outlet과 Markers outlet을 **동시에 구독**해 한 파일에 정렬·기록한다.

---

## 6. 포트·게인 일치 체크리스트

C++ `BridgeConfig.h` ↔ Python `config.toml`은 다음 값이 반드시 일치해야 한다.

| 항목 | C++ `BridgeConfig.h` | Python `config.toml` | 영향 |
|---|---|---|---|
| host | `BRIDGE_HOST "127.0.0.1"` | `[transport].host` | 다르면 패킷 미도달 |
| port | `BRIDGE_PORT 51234` | `[transport].port` | 다르면 패킷 미도달 |
| 게인 인덱스 | `BRIDGE_PGA_GAIN_IDX 9` | `[device].gain_idx` | 다르면 µV 스케일 오류 |

> ⚠️ `gain_idx`가 **반드시 일치**해야 한다. C++ `Set_PGA(BRIDGE_PGA_GAIN_IDX)`로 장치에 설정한 게인과 Python µV 변환에 쓰이는 `pga_gain = GAIN_TABLE[gain_idx]`가 어긋나면, 출력 µV 값이 틀린다. (예: 둘 다 `9` → `pga_gain 4.25`.)

### 무음(silent) 스트림 디버깅

EEG outlet이 잡히는데 sample이 흐르지 않거나, 브리지가 조용할 때 점검 순서:

1. **패킷이 도착하는가** — host/port 불일치면 패킷이 아예 안 온다(§1, 위 표). C++ 앱에서 `Init Device` → 샘플링 주파수 메뉴 선택 → `Start Stream` 순서를 지켰는지 확인.
2. **브리지 로그 확인** — 프레임이 와도 거부되면 브리지가 `logging`으로 남긴다. `bad frame`(magic/version/length) 또는 `frame/config mismatch`(§1) 줄이 보이면 해당 프레임이 모두 skip된 것이다 → C++ `DISPMAXCH`/샘플링 설정과 `config.toml`의 `max_channels`/`sample_freq_idx`가 어긋난 경우가 흔하다.
3. **`dropped N frame(s)` 경고** — `seq` gap 경고(§4의 `SeqTracker`)가 많으면 UDP 손실이 크다는 신호다(보통 부하·MTU 문제).
4. **µV 값이 이상** — 스트림은 흐르는데 스케일이 틀리면 위 `gain_idx` 불일치 또는 `fixed_gain` 미설정(기본 `1.0` placeholder)을 의심.

### 관련 문서

- [cpp-acquisition-app.md](./cpp-acquisition-app.md) — C++ 취득 앱(DLL API·핸들러·Forwarder)
- [polyg-lsl-bridge.md](./polyg-lsl-bridge.md) — LSL 브리지 모듈 요약
- [protocols-and-formats.md](./protocols-and-formats.md) — UDP/scenario/파일명/로그/LXEM 교차 참조
- Bridge README: [`../polyg-lsl-bridge/README.md`](../polyg-lsl-bridge/README.md)
