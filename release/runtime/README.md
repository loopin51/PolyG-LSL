# 벤더링된 런타임 DLL (선택)

`release/package.ps1`이 zip에 **함께 넣을 C/C++ 런타임 DLL**을 여기에 두면, 빌드 PC의
`System32`에 의존하지 않고 항상 같은 파일을 동봉합니다. 여기에 없는 DLL은 빌드 PC의
`System32`에서 자동으로 찾습니다.

## 언제 채워야 하나

- **로컬 빌드 PC**(Visual Studio 설치)라면 보통 비워둬도 됩니다 — 필요한 DLL이 `System32`에
  이미 있어 스크립트가 거기서 복사합니다.
- **GitHub Actions(windows-2022) 러너**에는 **VS2010 런타임(`mfc100.dll`, `msvcr100.dll`)이
  없습니다.** CI에서 만든 zip이 클린 PC에서 ACQPLOT.dll 로드에 실패하지 않으려면, 이 두
  파일을 여기에 커밋해 두세요.

## 동봉 대상 DLL

| DLL | 무엇을 위한 것 | 출처 |
|---|---|---|
| `mfc140.dll` | 호스트 exe (MBCS MFC, VS2015–2022) | VS / VC++ 2015–2022 재배포(MBCS) |
| `msvcp140.dll` | 호스트 exe C++ 표준 라이브러리 | VC++ 2015–2022 재배포 |
| `vcruntime140.dll` | 호스트 exe CRT | VC++ 2015–2022 재배포 |
| `vcruntime140_1.dll` | 호스트 exe CRT(x64 예외처리) | VC++ 2015–2022 재배포 |
| `mfc100.dll` | **ACQPLOT.dll** (MFC 10.0) | VC++ 2010 재배포 (x64) |
| `msvcr100.dll` | **ACQPLOT.dll** (CRT 10.0) | VC++ 2010 재배포 (x64) |

> UCRT(`ucrtbase.dll`, `api-ms-win-crt-*`)는 Windows 10 이상에 내장되어 있어 동봉하지
> 않습니다. 즉 배포 zip은 **Windows 10+** 를 가정합니다.

## 어디서 구하나

- `mfc100.dll`/`msvcr100.dll` : Microsoft **Visual C++ 2010 재배포 패키지(x64)** 설치 후
  `C:\Windows\System32`에서 복사하거나, 빌드 PC에 이미 있으면 그대로 사용.
- 나머지(`*140*`) : Visual Studio가 설치된 PC의 `System32`에 있습니다.

> ⚠️ 이 DLL들은 Microsoft 재배포 가능 구성요소입니다(앱과 같은 폴더에 동봉 허용). 직접
> 빌드한 파일이 아닌, 정품 Microsoft 재배포 DLL만 넣으세요.
