# C++ 취득 앱 수정 가이드

제조사 `Test_LXSM_D1WD10_VC2017`의 **사본**에 아래 수정을 적용해, 장비에서 받은
프레임을 localhost UDP로 Python 브리지에 넘기도록 만듭니다. **원본은 건드리지 않습니다.**

> 이 저장소 기준으로는 이미 사본(`../Test_LXSM_D1WD10_VC2017`)에 아래 내용이 모두
> 적용되어 있습니다. 이 문서는 무엇이 어떻게 바뀌었는지(또는 새 사본에 다시 적용하는 법)를
> 설명합니다.

---

## 0. winsock 헤더 순서 주의 (왜 포인터 멤버를 쓰는가)

`Forwarder.h`는 `<winsock2.h>`를 포함합니다. 그런데 `winsock2.h`는 반드시
`windows.h`보다 **먼저** 포함되어야 합니다. 이 프로젝트에서 `Test_LXSM_D1WD10.cpp`는
`stdafx.h`(→ `windows.h`) 다음에 `Test_LXSM_D1WD10View.h`를 include합니다. 따라서
**`View.h`에서 `Forwarder.h`를 include하면** 그 파일에서 `winsock2.h`가 `windows.h`
뒤에 들어가 컴파일 오류가 납니다.

그래서 헤더에는 `Forwarder`를 **전방선언 + 포인터 멤버**로만 두고, 실제 `Forwarder.h`는
`.cpp`에서만 include합니다(`.cpp`는 맨 위에서 `winsock2.h`를 먼저 포함하므로 안전).

---

## 1. 프로젝트에 파일 추가

`Forwarder.h`, `Forwarder.cpp`, `BridgeConfig.h`를 VS 프로젝트에 추가합니다
(`.vcxproj`의 `ClCompile`/`ClInclude` 및 `.vcxproj.filters`에 등록).

## 2. 64비트 lib 분기 수정 — `Test_LXSM_D1WD10View.cpp`

`#if(x64)`는 표준 매크로가 아니라 항상 거짓으로 평가되어 64비트 빌드에서도 32비트
lib를 링크하는 버그가 있습니다. `_WIN64`로 고칩니다:

```cpp
#ifdef _WIN64
#pragma comment(lib,"LIB_64bit\\LXSM-D1WD10.lib")
#pragma comment(lib,"LIB_64bit\\ACQPLOT.lib")
#elif (WIN32)
#pragma comment(lib,"LIB_32bit\\LXSM-D1WD10.lib")
#pragma comment(lib,"LIB_32bit\\ACQPLOT.lib")
#endif
```

(`#if(x64)` 한 줄만 `#ifdef _WIN64`로 바꾸면 됩니다. `#elif (WIN32)` 분기는 그대로
두어도 32비트 빌드에서 올바르게 동작합니다.)

## 3. `Test_LXSM_D1WD10View.h` 멤버 추가

`class` 선언 위에 전방선언과 `<cstdint>`를 두고, public 영역에 포인터 멤버를 추가합니다:

```cpp
class Forwarder;            // 전방선언 (실제 정의는 .cpp에서 include하는 Forwarder.h)
#include <cstdint>

class CTest_LXSM_D1WD10View : public CView
{
    // ... public 영역 ...
    int DISPMAXCH, DISPDATANUM, DISPWIDTH;
    Forwarder* m_forwarder;   // Python LSL 브리지로 보내는 UDP 포워더
    uint32_t   m_seq;          // LXEM 프레임 카운터
};
```

> ⚠️ 헤더에서는 `Forwarder.h`를 include하지 마세요(0절 참고). 포인터 + 전방선언만 둡니다.

## 4. `.cpp`에서 include + 생성자/소멸자

`Test_LXSM_D1WD10View.cpp`의 기존 include 블록 끝(예: `DlgSetPgaSrcGroup.h` 다음)에
추가합니다:

```cpp
#include "Forwarder.h"
#include "BridgeConfig.h"
```

생성자에서 포워더를 생성하고 시퀀스를 0으로, 소멸자에서 해제합니다:

