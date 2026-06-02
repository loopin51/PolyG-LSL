# PolyG LSL Bridge

`polyg-lsl-bridge/`는 본 EEG 자동화 시스템과 별도로 개발된 새로운 system으로, LXEM device 프레임을 µV로 변환해 LSL outlet으로 stream하는 Python bridge이다. 자체 문서가 이미 충분히 갖춰져 있으므로, 이 page에서는 중복을 피하기 위해 모듈 요약과 링크만 제공한다.

## 모듈 요약

| 모듈 | 책임 | 상세 |
|---|---|---|
| `protocol.py` | LXEM 프레임 parse/build, 상수 테이블 | [README](../polyg-lsl-bridge/README.md) |
| `scaling.py` | raw→µV 변환 | 〃 |
| `config.py` | `config.toml` 로드·검증 | 〃 |
| `bridge.py` | 프레임→µV→LSL EEG outlet | 〃 |
| `markers.py` | 시나리오 마커 LSL outlet | 〃 |
| `fake_device.py` | 합성 프레임 생성(테스트) | 〃 |

## 상세 문서

- Bridge README: [`../polyg-lsl-bridge/README.md`](../polyg-lsl-bridge/README.md)
- 설계 spec: [`../polyg-lsl-bridge/docs/superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md`](../polyg-lsl-bridge/docs/superpowers/specs/2026-06-02-eeg-lsl-bridge-design.md)
- 구현 plan: [`../polyg-lsl-bridge/docs/superpowers/plans/2026-06-02-eeg-lsl-bridge.md`](../polyg-lsl-bridge/docs/superpowers/plans/2026-06-02-eeg-lsl-bridge.md)

device-side C++ front-end은 [cpp-acquisition-app.md](./cpp-acquisition-app.md)에 문서화되어 있다.
네트워크·데이터 흐름(UDP↔LSL)은 [network-and-streams.md](./network-and-streams.md) 참조.
