# PolyG EEG 취득 앱 (Windows x64)

PolyG 뇌파 장비에서 프레임을 받아 **localhost UDP**로 내보내는 장비-측 취득
프로그램입니다. 이 프로그램만으로는 데이터가 저장되지 않으며, **Python LSL
브리지**(`polyg-bridge`)와 함께 동작합니다.

필요한 C/C++ 런타임 DLL이 **함께 들어 있어**, **Windows 10 이상**에서는 별도의
Visual C++ 재배포 패키지 설치 없이 압축만 풀면 실행됩니다.

## 포함 파일

| 파일 | 설명 |
|---|---|
| `Test_LXSM_D1WD10.exe` | 취득 앱 본체 |
| `ACQPLOT.dll` | 실시간 파형 표시 DLL |
| `LXSM-D1WD10.dll` | PolyG 장비 구동 DLL |
| `mfc140.dll`, `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` | exe용 런타임 |
| `mfc100.dll`, `msvcr100.dll` | ACQPLOT.dll용 런타임 |

> ⚠️ 폴더 안의 **모든 파일을 같은 위치에** 두세요. exe만 따로 옮기면 DLL을 못 찾아
> 실행되지 않습니다. (런타임 DLL이 빠진 zip을 받았다면 Windows 10+ 인지, 또는 VC++
> 2010·2015–2022 x64 재배포 패키지 설치 여부를 확인하세요.)

## 사용 전 준비

- **Windows 10 이상, 64비트.**
- **PolyG 장비**가 USB로 연결되어 있어야 하며, 제조사 **USB 드라이버**가 설치되어
  있어야 합니다(이 zip에는 포함되지 않습니다).
- 데이터를 LSL로 받으려면 같은(또는 연결된) PC에서 **Python 브리지**가 떠 있어야
  합니다. 설치/실행은 저장소의 `polyg-lsl-bridge/README.md` 참고:
  ```
  polyg-bridge --config config.toml
  ```

## 실행 순서

1. `polyg-bridge`를 먼저 실행해 둡니다(브리지가 UDP `127.0.0.1:51234`에서 대기).
2. `Test_LXSM_D1WD10.exe` 실행.
3. 메뉴에서 차례로:
   - **Init Device** (PolyG-A 또는 PolyG-I)
   - **Set Sample …** (샘플링 주파수 메뉴를 **반드시** 한 번 선택)
   - **Start Stream**
4. LabRecorder 등 LSL 레코더로 EEG/마커 스트림을 기록합니다.

> 채널 수를 바꾼 경우 **샘플링 주파수 메뉴를 다시** 선택해야 합니다.

## 설정값(host/port/gain)을 바꾸려면

전송 목적지(`BRIDGE_HOST`/`BRIDGE_PORT`)와 게인 인덱스(`BRIDGE_PGA_GAIN_IDX`)는
빌드 시점에 `BridgeConfig.h`에서 **컴파일되어 박혀 있습니다.** 값을 바꾸려면
소스에서 다시 빌드해야 합니다. 또한 이 값들은 브리지의 `config.toml`과 반드시
일치해야 합니다(특히 `gain_idx` ↔ `BRIDGE_PGA_GAIN_IDX`). 자세한 내용은 저장소
`README.md` / `docs/`를 참고하세요.
