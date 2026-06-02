# C++ MFC 수집 애플리케이션 (`PolyG_DLL_API`)

제조사가 제공한 C++ MFC 수집 애플리케이션은 PolyG → Python LSL 브리지의 **device-side front-end**다.
제조사 DLL `LXSM-D1WD10.dll`이 float 프레임 포인터를 담아 앱의 HWND로
`WM_AcqUnitData`(= `WM_USER+1`) 메시지를 post하면, View 클래스의 `OnStreamData` 핸들러가 이를 받아
(1) ACQPLOT DLL로 파형을 그리고, (2) `Forwarder` 클래스를 통해 원본 프레임을 localhost UDP로
Python 브리지에 그대로 forward한다. 즉 이 앱은 장치 제어(초기화/샘플링/게인 설정)와
실시간 프레임 중계만 담당하며, µV 스케일링·LSL 송출은 모두 Python 측에서 처리한다.

빌드/편집 절차는 별도 문서에 이미 정리되어 있으므로 여기서 반복하지 않는다. 아래를 참조한다.

- 빌드 절차 (Visual Studio 2017): [`../PolyG_DLL_API/BUILD_ko.md`](../PolyG_DLL_API/BUILD_ko.md)
- 브리지 측 C++ 연동 개요: [`../polyg-lsl-bridge/cpp/README.md`](../polyg-lsl-bridge/cpp/README.md)

> ⚠️ 소스 파일은 **CP949** 인코딩이다. 직접 열람할 때는
> `iconv -f CP949 -t UTF-8 <file>`로 디코드해야 한글 주석이 깨지지 않는다.

---

## DLL API (`LXSM-D1WD10.h`)

DLL을 사용하려면 이 헤더를 메인 프로그램에 포함해야 한다(DLL version 1.0, 2005-05-10 릴리스).
헤더는 메시지 매크로 1개와 가져오는(import) 함수 9개를 선언한다. 모든 함수는
`extern "C" __declspec(dllimport) short` 시그니처를 갖는다(반환형 `short`).

### 메시지 매크로

```cpp
#define WM_AcqUnitData	WM_USER+1
```

`Start_Stream()` 실행 후 DLL이 채널당 지정 샘플 수의 데이터를 확보할 때마다 이 메시지를
`Init_Device`에 전달한 HWND로 post한다. 메시지의 `lParam`에 AD 변환된 float 배열 포인터가 실린다.

### 가져오는 함수 (9개)

| 함수 | 시그니처 | 선행 조건 / 비고 |
|------|----------|------------------|
| `Init_Device` | `short Init_Device(HWND msgtarget_window, int pid)` | H/W 장치 초기화. `msgtarget_window`는 stream 메시지를 받을 윈도우 핸들, `pid`는 장치 고유 ID(**PolyG-A = 14, PolyG-I = 16**). |
| `Close_Device` | `short Close_Device()` | 장치 해제. |
| `Start_Stream` | `short Start_Stream()` | Stream 시작. **선행 `Init_Device` 필요, 단 한 번만 호출.** |
| `Stop_Stream` | `short Stop_Stream()` | Stream 종료. 선행 `Init_Device`, `Start_Stream` 필요. |
| `Set_SampleFreq` | `short Set_SampleFreq(unsigned char samplefreq_idx)` | Sampling Frequency 설정. `2^idx` Hz가 되도록 `idx` 전달(예: 256 Hz → `idx = 8`). |
| `Set_PGA` | `short Set_PGA(unsigned char gain_idx)` | 전 채널 동일 게인 설정. |
| `Set_PGA_SourceGroup` | `short Set_PGA_SourceGroup(unsigned char sourcegroup_idx, unsigned char gain_idx)` | 측정 대상(소스 그룹)별 PGA 인덱스 설정. |
| `Set_PGA_EachChannel` | `short Set_PGA_EachChannel(unsigned char channel_idx, unsigned char gain_idx)` | 채널 1개별 PGA 인덱스 설정. |
| `Set_ADCMaxNumChannel` | `short Set_ADCMaxNumChannel(unsigned char maxnum_channel)` | ADC 앞단 MUX의 최대 채널 설정. `2^n`로 채널 수를 지정. **정상 수행 시 반환값에 메시지 발생당 채널당 데이터 수(samples-per-channel)가 실린다.** |
| `Set_ConfigChannel` | `short Set_ConfigChannel(unsigned char *Is_Select_Channel)` | 채널 선택. |

