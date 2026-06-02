# Visual Studio에서 x64로 빌드하기 (초보자 가이드)

이 프로젝트(`Test_LXSM_D1WD10_VC2017`)를 Windows의 Visual Studio에서 **x64 구성**으로
빌드하는 방법입니다. Python LSL 브리지로 프레임을 전송하도록 수정된 사본이며, 원본은
별도 보관되어 있습니다.

**이 프로젝트의 정확한 빌드 조건**
- 플랫폼 도구집합 **v143** → **Visual Studio 2022** 가 맞습니다 (폴더명은 VC2017이지만 내부 설정은 v143).
- **MFC 동적 사용 + 문자셋 MultiByte(MBCS)** → 설치 시 **MFC(MBCS 포함) 구성요소**가 반드시 필요합니다 (초보자가 가장 많이 막히는 부분).
- Release|x64 결과물은 **`Release64bit\` 폴더**에 생성되며, 필요한 DLL 2개가 이미 들어 있습니다.

---

## 0. 준비물

- **Windows 10/11 64비트** PC (빌드만 할 거면 PolyG 장비는 없어도 됩니다. 실제 측정 시에만 장비+USB 필요)
- 인터넷 연결 (Visual Studio 설치용)

---

## 1. Visual Studio 2022 설치 (+ 필수 구성요소)

1. `https://visualstudio.microsoft.com/ko/downloads/` 접속 → **Visual Studio Community 2022**(무료) 다운로드 → 실행.
2. 설치 관리자(Visual Studio Installer)의 **"워크로드(Workloads)"** 탭에서:
   - ☑ **C++를 사용한 데스크톱 개발** (Desktop development with C++)
3. 오른쪽 **"설치 세부 정보(Installation details)"** 패널을 펼쳐 아래를 체크 (★ 핵심):
   - ☑ **최신 v143 빌드 도구용 C++ MFC (x86 및 x64)**
     (영문: *C++ MFC for latest v143 build tools (x86 & x64)*)
   - ☑ **Windows 11 SDK** (또는 Windows 10 SDK) — 보통 워크로드에 기본 포함
   > ⚠️ 이 프로젝트는 **MFC + MBCS(멀티바이트)** 설정이라, 위 MFC 구성요소가 없으면
   > `afxwin.h를 열 수 없습니다` 오류로 빌드가 실패합니다. 꼭 체크하세요.
4. **설치(Install)** 클릭 → 완료까지 대기 (10~20분).

> 이미 VS 2022가 깔려 있다면: 시작메뉴 **Visual Studio Installer** → **수정(Modify)** →
> 위 MFC 구성요소 추가 체크 → 수정.

---

## 2. 솔루션(프로젝트) 열기

1. 파일 탐색기에서 `Test_LXSM_D1WD10_VC2017` 폴더로 이동.
2. **`Test_LXSM_D1WD10.sln`** 더블클릭 → Visual Studio 2022가 열립니다.
3. **"프로젝트 다시 대상 지정(Retarget Projects)"** 창이 뜨면 설치된 최신 SDK/도구집합으로
   두고 **확인(OK)**. (안 떠도 정상입니다.)

---

## 3. 구성을 Release + x64로 설정 ★

화면 **상단 툴바**의 드롭다운 2개를 맞춥니다:

```
[ Release ▾ ]  [ x64 ▾ ]
```

- 왼쪽(구성): **`Release`**  (처음엔 보통 Debug)
- 오른쪽(플랫폼): **`x64`**  (처음엔 보통 x86/Win32)

> Debug로 빌드해도 동작하지만, 실제 실험엔 **Release** 권장.

---

## 4. 추가된 소스 파일 확인 (이미 등록됨)

오른쪽 **솔루션 탐색기(Solution Explorer)** 에 아래 파일들이 이미 등록돼 있습니다:

- `Forwarder.cpp` (Source Files)
- `Forwarder.h`, `BridgeConfig.h` (Header Files)

> 따로 추가할 필요 없습니다. 혹시 안 보이면 프로젝트 우클릭 → **추가(Add) → 기존
> 항목(Existing Item)** 으로 위 3개를 넣으세요.

**빌드 전 마지막 확인:** `BridgeConfig.h`의 `BRIDGE_PGA_GAIN_IDX`(현재 `9`)가 Python
`config.toml`의 `[device].gain_idx`와 **같은지** 확인 (다르면 µV 값이 틀어짐).

---

## 5. 빌드 실행

1. 메뉴 **빌드(Build) → 솔루션 빌드(Build Solution)** (단축키 **Ctrl + Shift + B**).
2. **출력(Output)** 창 마지막에 `빌드: 성공 1, 실패 0` (Build: 1 succeeded, 0 failed) 이면 성공.
3. 결과 실행파일:
   ```
   Test_LXSM_D1WD10_VC2017\Release64bit\Test_LXSM_D1WD10.exe
   ```
   이 폴더에 필요한 **`LXSM-D1WD10.dll`, `ACQPLOT.dll`** 이 이미 함께 있어 바로 실행됩니다.

---

## 6. 실행 (빌드 후)

- **장비 없이** 실행만 확인: **디버그(Debug) → 디버깅하지 않고 시작(Start Without
  Debugging)**, 단축키 **Ctrl + F5**. 창은 뜨지만 `Init Device`는 장비가 없으면 실패(정상).
- **실제 측정**:
  1. (다른 PC 또는 같은 PC에서) Python `polyg-bridge --config config.toml` 먼저 실행
  2. 이 C++ 앱에서 `Init Device` → **샘플링 주파수 메뉴 선택** → `Start Stream`
  3. µV LSL 스트림 송출 (자세한 순서는 `../polyg-lsl-bridge/README.md` 5절)

---

## 7. 자주 나는 오류 & 해결

| 오류 메시지 | 원인 / 해결 |
|---|---|
| `Cannot open include file: 'afxwin.h'` | **MFC 구성요소 미설치.** 1번 단계의 *C++ MFC for v143* 설치. |
| `... MBCS ...` / MFC 문자셋 관련 | 이 프로젝트는 MultiByte(MBCS). MFC 구성요소(MBCS 포함) 설치로 해결. |
| `플랫폼 도구집합 v143을 찾을 수 없음` | VS 2022가 아니거나 v143 미설치. VS 2022 설치, 또는 프로젝트 우클릭→속성→**플랫폼 도구집합**을 설치된 버전(v142 등)으로 변경. |
| `LNK1104: cannot open 'LIB_64bit\LXSM-D1WD10.lib'` | 구성이 **x64** 인지 확인(LIB_64bit는 64비트용). 3번 단계 재확인. |
| `winsock2.h ... windows.h` 순서 경고/오류 | 이미 포인터 멤버 방식으로 회피해 둠. 정상 빌드면 무시. |
| 실행 시 `DLL을 찾을 수 없음` | exe 옆에 `LXSM-D1WD10.dll`/`ACQPLOT.dll` 필요. Release64bit로 빌드하면 이미 있음. |

---

## 참고

- 이 앱에 적용한 수정 내용은 `../polyg-lsl-bridge/cpp/README.md` 참고.
- 전체 시스템(설정·실행·µV 변환·마커)은 `../polyg-lsl-bridge/README.md` 참고.
