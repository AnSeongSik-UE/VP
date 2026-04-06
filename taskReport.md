# Task Report

## 2026.04.06 23:16:38
### guide.md 실현가능성 검토
- guide.md 전체 내용 분석 완료
- MediaPipe Holistic 레거시 상태 확인 → MediaPipe Tasks API(FaceLandmarker, PoseLandmarker) 전환 필요
- UE 5.7 출시 확인 (2025.11.12 릴리즈)
- MediaPipe FaceLandmarker의 ARKit 52 블렌드쉐이프 네이티브 출력 지원 확인
- OBS WebSocket v5 Python 라이브러리(`obsws-python`) 사용 가능 확인
- 각 레이어별(Tracking, Rendering, Broadcasting) 기술적 실현가능성 평가 완료
- 4주 마일스톤 현실성 분석 → 6~8주 권장
- 수정 권장사항 4건 도출
- 결론: 기술적으로 실현 가능, 일부 용어/일정 수정 권장

## 2026.04.06 23:25:49
### guide.md 카메라 구성 변경 (휴대폰 → 노트북 웹캠)
- 모바일 카메라(IP WebCam/RTSP) → 노트북 내장 웹캠으로 전면 변경
- 다중 카메라 구조 → 단일 웹캠 구조로 아키텍처 단순화
- 변경 영역: 프로젝트 개요, 핵심 과제, Tracking Layer, Broadcasting Layer, 3.1 세부 전략, 포트폴리오 강조 포인트, Week 1 마일스톤
- RTSP/IP Webcam/모바일 관련 내용 모두 제거

## 2026.04.07 06:48:48
### guide.md 실현가능성 검토 결과 반영 및 전면 개선
- MediaPipe Holistic(레거시) → MediaPipe Tasks API (FaceLandmarker + PoseLandmarker) 용어/구현 전환
- FaceLandmarker 네이티브 ARKit 52 블렌드쉐이프 출력 기능 명시 (커스텀 변환 로직 불필요)
- 최소 하드웨어 요구사항 추가 (RTX 3060+, RAM 16GB+)
- AI 자동화 스택 추가 (Claude Desktop + MCP, Unreal MCP Server, Cursor/Gemini CLI)
- 마일스톤 4주 → 6주로 현실적 조정 (Week 2~3, Week 4~5 묶음)
- 각 주차별 구체적 작업 항목 및 검증 기준 추가
- 1PC 환경 GPU 분배 전략(렌더링 vs NVENC 인코딩) 명시
- Lumen 저사양 대안(Screen Space GI) 명시

### workflow.md 상세 워크플로우 문서 신규 작성
- .agents/workflows/workflow.md 생성
- Phase 0~4 (6주) 단계별 실행 가이드 작성
- Phase 0: 개발 환경 셋업 (Python uv, UE5.7, MCP 서버)
- Phase 1: MediaPipe Tracking 모듈 (tracker.py, sender.py 참조 코드 포함)
- Phase 2: UE5.7 C++ 수신 플러그인 (VPTrackerReceiver 구조, 핵심 헤더 코드)
- Phase 3: 애니메이션 리타겟팅 + Unreal Insights 프로파일링 워크플로우
- Phase 4: OBS WebSocket 방송 자동화 + 원클릭 런처
- 프로젝트 전용 MCP 서버(mcp_server.py) 설계 포함: 5개 진단 도구
- 각 Phase별 AI 자동화 활용 포인트 명시
- 트러블슈팅 가이드 및 성능 최적화 우선순위 포함
- 최종 프로젝트 디렉토리 구조 명시
