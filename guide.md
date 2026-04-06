# 프로젝트명: Unreal Engine 5.7 기반 실시간 렌더링 최적화 및 AI 자동화 버튜버 송출 파이프라인 구축

## 1. 프로젝트 개요 (Project Overview)
본 프로젝트는 고가의 특수 물리 장비 없이, 범용 하드웨어(노트북 내장 웹캠)만을 활용하여 가상 아바타를 구동하고, 이를 인터넷 방송 플랫폼(치지직)으로 송출하는 완전한 단일 엔진(Unreal Engine) 버추얼 프로덕션 파이프라인을 구축하는 것을 목표로 합니다. 

중간 브릿지 역할을 하던 서드파티 엔진(Unity)을 배제하고, 순수 Python 기반의 Vision AI 프로세스와 언리얼 엔진의 C++ 코어를 직접 연결하여 레이턴시(Latency)를 극단적으로 낮추는 시스템 엔지니어링 역량을 증명합니다.

* **타겟 플랫폼:** 치지직 (CHZZK) 실시간 스트리밍
* **주요 기술 스택:** Unreal Engine 5.7 (C++), Python 3.12+, OBS Studio 28.0+, MediaPipe Tasks API (FaceLandmarker + PoseLandmarker)
* **AI 자동화 스택:** Claude Desktop + MCP (Model Context Protocol), Unreal MCP Server, Cursor/Gemini CLI
* **핵심 과제:** 단일 웹캠 비전 스트림 기반 전신/표정 동시 추론, C++ 기반 엔진 코어 네트워크 최적화, MCP 기반 개발 워크플로우 자동화
* **최소 하드웨어 요구사항:** NVIDIA RTX 3060+ (Lumen 사용 시), RAM 16GB+, 노트북 내장 웹캠 720p+

---

## 2. 시스템 아키텍처 (System Architecture)

연산 부하를 분산하기 위해 Vision AI 기반의 트래킹 모듈과 렌더링 모듈을 분리한 마이크로서비스 구조입니다. 유니티를 배제함에 따라 트래킹 모듈이 전신과 표정 데이터를 모두 담당하도록 아키텍처를 재설계했습니다.

### A. Unified Tracking & Data Acquisition Layer (Python)
* **단일 웹캠 영상 스트림 처리:** 노트북 내장 웹캠 하나로 전신 포즈와 얼굴 표정을 동시에 촬영하는 단일 영상 소스 실시간 캡처.
* **MediaPipe Tasks API 통합 추론:** 단일 Python 프로세스 내에서 `mediapipe.tasks.python.vision`의 `PoseLandmarker`(33개 랜드마크)와 `FaceLandmarker`(478개 랜드마크)를 구동. FaceLandmarker의 `output_face_blendshapes=True` 옵션으로 **ARKit 52 블렌드쉐이프를 네이티브로 직접 출력** (커스텀 변환 로직 불필요).
* **Data Serialization:** 추출된 전신 트랜스폼 및 표정 가중치 데이터를 동기화(Sync)한 후, 단일 OSC(Open Sound Control) 페이로드 혹은 자체 규격의 UDP 패킷으로 직렬화하여 언리얼 엔진으로 송출.
* **성능 최적화:** FaceLandmarker + PoseLandmarker 동시 실행 시 CPU 부하 관리를 위해 GPU Delegate(CUDA) 활용 또는 비전 AI 스레드/네트워크 발신 스레드 분리.

### B. Rendering & Core Processing Layer (Unreal Engine 5.7)
* **Custom UDP/OSC Receiver (C++):** Python 프로세스에서 들어오는 고빈도(60Hz 이상) 트래킹 데이터를 멀티스레딩 환경에서 병목 없이 파싱하는 커스텀 수신 컴포넌트 플러그인 개발. UE의 `FSocket`, `FUdpSocketReceiver` API 활용.
* **Direct Animation Retargeting (C++ & AnimBP):** 수신된 구조체 데이터를 `TQueue<T>` (Lock-free Queue)로 게임 스레드에 안전하게 넘겨, `FAnimInstanceProxy`를 통해 아바타의 본(Bone) 회전값과 모프 타겟(Morph Target)에 실시간으로 매핑.
* **실시간 렌더링 최적화:** 60FPS 스트리밍 방어를 위한 언리얼 인사이트(Unreal Insights) 프레임 프로파일링, 메모리 풀링 적용 및 드로우콜(Draw Call) 최적화.
* **MCP 연동 (개발 시):** Unreal MCP Server 플러그인을 통해 AI 어시스턴트가 에디터 내 액터 배치, 프로퍼티 조정, 레벨 디자인을 자연어로 제어.