> `Set_ADCMaxNumChannel`은 성공 시 단순 1이 아니라 **채널당 샘플 수**를 반환하므로,
> View 코드는 이 반환값을 `DISPDATANUM`에 저장해 이후 ACQPLOT·Forwarder에 넘긴다.

---

## View 핸들러 (`Test_LXSM_D1WD10View.cpp`)

메뉴 명령은 메시지 맵(`BEGIN_MESSAGE_MAP`)에서 핸들러로 라우팅되고, 스트림 메시지는
`ON_MESSAGE(WM_AcqUnitData, &CTest_LXSM_D1WD10View::OnStreamData)`로 연결된다.
View 멤버 중 핵심은 `DISPMAXCH`(표시·전송 채널 수, 마킹 채널 포함), `DISPDATANUM`(채널당 샘플 수),
`m_forwarder`(`Forwarder*`), `m_seq`(프레임 시퀀스 카운터)다. 생성자 초기값은
`DISPMAXCH = 33`, `DISPDATANUM = 16`이며 `m_forwarder = new Forwarder(); m_seq = 0;`로 시작한다
(소멸자에서 `delete m_forwarder`).

### `OnStreamData(WPARAM wParam, LPARAM lParam)`

스트림 프레임 수신 핸들러(`afx_msg LRESULT`). `lParam`이 DLL이 넘긴 float 프레임 포인터다.

```cpp
ACQPLOT_DLL_Array_Datain_Strip((float *)(lParam), DISPMAXCH, DISPDATANUM);
m_forwarder->Send(m_seq++, (const float*)lParam, (uint16_t)DISPMAXCH, (uint16_t)DISPDATANUM);
return 0;
```

먼저 ACQPLOT으로 파형을 갱신한 뒤, 같은 원본 포인터를 `Forwarder::Send`로 넘겨
localhost UDP로 Python 브리지에 forward한다. `m_seq`는 매 프레임 후위 증가한다.

Forwarder가 보내는 UDP 패킷의 수신·LSL 변환 전체 흐름은 [network-and-streams.md](./network-and-streams.md) 참조.

### `OnMENUInitDevicePolyGA()` — PolyG-A 초기화

초기화 순서(반환값 검사 포함):

1. `retv = Init_Device(this->m_hWnd, 14)` — PolyG-A 고유 ID 14.
2. 성공(`retv == 1`) 시 `Sleep(1000)` — `Set_ADCMaxNumChannel` 호출 전 1초 대기.
3. `DISPMAXCH = 33` — 32채널 + 마킹 채널 1.
4. `retv = Set_ADCMaxNumChannel(DISPMAXCH - 1)` (= 32) → `Sleep(100)`.
5. `retv > 1`이면 `DISPDATANUM = retv` (채널당 샘플 수).
6. `Set_PGA(BRIDGE_PGA_GAIN_IDX)` → `Sleep(100)`.
7. `m_forwarder->Init(BRIDGE_HOST, BRIDGE_PORT)` → `m_seq = 0`.

**`Init_Device` 반환 코드:** `1` 성공 / `-1` 실패 / `-2` 이미 초기화됨 / `-3` 장치 명령 전송 실패.
각 코드마다 `AfxMessageBox`로 사용자에게 안내한다.

### `OnMenuInitdevicePolygI()` — PolyG-I 초기화

`OnMENUInitDevicePolyGA`와 동일 패턴이되 `Init_Device(this->m_hWnd, 16)`(PolyG-I 고유 ID 16),
`DISPMAXCH = 17`(16채널 + 마킹 1)을 사용한다. 이후 `Set_ADCMaxNumChannel(16)` → `Set_PGA` →
`m_forwarder->Init` → `m_seq = 0` 흐름과 반환 코드(`1/-1/-2/-3`)는 동일하다.
(PolyG-A 핸들러와 달리 `Init_Device` 성공 직후의 `Sleep(1000)`은 없다.)

