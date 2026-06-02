# polyg_lsl API Reference

이 문서는 `polyg_lsl` 패키지의 함수/클래스/상수 단위 reference이다. PolyG EEG 장비가 localhost UDP로 송신하는 LXEM wire frame을 파싱하여 µV 단위로 스케일링하고 LSL(Lab Streaming Layer) outlet으로 push하는 bridge가 핵심 기능이다. 설치/실행 방법은 [../README.md](../README.md)를, 설계 배경과 의사결정은 `docs/superpowers/specs/`를 참고한다. 아래는 module별로 public한 constant / dataclass / function / class를 정리한 것이며, 모든 signature는 source와 verbatim으로 일치한다.

## 모듈 색인

- [`__init__.py`](#initpy) — 패키지 버전
- [`protocol.py`](#protocolpy) — LXEM wire 포맷 (상수·`FrameHeader`·`parse_*`/`build_frame`)
- [`scaling.py`](#scalingpy) — raw volts → µV 변환
- [`config.py`](#configpy) — `config.toml` 로드·검증 (`Config`/`load_config`)
- [`bridge.py`](#bridgepy) — 프레임→µV→LSL EEG outlet (`EEGBridge`/`build_stream_info`/`SeqTracker`)
- [`markers.py`](#markerspy) — 시나리오 마커 LSL outlet (`MarkerStream`)
- [`fake_device.py`](#fake_devicepy) — 합성 LXEM 프레임 소스 (테스트/오프라인)

> 앵커는 GitHub식 slug 기준이다. 일부 Markdown 뷰어는 헤딩 내 백틱/마침표 처리가 달라 점프가 안 될 수 있으나, 각 모듈은 `## <파일명>` 헤딩으로 찾을 수 있다.

---

## `__init__.py`

패키지 진입점. 버전 문자열만 노출한다.

### `__version__ = "0.1.0"`

- **목적**: 패키지 버전 문자열. `polyg_lsl.__version__`로 접근한다.

---

## `protocol.py`

LXEM localhost wire format을 정의하는 module. **순수 byte 로직만** 담으며 socket I/O는 없다. frame은 16-byte little-endian header + channel-major `float32` payload으로 구성된다. 장비 매뉴얼의 gain/device/frequency table도 여기에 상수로 박혀 있다.

### `MAGIC = 0x4C58454D`

- **목적**: frame 식별용 magic number. ASCII `'LXEM'`을 little-endian으로 해석한 값이다. header의 첫 u32 필드와 일치해야 valid frame으로 인정된다.

### `VERSION = 1`

- **목적**: 지원하는 wire protocol 버전. header version 필드가 이 값과 다르면 `parse_header`가 `FrameError`를 raise한다.

### `HEADER_FORMAT = "<IHHHHI"`

- **목적**: `struct` 포맷 문자열. little-endian으로 `magic u32, version u16, num_channels u16, samples_per_channel u16, flags u16, seq u32` 순서를 정의한다.

### `HEADER_SIZE = struct.calcsize(HEADER_FORMAT)`

- **목적**: header의 byte 크기. `HEADER_FORMAT` 기준으로 계산되며 값은 `16`이다.

### `GAIN_TABLE`

- **목적**: `gain_idx`(0..15) → PGA voltage gain 매핑 dict. 매뉴얼 Table-5를 그대로 옮긴 값이다 (예: `0 → 0.1`, `4 → 1.0`, `15 → 17.0`).
- **⚠️ 비자명한 동작**: `config.load_config`가 `pga_gain = GAIN_TABLE[gain_idx]`로 사용한다. key는 정확히 0..15만 존재하므로 그 외 index는 `KeyError`를 유발한다(단, `load_config`는 사전에 0..15 범위를 검증한다).

### `DEVICE_IDS = {"PolyG-A": 14, "PolyG-E": 15, "PolyG-I": 16, "PolyG-U": 17}`

- **목적**: model 이름 → USB device id 매핑 dict (매뉴얼 Table-4). `load_config`에서 model 유효성 검사 및 `device_id` 도출에 사용된다.

### `DEVICE_MAX_CHANNELS = {"PolyG-A": 32, "PolyG-I": 16, "PolyG-U": 8}`

- **목적**: model → 최대 analog 채널 수 매핑 dict (매뉴얼 `Set_ADCMaxNumChannel`).
- **⚠️ 비자명한 동작**: `"PolyG-E"`는 이 dict에 **없다**. `load_config`는 `.get(model, max_channels)`로 조회하므로, PolyG-E의 경우 device cap이 설정상의 `max_channels`로 fallback된다.

### `MAX_SAMPLE_FREQ_BY_CHANNELS = {2: 4096, 4: 2048, 8: 1024, 16: 512, 32: 512}`

- **목적**: `max_channels` → 허용 최고 sample frequency(Hz) 매핑 dict (매뉴얼 Table-2). `load_config`가 설정된 `sample_freq`가 이 상한을 넘는지 검증한다.

### `class FrameError(Exception)`

- **목적**: 수신 buffer가 valid LXEM frame이 아닐 때 raise되는 exception. `parse_header` / `parse_frame`이 사용한다.

### `@dataclass(frozen=True) class FrameHeader`

- **목적**: 파싱된 frame header를 담는 immutable dataclass.
- **필드**: `magic: int`, `version: int`, `num_channels: int`, `samples_per_channel: int`, `flags: int`, `seq: int`.

### `parse_header(buf: bytes) -> FrameHeader`

- **목적**: buffer 앞 16 byte를 unpack하여 `FrameHeader`를 반환한다. magic/version 유효성도 검사한다.
- **인자 / 반환**: `buf` (raw bytes) → `FrameHeader`.
- **부작용 / 예외**: 순수 함수(I/O 없음). 다음 경우 `FrameError`를 raise한다 — buffer 길이가 `HEADER_SIZE`(16) 미만, magic이 `MAGIC`와 불일치, version이 `VERSION`과 불일치.

### `parse_frame(buf: bytes) -> tuple[FrameHeader, np.ndarray]`

- **목적**: header를 파싱하고 뒤따르는 payload를 channel-major `(num_channels, samples_per_channel)` 배열로 복원한다.
- **인자 / 반환**: `buf` → `(FrameHeader, np.ndarray)` tuple. 배열 dtype은 `"<f4"`(little-endian float32), shape은 `(header.num_channels, header.samples_per_channel)`.
- **부작용 / 예외**: 순수 함수. `parse_header`가 던지는 `FrameError`를 그대로 전파하며, 추가로 전체 길이가 `HEADER_SIZE + num_channels * samples_per_channel * 4`와 정확히 같지 않으면 `FrameError`를 raise한다.
- **⚠️ 비자명한 동작**: `np.frombuffer`로 복원하므로 반환 배열은 입력 `buf`의 메모리를 공유하는 read-only view이다. 쓰기가 필요하면 호출 측에서 copy해야 한다.

### `build_frame(num_channels: int, samples_per_channel: int, seq: int, data: np.ndarray, flags: int = 0) -> bytes`

- **목적**: channel-major 배열 `data`를 LXEM frame bytes로 직렬화한다 (header + payload). 주로 `fake_device`가 사용한다.
- **인자 / 반환**: `num_channels`, `samples_per_channel`, `seq`, `data` (channel-major), `flags`(기본 0) → frame `bytes`.
- **부작용 / 예외**: 순수 함수. `data`를 `"<f4"` C-contiguous로 변환한 뒤 shape이 `(num_channels, samples_per_channel)`가 아니면 `ValueError`를 raise한다.

---

## `scaling.py`

raw ADC 전압을 물리 단위 µV로 변환하는 순수 module.

### `raw_to_microvolts(volts: np.ndarray, fixed_gain: float, pga_gain: float) -> np.ndarray`

- **목적**: ADC raw volts를 µV로 환산한다. 공식은 `µV = volts / (fixed_gain * pga_gain) * 1e6`이다.
- **인자 / 반환**: `volts` (입력 범위는 docstring 기준 -1.25..+1.25 V), `fixed_gain`(사용자 지정 front-end amp gain), `pga_gain`(device `gain_idx`에서 온 PGA gain, 매뉴얼 Table-5) → µV 배열.
- **⚠️ 비자명한 동작**: 입력을 `np.float64`로 강제 변환한 뒤 계산하므로 반환 dtype은 `float64`이다. 순수 함수(부작용 없음).

---

## `config.py`

공유 `config.toml`을 읽어 검증한 뒤 immutable `Config` dataclass로 만드는 module. 모든 검증 실패는 `ConfigError`로 표면화된다.

### `LEGAL_MAX_CHANNELS = (2, 4, 8, 16, 32)`

- **목적**: 허용되는 `max_channels` 값 tuple. `load_config`가 설정값이 이 안에 드는지 검사한다.

### `class ConfigError(Exception)`

- **목적**: `config.toml`에 필드가 빠졌거나 illegal한 값이 있을 때 raise되는 exception.

### `@dataclass(frozen=True) class Config`

- **목적**: 검증을 통과한 설정 전체를 담는 immutable dataclass.
- **필드**: `model: str`, `device_id: int`, `max_channels: int`, `sample_freq_idx: int`, `sample_freq: int`, `gain_idx: int`, `pga_gain: float`, `fixed_gain: float`, `select: tuple[int, ...]`, `labels: tuple[str, ...]`, `host: str`, `port: int`.

#### `Config.expected_num_channels` (property) `-> int`

- **목적**: 장비가 보내는 frame의 채널 수. `max_channels + 1`이며 `+1`은 장비가 덧붙이는 marking 채널이다.
- **부작용 / 예외**: 순수 property.

#### `Config.expected_samples_per_channel` (property) `-> int`

- **목적**: frame당 채널별 sample 수. `512 // max_channels`로 계산된다.
- **부작용 / 예외**: 순수 property.

#### `Config.select_zero_based` (property) `-> tuple[int, ...]`

- **목적**: 1-based인 `select`를 0-based index tuple로 변환한다 (`i - 1`).
- **부작용 / 예외**: 순수 property. `frame_to_microvolts`의 row 선택 index로 쓰인다.

### `load_config(path: str | os.PathLike) -> Config`

- **목적**: TOML 파일을 `tomllib`로 읽어 모든 필드를 검증/도출한 뒤 `Config`를 반환한다.
- **인자 / 반환**: `path` (config.toml 경로) → `Config`.
- **부작용 / 예외**: 파일을 binary로 open하여 읽는다(I/O). 아래 검증 규칙 위반 시 `ConfigError`를 raise한다.
  - **missing key**: `device.model`, `device.max_channels`, `device.sample_freq_idx`, `device.gain_idx`, `scale.fixed_gain`, `channels.select`, `channels.labels`, `transport.host`, `transport.port` 중 하나라도 없으면 `KeyError`를 잡아 `ConfigError("missing config key: ...")`로 변환(`from e`).
  - **unknown model**: `model`이 `DEVICE_IDS`에 없으면 `ConfigError`.
  - **max_channels 위반**: `max_channels`가 `LEGAL_MAX_CHANNELS`에 없으면 `ConfigError`.
  - **sample_freq_idx 범위**: `0 <= sample_freq_idx <= 14`가 아니면 `ConfigError`.
  - **gain_idx 범위**: `0 <= gain_idx <= 15`가 아니면 `ConfigError`.
  - **labels/select 길이 불일치**: `len(labels) != len(select)`이면 `ConfigError`.
  - **sample_freq 상한 초과**: `sample_freq = 2 ** sample_freq_idx`가 `MAX_SAMPLE_FREQ_BY_CHANNELS[max_channels]`를 초과하면 `ConfigError`.
  - **select index 범위**: 각 `select` 원소 `i`가 `1 <= i <= min(max_channels, DEVICE_MAX_CHANNELS.get(model, max_channels))`를 벗어나면 `ConfigError`.
- **⚠️ 비자명한 동작**: 파일에서 직접 읽는 값은 `model / max_channels / sample_freq_idx / gain_idx / fixed_gain / select / labels / host / port`뿐이다. `device_id`(`DEVICE_IDS[model]`), `sample_freq`(`2 ** sample_freq_idx`), `pga_gain`(`GAIN_TABLE[gain_idx]`)는 **파일이 아니라 코드에서 도출**된다. `select`/`labels`는 tuple로, `fixed_gain`은 `float()`로 변환되어 저장된다. PolyG-E는 `DEVICE_MAX_CHANNELS`에 없어 device cap이 `max_channels`로 fallback된다.

---

## `bridge.py`

수신한 LXEM datagram을 µV로 변환해 LSL EEG outlet으로 push하는 핵심 module. frame transform(순수)과 LSL/socket I/O가 한 파일에 공존하며, `pylsl` import는 함수 내부로 미뤄져 있다.

### `frame_to_microvolts(data: np.ndarray, select_zero_based, fixed_gain: float, pga_gain: float) -> np.ndarray`

- **목적**: channel-major volts 배열에서 선택 채널 row만 추려 µV로 스케일링하고, LSL이 기대하는 sample-major 레이아웃으로 transpose한다.
- **인자 / 반환**: `data` (`(num_channels, samples)` channel-major volts), `select_zero_based` (선택할 0-based row index sequence), `fixed_gain`, `pga_gain` → `(samples, n_selected)` `float32` 배열.
- **부작용 / 예외**: 순수 함수. 내부적으로 `raw_to_microvolts`를 호출한다.
- **⚠️ 비자명한 동작**: 반환 배열은 transpose 후 `np.ascontiguousarray(..., dtype=np.float32)`를 거치므로 **C-contiguous float32**가 보장된다. `select_zero_based`는 `list(...)`로 감싸 fancy-indexing되므로 tuple/list 등 sequence면 된다.

### `class SeqTracker`

frame seq counter로부터 drop된 frame 수를 세는 helper.

#### `SeqTracker.__init__(self) -> None`

- **목적**: 내부 last-seq 상태를 `None`으로 초기화한다.
- **부작용 / 예외**: 인스턴스 상태 `_last` 설정.

#### `SeqTracker.update(self, seq: int) -> int`

- **목적**: 직전 seq 대비 누락된 frame 수를 반환하고 last-seq를 갱신한다.
- **인자 / 반환**: `seq` (이번 frame의 seq) → dropped count `int`.
- **부작용 / 예외**: 인스턴스의 `_last`를 갱신(상태 변경).
- **⚠️ 비자명한 동작**: 첫 호출은 항상 `0`을 반환한다(기준점 설정). 이후엔 `(seq - last - 1) & 0xFFFFFFFF`로 계산하므로 u32 wrap-around를 올바르게 처리한다. seq가 역행해도 mask 때문에 큰 양수가 나올 수 있다.

### `build_stream_info(cfg: Config)`

- **목적**: `Config`로부터 LSL `StreamInfo`를 구성한다. 채널 메타데이터와 device 메타데이터를 `desc()`에 채운다.
- **인자 / 반환**: `cfg` → `pylsl.StreamInfo`.
- **반환 StreamInfo 속성**: `name=f"PolyG_{cfg.model}"`, `type="EEG"`, `channel_count=len(cfg.select)`, `nominal_srate=cfg.sample_freq`, `channel_format=cf_float32`, `source_id=f"polyg-{cfg.model}-{cfg.port}"`. `desc()` 아래에 채널별 `label`(= `cfg.labels`의 각 항목) / `unit="microvolts"` / `type="EEG"`를 추가하고, device child에 `model / sample_freq / max_channels / gain_idx / pga_gain / fixed_gain`(모두 str로 변환)을 추가한다.
- **부작용 / 예외**: 순수 계산이나 `pylsl` 객체를 생성한다.
- **⚠️ 비자명한 동작**: `pylsl`(`StreamInfo`, `cf_float32`)을 **함수 내부에서 lazy import**한다. 덕분에 `protocol`/`scaling`/`config` 같은 순수 module은 liblsl 설치 없이도 import된다. 채널 메타는 `cfg.labels`를 순회하므로 `labels` 길이와 `select` 길이는 일치해야 한다(`load_config`가 강제).

### `class EEGBridge`

LSL EEG outlet을 소유하고, 수신 datagram을 push chunk로 변환하는 클래스.

#### `EEGBridge.__init__(self, cfg: Config) -> None`

- **목적**: `cfg`를 저장하고 `build_stream_info(cfg)`로 만든 `StreamOutlet`을 생성하며 `SeqTracker`를 초기화한다.
- **인자 / 반환**: `cfg` → 없음.
- **부작용 / 예외**: `pylsl.StreamOutlet`을 lazy import 후 생성(네트워크/LSL 자원 점유). 인스턴스에 `cfg`, `outlet`, `_seq` 설정.

#### `EEGBridge.handle_datagram(self, buf: bytes) -> int | None`

- **목적**: 한 개의 raw datagram을 파싱·검증·스케일링하여 outlet으로 push한다.
- **인자 / 반환**: `buf` → dropped-frame count `int`, 또는 frame이 거부되면 `None`.
- **부작용 / 예외**: 정상 경로에서 `outlet.push_chunk(chunk.tolist(), local_clock())`를 호출(LSL push). 로그를 남긴다. `FrameError`는 내부에서 잡아 `None`을 반환하므로 호출자에게 전파되지 않는다.
- **⚠️ 비자명한 동작**: 두 가지 거부 사유로 `None`을 반환한다 — (1) `parse_frame`이 `FrameError`를 던질 때(`log.warning` 후), (2) frame의 `num_channels`/`samples_per_channel`이 `cfg.expected_num_channels`/`expected_samples_per_channel`과 불일치할 때(`log.error` 후 skip). drop이 감지되면 `log.warning`을 남기지만 push 자체는 계속 진행한다. `local_clock` 역시 함수 내부에서 lazy import된다. push timestamp로는 chunk 전체에 단일 `local_clock()` 값을 부여한다.

#### `EEGBridge.run(self, *, stop=None) -> None`

- **목적**: UDP socket을 열어 datagram을 받아 `handle_datagram`으로 넘기는 수신 루프를 돈다.
- **인자 / 반환**: keyword-only `stop` (호출 가능한 종료 조건, 기본 `None`) → 없음.
- **부작용 / 예외**: `AF_INET`/`SOCK_DGRAM` socket을 `cfg.host:cfg.port`에 bind하고 `recvfrom(65535)`로 수신한다(네트워크 I/O). `finally`에서 socket을 `close()`한다.
- **⚠️ 비자명한 동작**: socket timeout은 `0.5`초로 설정되어, `socket.timeout` 발생 시 `continue`로 루프를 돌며 `stop()` 조건을 재확인한다. `stop`이 `None`이면 무한 루프이다. 종료는 보통 `main`에서 `KeyboardInterrupt`로 이뤄진다.

### `main() -> None`

- **목적**: CLI 진입점. `--config`(기본 `config.toml`)를 파싱해 config를 로드하고 `EEGBridge`를 실행한다. console-script `polyg-bridge`로 노출된다.
- **인자 / 반환**: 없음(argparse가 `sys.argv` 사용) → 없음.
- **부작용 / 예외**: `argparse`로 CLI 파싱, `logging.basicConfig`로 INFO 레벨 로깅 설정, `load_config` 호출(파일 I/O 및 `ConfigError` 가능), `EEGBridge(cfg).run()` 실행(네트워크 I/O). `KeyboardInterrupt`는 잡아서 `"stopped"` 로그를 남기고 정상 종료한다.

---

## `markers.py`

scenario/실험 측에서 stimulus-onset 같은 event를 LSL Markers stream으로 내보내는 module. `protocol`/`config`와 달리 `pylsl`을 module 최상단에서 import한다.

### `class MarkerStream`

stimulus-onset event용 single-channel string Markers outlet.

#### `MarkerStream.__init__(self, name: str, source_id: str, *, stream_type: str = "Markers") -> None`

- **목적**: Markers용 `StreamInfo`와 `StreamOutlet`을 생성한다.
- **인자 / 반환**: `name` (stream 이름), `source_id` (LSL source id), keyword-only `stream_type` (기본 `"Markers"`) → 없음.
- **부작용 / 예외**: `pylsl.StreamOutlet`을 생성(LSL 자원 점유). 인스턴스에 `outlet` 설정.
- **⚠️ 비자명한 동작**: `StreamInfo`는 `type=stream_type`, `channel_count=1`, `nominal_srate=IRREGULAR_RATE`, `channel_format=cf_string`으로 고정 구성된다. 즉 비정규(irregular) sampling rate의 1채널 문자열 stream이다.

#### `MarkerStream.push(self, label: str, timestamp: float | None = None) -> None`

- **목적**: 단일 marker label을 outlet으로 내보낸다.
- **인자 / 반환**: `label` (marker 문자열), `timestamp` (LSL timestamp, 기본 `None`) → 없음.
- **부작용 / 예외**: `outlet.push_sample([label], ts)`를 호출(LSL push).
- **⚠️ 비자명한 동작**: `timestamp`가 `None`이면 `local_clock()`을 사용하고, 아니면 주어진 값을 그대로 사용한다. label은 list `[label]`로 감싸 1채널 sample로 push된다.

---

## `fake_device.py`

테스트/오프라인 개발용 synthetic LXEM frame source. 실제 PolyG 장비 없이 bridge를 구동·검증할 수 있다.

### `generate_frame(seq: int, num_channels: int, samples_per_channel: int, *, sample_freq: float = 512.0, amplitude: float = 1.0) -> bytes`

- **목적**: 데이터 채널마다 고유한 sine파를 담은 한 frame을 생성한다.
- **인자 / 반환**: `seq`, `num_channels`, `samples_per_channel`, keyword-only `sample_freq`(기본 `512.0`), `amplitude`(기본 `1.0`) → LXEM frame `bytes`.
- **부작용 / 예외**: 순수 함수. 내부에서 `build_frame`을 호출하므로 shape 불일치 시 `ValueError` 가능(여기서는 항상 맞게 구성).
- **⚠️ 비자명한 동작**: 시간축 `t`는 `(seq * n + arange(n)) / sample_freq`로 frame 간 연속성을 유지한다. data 채널 `ch`(0..num_channels-2)는 `freq = 5.0 + ch`의 sine을 갖고, **마지막(marking) 채널은 1.0으로 고정**된다(스위치 OFF idle = 1).

### `run(cfg, *, duration: float | None = None) -> None`

- **목적**: `cfg.host:cfg.port`로 합성 frame을 주기적으로 송신한다.
- **인자 / 반환**: `cfg` (`Config`), keyword-only `duration` (초, 기본 `None`=무한) → 없음.
- **부작용 / 예외**: UDP socket을 생성해 `sendto`로 frame을 보낸다(네트워크 I/O). `finally`에서 socket을 `close()`한다.
- **⚠️ 비자명한 동작**: 채널 수/샘플 수는 `cfg.expected_num_channels` / `cfg.expected_samples_per_channel`에서 가져온다. 송신 간격은 `interval = spc / cfg.sample_freq`초이며 매 frame 후 `time.sleep(interval)`한다(`time.perf_counter`로 duration 측정). seq는 `(seq + 1) & 0xFFFFFFFF`로 u32 wrap-around된다.

### `main() -> None`

- **목적**: CLI 진입점. `--config`(기본 `config.toml`)와 `--duration`(초, 기본 무한)을 파싱해 `run`을 실행한다. console-script `polyg-fake-device`로 노출된다.
- **인자 / 반환**: 없음 → 없음.
- **부작용 / 예외**: `argparse` 파싱, `logging.basicConfig`로 INFO 로깅 설정, `load_config` 호출(파일 I/O 및 `ConfigError` 가능), `run(cfg, duration=...)` 실행(네트워크 I/O). `KeyboardInterrupt`를 잡아 `"stopped"` 로그 후 정상 종료한다.