### C. Broadcasting & Automation Layer
* **AI 자동화 런처 (Python):** 엔진 실행, 웹캠 트래킹 모듈 구동, OBS 씬(Scene) 전환을 원클릭으로 제어하는 자동화 파이프라인.
* **OBS WebSocket v5 연동:** `obsws-python` 라이브러리를 활용하여 치지직 RTMP 송출을 위한 OBS Studio 백그라운드 제어(`ReqClient`) 및 방송 상태 실시간 모니터링(`EventClient`). OBS Studio 28.0+ 빌트인 WebSocket 포트 4455.
* **MCP 기반 방송 자동화:** Claude Desktop MCP 서버를 통해 방송 전 환경 셋업, 의존성 검사, 네트워크 포트 바인딩 확인을 자연어 명령으로 일괄 실행.

---

## 3. 세부 구현 및 최적화 전략 (Implementation & Optimization)

### 3.1 Python 기반 단일 웹캠 트래킹 데이터 처리
* 노트북 내장 웹캠의 단일 영상 스트림에서 `PoseLandmarker`와 `FaceLandmarker`를 동일 프레임에 대해 순차 실행하여 전신 포즈와 얼굴 표정 데이터를 동시에 추론.
* FaceLandmarker의 네이티브 ARKit 52 블렌드쉐이프 출력을 직접 활용하여 별도 변환 로직 없이 표정 데이터 획득.
* 비전 AI 연산 스레드와 UDP 네트워크 발신 스레드를 분리하여, 이미지 프로세싱이 네트워크 송신 속도에 병목을 일으키지 않도록 아키텍처 설계.
* `python-osc` 라이브러리 또는 Python `socket` 모듈을 활용한 효율적인 데이터 직렬화.

### 3.2 로우레벨 C++ 네트워크 최적화 (엔지니어링 코어)
* **Thread-safe Data Queue:** `FRunnable`을 상속받은 워커 스레드에서 패킷을 수신하고 `TQueue<T>` (Lock-free Queue)를 활용하여 게임 스레드와 데이터를 안전하게 교환.
* **Memory Pooling:** 초당 수십 번 갱신되는 트래킹 패킷으로 인한 메모리 단편화 및 가비지 컬렉션(GC) 스파이크를 방지하기 위해, 패킷 파싱용 C++ 구조체를 미리 할당(Pre-allocation)해두고 재사용.
* **Tick 오버헤드 통제:** 이벤트 기반(Event-driven) 아키텍처를 도입하여 매 프레임 Tick 함수 호출을 최소화하고 데이터 갱신 시점에만 렌더링 파이프라인에 개입.

### 3.3 실시간 렌더링 및 씬 최적화
* **Culling 및 LOD 최적화:** 버튜버 방송용 고정 뷰포트 외부의 렌더링 자원 차단, 과도한 텍스처 해상도 스트리밍 제한 설정.
* **Lumen 부하 제어:** 동적 글로벌 일루미네이션(Lumen)의 업데이트 빈도나 퀄리티 세팅을 CVar로 제어. 저사양 환경(RTX 3060 미만)에서는 Screen Space GI로 대체.
* **1PC 스트리밍 환경 최적화:** GPU 자원을 렌더링과 인코딩(OBS NVENC)에 분배하는 전략 필요. 렌더링 해상도와 인코딩 품질 간 트레이드오프 관리.

