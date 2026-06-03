# Visual Studio에서 x64로 빌드하기 (초보자 가이드)

이 프로젝트(`PolyG_DLL_API`)를 Windows의 Visual Studio에서 **x64 구성**으로
빌드하는 방법입니다. Python LSL 브리지로 프레임을 전송하도록 수정된 사본이며, 원본은
별도 보관되어 있습니다.

**이 프로젝트의 정확한 빌드 조건**
- 플랫폼 도구집합 **v143** → **Visual Studio 2022** 가 맞습니다 (폴더명은 VC2017이지만 내부 설정은 v143).
- **문자셋 MultiByte(MBCS)** → 설치 시 **MFC(MBCS 포함) 구성요소**가 반드시 필요합니다 (초보자가 가장 많이 막히는 부분).
- **Release|x64 는 MFC 정적 링크 + `/MT`(CRT 정적)** 로 설정돼 있어, 빌드된 exe는
  VC++ 재배포 패키지 없이 단독 실행됩니다 (배포용). Debug/Win32 구성은 종전대로 동적 링크입니다.
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

1. 파일 탐색기에서 `PolyG_DLL_API` 폴더로 이동.
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
   PolyG_DLL_API\Release64bit\Test_LXSM_D1WD10.exe
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

## 8. 배포 — GitHub Release zip 만들기

사용자가 직접 빌드하지 않고 **zip만 받아 실행**하게 하려면, 빌드된 Release|x64
산출물을 zip으로 묶어 GitHub Release에 올립니다. Release|x64는 정적 링크라 받는
사람 PC에 별도 런타임 설치가 필요 없습니다.

저장소 루트의 **`release/package.ps1`** 스크립트가 이 과정을 자동화합니다
(exe + DLL 2개 + 최종 사용자용 README를 한 zip으로 묶음).

```powershell
# 0) (권장) 먼저 커밋 & 푸시 — 태그가 올바른 커밋을 가리키도록
git push

# 1) Visual Studio에서 Release|x64로 이미 빌드했다면 — 패키징만:
pwsh release\package.ps1 -Version v0.1.0

# 2) 빌드부터 릴리스 업로드까지 한 번에 (msbuild + gh 필요):
pwsh release\package.ps1 -Version v0.1.0 -Build -Publish
```

- `-Build` : msbuild로 Release|x64를 빌드한 뒤 패키징.
- `-Publish` : `gh release create`로 GitHub Release를 만들고 zip을 업로드
  (사전에 `gh auth login` 필요).
- 결과 zip: 루트의 `PolyG_DLL_API-<version>-win-x64.zip`.
- 스크립트는 `dumpbin`이 있으면 exe가 MFC/CRT DLL을 동적으로 참조하는지 검사해,
  정적 링크가 안 됐으면 경고합니다(이 경우 Release|x64로 다시 빌드).

> ℹ️ 장비 USB 드라이버는 zip에 포함되지 않습니다. 실제 측정 PC에는 제조사 드라이버가
> 따로 설치돼 있어야 합니다.

### 자동화 (GitHub Actions)

수동 빌드·업로드 대신, **태그를 push하면 자동으로** Windows 러너에서 빌드 → zip
패키징 → Release 업로드까지 진행됩니다 (`.github/workflows/release.yml`).

```bash
git tag v0.1.1
git push origin v0.1.1     # 이 push가 워크플로를 트리거
```

- 러너(windows-2022)에는 VS 2022 **v143**만 있으므로 워크플로는 `/p:PlatformToolset=v143`
  으로 빌드합니다(정적 링크·MBCS 설정은 동일하게 적용되어 단독 실행 exe가 나옵니다).
- 릴리스 노트는 `release/RELEASE_NOTES.md`(UTF-8)에서 읽습니다. 문구 변경은 이 파일만 수정.
- `Actions` 탭에서 **Run workflow**(workflow_dispatch)로 수동 실행하면 Release를 만들지
  않고 zip을 **빌드 아티팩트**로만 올려, 패키징을 미리 시험할 수 있습니다.

---

## 참고

- 이 앱에 적용한 수정 내용은 `../polyg-lsl-bridge/cpp/README.md` 참고.
- 전체 시스템(설정·실행·µV 변환·마커)은 `../polyg-lsl-bridge/README.md` 참고.