### 샘플링 주파수 메뉴 그룹

각 핸들러는 `Set_SampleFreq(idx)`를 호출하며, `2^idx = 원하는 주파수`가 되도록 `idx`를 정한다.

| 핸들러 | 호출 | 결과 주파수 |
|--------|------|------------|
| `OnMENUSetSample128` | `Set_SampleFreq(7)` | 128 Hz |
| `OnMENUSetSample256` | `Set_SampleFreq(8)` | 256 Hz |
| `OnMENUSetSample512` | `Set_SampleFreq(9)` | 512 Hz |
| `OnMENUSetSample1024` | `Set_SampleFreq(10)` | 1024 Hz |
| `OnMENUSetSample2048` | `Set_SampleFreq(11)` | 2048 Hz |
| `OnMENUSetSample4096` | `Set_SampleFreq(12)` | 4096 Hz |

**반환 코드:** `1` 성공 / `-1` 장치 미초기화(먼저 초기화 필요) / `-3` 장치 명령 전송 실패 /
`-4` 현재 최대 채널 수에서 지원되지 않는 샘플링 주파수(낮은 값 설정 필요) /
`-10` 샘플링 주파수 인덱스를 0~14 사이로 지정해야 함.

### ADC 최대 채널 수 메뉴 그룹

| 핸들러 | 호출 | 설정 후 `DISPMAXCH` |
|--------|------|---------------------|
| `OnMENUSetADCMaxNumCh32` | `Set_ADCMaxNumChannel(32)` | 33 |
| `OnMENUSetADCMaxNumCh16` | `Set_ADCMaxNumChannel(16)` | 17 |
| `OnMENUSetADCMaxNumCh8` | `Set_ADCMaxNumChannel(8)` | 9 |
| `OnMENUSetADCMaxNumCh4` | `Set_ADCMaxNumChannel(4)` | 5 |
| `OnMENUSetADCMaxNumCh2` | `Set_ADCMaxNumChannel(2)` | 3 |

각 핸들러는 `retv > 0`이면 `DISPDATANUM = retv`로 갱신하고,
`DISPWIDTH`는 `DISPDATANUM <= 64`이면 64, 초과하면 `retv`로 설정한 뒤 화면을 갱신한다.

### 스트림 / 장치 제어 핸들러

- `OnMENUStartStream()` — `Start_Stream()` 호출. 이후 모든 처리는 `OnStreamData` 메시지 루틴에서 수행된다. Stop_Stream 호출 전까지 채널당 지정 샘플 수가 모일 때마다 메시지가 무한 반복 발생한다.
- `OnMENUStopStream()` — `Stop_Stream()` 호출(실시간 수집 중단).
- `OnMENUCloseDevice()` — `Close_Device()` 호출. 성공(`retv == 1`) 시 "장치 해제함." 표시.
- `OnMENUSetPGASourceGroup()` — `CDlgSetPgaSrcGroup` 다이얼로그로 `Pga_idx`/`SrcGrp_idx`를 입력받아, 범위가 유효할 때만(`Pga_idx` 0~15, `SrcGrp_idx` 0~7 또는 254/255) `Set_PGA_SourceGroup(SrcGrp_idx, Pga_idx)`를 호출한다.
- `OnDraw(CDC* pDC)` — 채널별 Y축 라벨(`Ch1`…`Ch{n}`, 마지막은 `Ch{n}-Mark`)을 만들고 `ACQPLOT_DLL_Window_Size_Changed_Or_Data_Set_Changed` → `ACQPLOT_DLL_Array_Draw_Box_Axis` → `ACQPLOT_DLL_Draw_Axis_Y_Text`로 파형 디스플레이 프레임을 그린다.

> ⚠️ `Start_Stream` 전에 샘플링 주파수 메뉴를 한 번 선택해야 한다. 매뉴얼상 최대 채널 수를
> 변경하면 샘플링 주파수를 다시 설정해야 하므로(채널 수 변경 → 주파수 재설정), 채널 수를 바꾼
> 뒤에는 반드시 주파수 메뉴를 다시 눌러야 한다.

---

## `Forwarder` 클래스 (`Forwarder.h` / `Forwarder.cpp`)