```cpp
CTest_LXSM_D1WD10View::CTest_LXSM_D1WD10View()
{
    // ... 기존 초기화 ...
    m_forwarder = new Forwarder();
    m_seq = 0;
}

CTest_LXSM_D1WD10View::~CTest_LXSM_D1WD10View()
{
    delete m_forwarder;
}
```

## 5. `OnStreamData` 본문 교체

기존 `printf` 루프와 옛 `SendDataOverUDP(...)` 호출을 지우고, 헤더+프레임을 한 번에
전송합니다(실시간 파형 플로팅 `ACQPLOT_DLL_*`는 그대로 둡니다):

```cpp
afx_msg LRESULT CTest_LXSM_D1WD10View::OnStreamData(WPARAM wParam, LPARAM lParam)
{
    ACQPLOT_DLL_Array_Datain_Strip((float*)(lParam), DISPMAXCH, DISPDATANUM);

    // 원시 프레임을 localhost UDP로 Python LSL 브리지에 전달
    m_forwarder->Send(m_seq++, (const float*)lParam,
                      (uint16_t)DISPMAXCH, (uint16_t)DISPDATANUM);
    return 0;
}
```

(`DISPMAXCH`에는 이미 마킹 채널이 포함되어 있고, `DISPDATANUM`은 채널당 샘플 수입니다.
각각 프레임 헤더의 `num_channels`, `samples_per_channel`이 됩니다.)

## 6. 초기화 타이밍 + 포워더 Init — `OnMENUInitDevicePolyGA`, `OnMenuInitdevicePolygI`

매뉴얼이 요구하는 0.1초 대기를 넣고, 게인 인덱스를 config와 맞춘 뒤, 포워더를 한 번
초기화합니다. **두 핸들러(PolyG-A, PolyG-I) 모두**에 적용합니다:

```cpp
retv = Set_ADCMaxNumChannel(DISPMAXCH-1);
Sleep(100);                                  // 매뉴얼: Set_ADCMaxNumChannel 후 0.1초 대기
if (retv > 1) {
    DISPDATANUM = retv;
    // ... 기존 AfxMessageBox / OnUpdate ...
    Set_PGA(BRIDGE_PGA_GAIN_IDX);            // 반드시 config.toml [device].gain_idx 와 동일
    Sleep(100);                              // 매뉴얼: Set_PGA 후 0.1초 대기
    m_forwarder->Init(BRIDGE_HOST, BRIDGE_PORT);
    m_seq = 0;
}
```

- `BridgeConfig.h`의 `BRIDGE_PGA_GAIN_IDX`(장비 PGA 게인 인덱스)는 **반드시**
  `config.toml`의 `[device].gain_idx`와 같아야 합니다. 다르면 장비가 실제 적용한 게인과
  Python 브리지의 µV 변환 게인이 어긋나 **µV 값이 틀립니다.**
- 초기화 후 **`Start_Stream` 전에** 샘플링 주파수 메뉴(`Set Sample ...`)를 반드시
  한 번 선택하세요. 매뉴얼상 채널 수를 바꾼 뒤에는 샘플링 주파수를 다시 설정해야 합니다.

## 7. (선택) 미사용 `SendDataOverUDP` 함수 제거

이제 쓰이지 않는 `SendDataOverUDP(...)` 자유 함수 정의는 지워도 됩니다(무해한 dead
code라 남겨둬도 빌드에는 영향 없음).

---

## 빌드 / 실행

- Visual Studio에서 **x64 구성**으로 빌드합니다.
- 실행 순서: Python 쪽 `polyg-bridge --config config.toml`를 먼저 띄운 뒤, 이 C++ 앱에서
  `Init Device` → 샘플링 주파수 설정 → `Start_Stream`. 정상이라면 브리지 로그에 EEG
  outlet이 잡히고 `mismatch`/`dropped` 경고가 거의 없어야 합니다.
- 자세한 전체 파이프라인은 상위 `../README.md` 4·5절을 참고하세요.