### 3.4 AI/MCP 기반 워크플로우 자동화
* **Claude Desktop + MCP 서버 활용:** 프로젝트 전용 MCP 서버를 구축하여 AI 어시스턴트가 다음 작업을 자동화:
  - 실행 환경 셋업 및 의존성 라이브러리 검사
  - 네트워크 포트 바인딩 확인 (UDP/OSC 포트, OBS WebSocket 4455)
  - UE Remote Control API를 통한 에디터 내 프로퍼티 조정
  - Python 스크립트 생성 및 트래킹 모듈 디버깅
* **Unreal MCP Server 통합:** 커뮤니티 오픈소스 Unreal MCP Server(예: `chongdashu/unreal-mcp`)를 활용하여 Claude/Cursor가 언리얼 에디터를 자연어로 제어.
* **Cursor/Gemini CLI 병행:** 복잡한 C++ 플러그인 코드와 Blueprint 로직을 AI 코딩 어시스턴트로 신속하게 생성 및 유지보수.

---

## 4. 포트폴리오 강조 포인트 (For VP Engineer Position)

본 포트폴리오는 외부 미들웨어(Unity 등)에 의존하지 않고 데이터를 엔진 레벨에서 직접 제어하는 **'로우레벨 시스템 통합 및 최적화 엔지니어'**로서의 문제 해결 능력을 증명합니다.

1.  **Architecture Design:** 단일 노트북 웹캠 영상에서 MediaPipe Tasks API로 전신/표정 데이터를 추출하고 언리얼 엔진으로 쏘아주는 순수 Python/C++ 기반의 가벼운(Lightweight) 커스텀 트래킹 파이프라인 구축.
2.  **Performance Profiling:** 하드웨어 자원이 제한된 1PC 스트리밍 환경에서 C++를 활용한 스레드 분산 및 메모리 재사용 기법으로 실시간 렌더링 성능 최적화 달성.
3.  **Modern AI Workflow:** MCP(Model Context Protocol)와 AI 코딩 어시스턴트를 적극 활용하여 파이프라인 자동화 및 복잡한 네트워크 통신(OSC/UDP) 모듈 구축 시간을 획기적으로 단축. AI-Augmented Development 역량 증명.

---

## 5. 단계별 개발 마일스톤 (Milestones, 6주)

* **Week 1:** 개발 환경 셋업 + MediaPipe Tasks API 기반 통합 트래킹 Python 프로토타입.
  - Python 3.12+ 환경 구성, MediaPipe Tasks API (`FaceLandmarker`, `PoseLandmarker`) 설치
  - 노트북 웹캠 캡처 → 단일 프레임 PoseLandmarker + FaceLandmarker 동시 추론 검증
  - ARKit 52 블렌드쉐이프 네이티브 출력 확인 및 UDP 직렬화 프로토타입

* **Week 2~3:** 언리얼 엔진 5.7 C++ UDP/OSC 커스텀 수신 컴포넌트 및 멀티스레딩 데이터 파싱 로직 개발.
  - UE 5.7 프로젝트 생성 및 C++ 플러그인 프로젝트 구조 셋업
  - `FRunnable` 기반 워커 스레드 + `TQueue<T>` 구현
  - `FUdpSocketReceiver` 기반 UDP 수신 및 구조체 파싱
  - MCP 서버 연동으로 AI 어시스턴트 활용한 C++ 코드 생성 가속

* **Week 4~5:** 수신된 전신/표정 데이터를 아바타에 직접 매핑하는 C++ 애니메이션 리타겟팅 로직 및 엔진 프로파일링 최적화.
  - `FAnimInstanceProxy` 기반 Bone Transform 실시간 매핑
  - Morph Target (ARKit 52 블렌드쉐이프) 실시간 반영
  - Unreal Insights 프로파일링 → 병목 식별 및 최적화
  - Lumen/SSGI CVar 튜닝, 드로우콜 최적화

* **Week 6:** OBS WebSocket 연동, AI 기반 원클릭 실행 런처 자동화 스크립트 작성 및 치지직 실전 송출 부하 테스트.
  - `obsws-python` 기반 OBS 씬 전환/방송 시작 자동화
  - 전체 파이프라인 원클릭 런처 Python 스크립트
  - 치지직 RTMP 실전 송출 30분+ 안정성 테스트
  - 최종 문서화 및 포트폴리오 정리