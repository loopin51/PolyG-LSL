# EEG 실험 인프라 문서 허브

이 repository는 뇌파(EEG) 실험을 위한 인프라 코드와 문서를 모은 곳으로, 서로 독립적으로 동작하는 세 개의 subsystem으로 구성된다. (1) 세 대의 PC를 UDP로 묶어 Telescan 녹화와 PowerPoint 슬라이드를 자동화하는 **3-PC 자동화 system**, (2) PolyG 장비에서 실시간 EEG를 취득해 µV 채널을 LSL stream으로 내보내는 **PolyG LSL 브리지**, 그리고 (3) 브리지의 장비-측 front-end인 **C++ 취득 앱**이다. 이 page는 각 subsystem의 관계와 세부 문서로의 navigation을 제공한다.

## 서브시스템 지도

```
3-PC 자동화 (Telescan 버튼 자동화 · EEG 스트리밍 아님)        ── 독립
PolyG LSL 브리지 (실시간 EEG → LSL outlet)  ◄── C++ 취득 앱 (장비 프런트엔드)
```

3-PC 자동화 system은 Telescan의 녹화/정지 **버튼을 좌표 클릭으로 자동화**할 뿐 EEG 자체를 streaming하지 않는 독립 subsystem이다. 반면 PolyG LSL 브리지는 본 자동화 system과 별개로 새로 개발된 system으로, 실시간 µV 신호를 LSL outlet으로 내보낸다. C++ 취득 앱은 PolyG 장비를 구동해 프레임을 브리지로 전달하는 그 브리지의 장비-측 front-end이다.

## 문서 navigation

| 문서 | 내용 |
|---|---|
| [three-pc-automation.md](./three-pc-automation.md) | 3-PC Telescan 자동화 함수 레퍼런스 |
| [cpp-acquisition-app.md](./cpp-acquisition-app.md) | 제조사 C++ MFC 앱 (DLL API·핸들러·Forwarder) |
| [polyg-lsl-bridge.md](./polyg-lsl-bridge.md) | LSL 브리지 모듈 요약 + 링크 |
| [protocols-and-formats.md](./protocols-and-formats.md) | UDP/scenario/파일명/로그/LXEM 교차 참조 |
| [network-and-streams.md](./network-and-streams.md) | C++ UDP 패킷(LXEM) ↔ Python LSL 스트림 네트워크/데이터 흐름 |

## 실행 진입점

3-PC 자동화 system의 각 script는 서로 다른 PC에서 실행한다.

| PC | script | 역할 |
|---|---|---|
| A | `A_eeg_client.py` | EEG PC — Telescan 녹화/정지 자동화 |
| B | `B_slide_client.py` | Slide PC — PowerPoint 슬라이드 진행 |
| C | `C_controller.py` | Controller PC — 시나리오 타이머·UDP 브로드캐스트 (operator) |

worker인 A·B를 먼저 띄운 뒤 controller인 C를 마지막에 실행한다. 설치·시나리오 편집·좌표 생성 등 전체 실행 절차는 root의 [`../readme.md`](../readme.md)를 참고한다.

## 기존 readme.md와의 차이

root의 `readme.md`는 이번 문서화 pass에서 실제 코드와 일치하도록 수정되었다. 주요 차이는 다음과 같다.

- **script 파일명**: 문서상의 `eeg_client.py` → 실제 `A_eeg_client.py`, `slide_client.py` → `B_slide_client.py`, `controller.py` → `C_controller.py`로 정정.
- **미구현 항목(확장 지점)**: 기존 readme가 언급한 `python controller.py --file custom.yaml` CLI 옵션은 구현되어 있지 않으며, `C_controller.py`는 항상 `config.py`의 `SCENARIO_FILE`을 로드한다. 또한 명령 `MARK_RESP`(A), `SHOW_START`·`SHOW_END`(B)도 client에 구현되어 있지 않다. 이들은 향후 추가 가능한 확장 지점일 뿐 현재 동작하는 명령이 아니다.