LXEM 프레임(16바이트 헤더 + 채널-major float 페이로드)을 UDP로 전송한다. 소켓은
`Init()`에서 한 번 열고 모든 `Send()`에서 재사용한다(프레임마다 소켓을 새로 만들지 않음).
헤더 레이아웃은 `src/polyg_lsl/protocol.py`와 정확히 일치한다.

### 멤버 함수

- `Forwarder()` — `m_sock = INVALID_SOCKET`, `m_wsa = false`, `m_dest`를 0으로 초기화.
- `~Forwarder()` — `Close()` 호출.
- `bool Init(const char* host, unsigned short port)` — `WSAStartup(MAKEWORD(2,2), ...)` → `m_wsa = true` → UDP 소켓 생성(`socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)`) → `m_dest`에 `sin_family/htons(port)/inet_pton(host)`로 목적지 설정. 단계 실패 시 `false` 반환.
- `bool Send(uint32_t seq, const float* data, uint16_t num_channels, uint16_t samples_per_channel)` — 16바이트 LE 헤더 + 페이로드를 한 버퍼에 `memcpy`로 조립해 `sendto`. 소켓이 `INVALID_SOCKET`이면 `false`. 보낸 바이트 수가 버퍼 크기와 같을 때 `true`.
- `void Close()` — `closesocket(m_sock)` 후 `m_sock = INVALID_SOCKET`, `m_wsa`가 참이면 `WSACleanup()` 후 `m_wsa = false`.

### private 멤버

| 멤버 | 형 | 용도 |
|------|----|------|
| `m_sock` | `SOCKET` | 재사용 UDP 소켓 |
| `m_dest` | `sockaddr_in` | 목적지 주소(host:port) |
| `m_wsa` | `bool` | WSAStartup 완료 여부(Close 시 WSACleanup 가드) |

### LXEM 헤더 레이아웃 (16바이트, little-endian)

`Send`는 페이로드 크기 `num_channels * samples_per_channel * sizeof(float)`를 계산하고
`16 + payload` 버퍼를 채운다. 상수는 `LXEM_MAGIC = 0x4C58454D`('LXEM'), `LXEM_VERSION = 1`.

| offset | size | field | 값 / 비고 |
|--------|------|-------|-----------|
| 0 | 4 | magic | `0x4C58454D` ('LXEM') |
| 4 | 2 | version | `1` |
| 6 | 2 | num_channels | `nch` (= `DISPMAXCH`) |
| 8 | 2 | samples_per_channel | `spc` (= `DISPDATANUM`) |
| 10 | 2 | flags | `0` |
| 12 | 4 | seq | `seq` (`m_seq` 후위 증가) |
| 16 | payload | float 데이터 | `memcpy(p+16, data, payload)` |

> ⚠️ Winsock 헤더 순서: `<winsock2.h>`가 `<windows.h>`보다 먼저 와야 한다. 그래서 View 헤더에는
> 전방 선언된 포인터 멤버(`Forwarder* m_forwarder;`)만 두고, `Forwarder.h`는 `.cpp`에서만 include한다.
> (`Forwarder.cpp`는 추가로 `<ws2tcpip.h>`를 포함하고 `#pragma comment(lib, "Ws2_32.lib")`로 링크한다.)

---

## `BridgeConfig.h`

C++ 측에는 TOML 파서가 없으므로(v1 기준), 브리지 연동에 필요한 값을 매크로로 미러링한다.

| 매크로 | 값 | 용도 |
|--------|----|------|
| `BRIDGE_HOST` | `"127.0.0.1"` | forward 목적지 host (localhost) |
| `BRIDGE_PORT` | `51234` | forward 목적지 UDP 포트 |
| `BRIDGE_PGA_GAIN_IDX` | `9` | 초기화 시 `Set_PGA`에 넘기는 게인 인덱스 |

> ⚠️ `BRIDGE_PGA_GAIN_IDX`는 반드시 `config.toml`의 `[device].gain_idx`와 **같아야** 한다.
> 두 값이 다르면 장치 게인과 Python의 µV 스케일링이 어긋나 잘못된 µV 출력이 나온다.
