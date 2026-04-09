---
description: UE5.7 버추얼 프로덕션 파이프라인 전체 워크플로우 (MCP/AI 자동화 포함)
---

# 📋 VP Pipeline 개발 워크플로우

> 이 문서는 guide.md의 실현가능성 검토 결과를 반영한 **구체적 실행 워크플로우**입니다.
> 각 단계마다 AI 자동화(MCP, Claude, Antigravity)를 적극 활용하여 개발 속도를 극대화합니다.

---

## 워크플로우 구조

본 워크플로우는 Phase별로 분리되어 있습니다. 필요한 Phase를 선택하여 진행하세요.

| Phase | 파일 | 기간 | 핵심 내용 |
|-------|------|------|-----------|
| **Phase 0** | [phase0_setup.md](phase0_setup.md) | Day 1~2 | Python 환경 + UE5.7 프로젝트 + MCP 서버 셋업 |
| **Phase 1** | [phase1_tracking.md](phase1_tracking.md) | Week 1 | MediaPipe 트래킹 + UDP 네트워크 발신 모듈 |
| **Phase 2** | [phase2_ue_plugin.md](phase2_ue_plugin.md) | Week 2~3 | UE5.7 C++ 수신 플러그인 개발 |
| **Phase 3** | [phase3_animation.md](phase3_animation.md) | Week 4~5 | 애니메이션 리타겟팅 + 프로파일링 최적화 |
| **Phase 4** | [phase4_broadcast.md](phase4_broadcast.md) | Week 6 | 방송 자동화 + 실전 테스트 + 트러블슈팅 |

---

## 전체 파이프라인 흐름

```
[노트북 웹캠] → [Python: MediaPipe 추론] → [UDP/OSC 패킷] → [UE5.7: C++ 수신]
     ↓                                                              ↓
[720p 30fps+]                                              [AnimInstance 매핑]
                                                                    ↓
                                                           [아바타 실시간 구동]
                                                                    ↓
                                                    [OBS 캡처 → 치지직 RTMP 송출]
```

---

## 📁 최종 프로젝트 구조

```
c:\UnrealWork\VP\
├── guide.md                         # 프로젝트 가이드 문서
├── taskReport.md                    # 작업 이력 보고서
├── .agents/
│   └── workflows/
│       ├── workflow.md              # 이 문서 (마스터 인덱스)
│       ├── phase0_setup.md          # Phase 0: 환경 셋업
│       ├── phase1_tracking.md       # Phase 1: 트래킹 모듈
│       ├── phase2_ue_plugin.md      # Phase 2: C++ 플러그인
│       ├── phase3_animation.md      # Phase 3: 애니메이션
│       └── phase4_broadcast.md      # Phase 4: 방송 자동화
├── vp-tracker/                      # Python 트래킹 + 자동화 모듈
│   ├── pyproject.toml               # uv 프로젝트 설정
│   ├── tracker.py                   # 통합 트래킹 (Face + Pose)
│   ├── sender.py                    # UDP/OSC 네트워크 발신
│   ├── obs_controller.py            # OBS WebSocket 제어
│   ├── launcher.py                  # 원클릭 파이프라인 런처
│   ├── mcp_server.py                # 프로젝트 전용 MCP 서버
│   └── models/                      # MediaPipe .task 모델 파일
│       ├── face_landmarker.task
│       └── pose_landmarker_heavy.task
└── VPPipeline/                      # Unreal Engine 5.7 프로젝트
    ├── VPPipeline.uproject
    ├── Source/
    │   └── VPPipeline/
    └── Plugins/
        └── VPTrackerReceiver/       # C++ 수신 플러그인
            ├── VPTrackerReceiver.uplugin
            └── Source/
                └── VPTrackerReceiver/
                    ├── Public/
                    │   ├── VPUDPReceiver.h
                    │   ├── VPTrackingData.h
                    │   └── VPAnimInstance.h
                    └── Private/
                        ├── VPUDPReceiver.cpp
                        ├── VPTrackingData.cpp
                        └── VPAnimInstance.cpp
```

---

## 📊 MCP 자동화 도구 요약

| MCP 도구 | 용도 | Phase |
|-----------|------|-------|
| `check_webcam()` | 웹캠 접근 가능 여부 진단 | 0, 1 |
| `check_port_available(port)` | UDP/OBS 포트 충돌 확인 | 0, 2, 4 |
| `check_obs_connection()` | OBS WebSocket 연결 상태 | 0, 4 |
| `check_mediapipe_models()` | .task 모델 파일 존재 확인 | 0, 1 |
| `run_pipeline_diagnostics()` | 전체 파이프라인 사전 진단 (원클릭) | 0~4 |
| **Unreal MCP Server** | 에디터 내 액터/프로퍼티 자연어 제어 | 2~4 |
| **Claude Desktop** | C++/Python 코드 생성, 디버깅 | 전 과정 |
| **Antigravity/Gemini CLI** | 코드 편집, 리팩터링, 문서 생성 | 전 과정 |