---
description: UE5.7 버추얼 프로덕션 파이프라인 전체 워크플로우 (MCP/AI 자동화 포함)
---

# 📋 VP Pipeline 개발 워크플로우

> 이 문서는 guide.md의 실현가능성 검토 결과를 반영한 **구체적 실행 워크플로우**입니다.
> 각 단계마다 AI 자동화(MCP, Claude, Cursor)를 적극 활용하여 개발 속도를 극대화합니다.

---

## Phase 0: 개발 환경 셋업 (Day 1~2)

### 0-1. Python 환경 구성

```bash
# uv로 프로젝트 초기화 (의존성 관리 + 가상환경)
uv init vp-tracker
cd vp-tracker
uv add mediapipe opencv-python python-osc obsws-python numpy
```

**필수 패키지 목록:**
| 패키지 | 버전 | 용도 |
|--------|------|------|
| `mediapipe` | 0.10.x+ | FaceLandmarker + PoseLandmarker |
| `opencv-python` | 4.x | 웹캠 캡처 및 프레임 처리 |
| `python-osc` | 1.8+ | OSC 프로토콜 데이터 직렬화 |
| `obsws-python` | 1.x | OBS WebSocket v5 제어 |
| `numpy` | 1.x | 수치 연산 |

검증 기준:
```python
# 이 스크립트가 에러 없이 실행되면 환경 셋업 완료
import mediapipe as mp
import cv2
import numpy as np
from pythonosc import udp_client
import obsws_python as obs

print(f"MediaPipe: {mp.__version__}")
print(f"OpenCV: {cv2.__version__}")
print("✅ All dependencies verified")
```

### 0-2. Unreal Engine 5.7 프로젝트 구성

1. Epic Games Launcher에서 UE 5.7 설치 확인
2. 새 C++ 프로젝트 생성: `File > New Project > Games > Blank > C++`
   - 프로젝트명: `VPPipeline`
   - 경로: `c:\UnrealWork\VP\VPPipeline`
3. 필수 플러그인 활성화:
   - `Remote Control API` (MCP 연동용)
   - `Python Editor Script Plugin` (UE Python 스크립팅)
   - `OSC` (빌트인 OSC 플러그인, 참고용)
4. Project Settings > Python > `Allow Python Remote Execution` 활성화

### 0-3. MCP 서버 셋업 (AI 자동화 핵심)

#### Claude Desktop 연동용 MCP 서버 구성

```bash
# Unreal MCP Server 클론
git clone https://github.com/chongdashu/unreal-mcp.git
cd unreal-mcp
uv sync
```

#### `claude_desktop_config.json` 설정

```json
{
  "mcpServers": {
    "unreal-mcp": {
      "command": "uv",
      "args": ["--directory", "C:/path/to/unreal-mcp", "run", "main.py"],
      "env": {}
    },
    "vp-pipeline": {
      "command": "uv",
      "args": ["--directory", "C:/UnrealWork/VP/vp-tracker", "run", "mcp_server.py"],
      "env": {}
    }
  }
}
```

#### 프로젝트 전용 MCP 서버 (vp-tracker/mcp_server.py)

```python
"""VP Pipeline 전용 MCP 서버 - AI 어시스턴트가 파이프라인 상태를 진단/제어"""
from mcp.server.fastmcp import FastMCP
import socket
import subprocess
import json

mcp = FastMCP("VP Pipeline Tools")

@mcp.tool()
def check_port_available(port: int) -> str:
    """UDP/OSC 포트가 사용 가능한지 확인"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind(('127.0.0.1', port))
        s.close()
        return f"✅ Port {port} is available"
    except OSError:
        return f"❌ Port {port} is already in use"

@mcp.tool()
def check_webcam() -> str:
    """노트북 웹캠 접근 가능 여부 확인"""
    import cv2
    cap = cv2.VideoCapture(0)
    if cap.isOpened():
        ret, frame = cap.read()
        h, w = frame.shape[:2] if ret else (0, 0)
        cap.release()
        return f"✅ Webcam OK - Resolution: {w}x{h}"
    return "❌ Webcam not accessible"

@mcp.tool()
def check_obs_connection(port: int = 4455, password: str = "") -> str:
    """OBS WebSocket 연결 상태 확인"""
    try:
        import obsws_python as obs
        client = obs.ReqClient(host='localhost', port=port, password=password, timeout=3)
        ver = client.get_version()
        client.disconnect()
        return f"✅ OBS Connected - Version: {ver.obs_version}"
    except Exception as e:
        return f"❌ OBS Connection Failed: {str(e)}"

@mcp.tool()
def check_mediapipe_models() -> str:
    """MediaPipe Task 모델 파일 존재 여부 확인"""
    import os
    models = {
        "face_landmarker.task": "FaceLandmarker",
        "pose_landmarker_heavy.task": "PoseLandmarker"
    }
    results = []
    for filename, name in models.items():
        path = os.path.join("models", filename)
        if os.path.exists(path):
            size_mb = os.path.getsize(path) / (1024*1024)
            results.append(f"✅ {name}: {path} ({size_mb:.1f}MB)")
        else:
            results.append(f"❌ {name}: {path} NOT FOUND")
    return "\n".join(results)

@mcp.tool()
def run_pipeline_diagnostics() -> str:
    """전체 파이프라인 사전 진단 (원클릭)"""
    checks = []
    checks.append(check_webcam())
    checks.append(check_port_available(7000))   # UDP 트래킹 포트
    checks.append(check_port_available(4455))   # OBS WebSocket 포트
    checks.append(check_mediapipe_models())
    return "\n".join(["=== Pipeline Diagnostics ==="] + checks)

if __name__ == "__main__":
    mcp.run()
```

검증:
```bash
# MCP Inspector로 도구 등록 확인
npx @modelcontextprotocol/inspector uv run mcp_server.py
```

---

## Phase 1: MediaPipe Tracking 모듈 (Week 1)

### 1-1. MediaPipe Task 모델 다운로드

```bash
mkdir models
# FaceLandmarker 모델 (ARKit 52 블렌드쉐이프 포함)
curl -L -o models/face_landmarker.task \
  https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/latest/face_landmarker.task

# PoseLandmarker Heavy 모델 (33 랜드마크, 정확도 우선)
curl -L -o models/pose_landmarker_heavy.task \
  https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_heavy/float16/latest/pose_landmarker_heavy.task
```

### 1-2. 단일 웹캠 통합 트래킹 구현

**핵심 파일: `tracker.py`**

```python
"""
VP Pipeline - Unified Webcam Tracker
MediaPipe Tasks API (FaceLandmarker + PoseLandmarker)를 동일 프레임에서 순차 실행
"""
import mediapipe as mp
import cv2
import time
import threading
from dataclasses import dataclass
from typing import Optional
from queue import Queue

@dataclass
class TrackingFrame:
    """하나의 프레임에서 추출된 전체 트래킹 데이터"""
    timestamp: float
    # Face
    blendshapes: dict[str, float]  # ARKit 52 블렌드쉐이프 {name: weight}
    # Pose
    pose_landmarks: list[tuple[float, float, float]]  # 33개 (x, y, z)
    
BaseOptions = mp.tasks.BaseOptions
FaceLandmarker = mp.tasks.vision.FaceLandmarker
FaceLandmarkerOptions = mp.tasks.vision.FaceLandmarkerOptions
PoseLandmarker = mp.tasks.vision.PoseLandmarker
PoseLandmarkerOptions = mp.tasks.vision.PoseLandmarkerOptions
VisionRunningMode = mp.tasks.vision.RunningMode

class UnifiedTracker:
    def __init__(self, camera_id: int = 0):
        self.camera_id = camera_id
        self.data_queue: Queue[TrackingFrame] = Queue(maxsize=2)
        self.running = False
        
        # FaceLandmarker 설정
        self.face_options = FaceLandmarkerOptions(
            base_options=BaseOptions(model_asset_path='models/face_landmarker.task'),
            running_mode=VisionRunningMode.VIDEO,
            output_face_blendshapes=True,
            num_faces=1
        )
        
        # PoseLandmarker 설정
        self.pose_options = PoseLandmarkerOptions(
            base_options=BaseOptions(model_asset_path='models/pose_landmarker_heavy.task'),
            running_mode=VisionRunningMode.VIDEO,
            num_poses=1
        )
    
    def start(self):
        self.running = True
        self.thread = threading.Thread(target=self._tracking_loop, daemon=True)
        self.thread.start()
    
    def stop(self):
        self.running = False
        self.thread.join(timeout=3)
    
    def _tracking_loop(self):
        cap = cv2.VideoCapture(self.camera_id)
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
        
        with FaceLandmarker.create_from_options(self.face_options) as face_lm, \
             PoseLandmarker.create_from_options(self.pose_options) as pose_lm:
            
            frame_idx = 0
            while self.running and cap.isOpened():
                ret, frame = cap.read()
                if not ret:
                    continue
                
                timestamp_ms = int(time.time() * 1000)
                mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=frame)
                
                # 순차 추론 (동일 프레임)
                face_result = face_lm.detect_for_video(mp_image, timestamp_ms)
                pose_result = pose_lm.detect_for_video(mp_image, timestamp_ms)
                
                tracking_frame = self._build_tracking_frame(
                    timestamp_ms, face_result, pose_result
                )
                
                # 큐가 가득 차면 오래된 데이터 버림 (최신 우선)
                if self.data_queue.full():
                    try:
                        self.data_queue.get_nowait()
                    except:
                        pass
                self.data_queue.put(tracking_frame)
                
                frame_idx += 1
        
        cap.release()
    
    def _build_tracking_frame(self, timestamp, face_result, pose_result) -> TrackingFrame:
        # 블렌드쉐이프 추출
        blendshapes = {}
        if face_result.face_blendshapes:
            for bs in face_result.face_blendshapes[0]:
                blendshapes[bs.category_name] = bs.score
        
        # 포즈 랜드마크 추출
        pose_landmarks = []
        if pose_result.pose_landmarks:
            for lm in pose_result.pose_landmarks[0]:
                pose_landmarks.append((lm.x, lm.y, lm.z))
        
        return TrackingFrame(
            timestamp=timestamp,
            blendshapes=blendshapes,
            pose_landmarks=pose_landmarks
        )
    
    def get_latest(self) -> Optional[TrackingFrame]:
        try:
            return self.data_queue.get_nowait()
        except:
            return None
```

### 1-3. UDP 네트워크 발신 모듈

**핵심 파일: `sender.py`**

```python
"""
UDP/OSC 네트워크 발신 - 트래킹 데이터를 언리얼 엔진으로 전송
"""
import struct
import json
import threading
import time
from pythonosc import udp_client
from tracker import UnifiedTracker, TrackingFrame

class OSCSender:
    """python-osc를 활용한 OSC 프로토콜 전송"""
    
    def __init__(self, host: str = "127.0.0.1", port: int = 7000):
        self.client = udp_client.SimpleUDPClient(host, port)
        self.running = False
    
    def send_tracking_frame(self, frame: TrackingFrame):
        # 블렌드쉐이프 전송 (52개 float)
        for name, value in frame.blendshapes.items():
            self.client.send_message(f"/face/{name}", value)
        
        # 포즈 랜드마크 전송 (33 x 3 = 99 floats)
        for i, (x, y, z) in enumerate(frame.pose_landmarks):
            self.client.send_message(f"/pose/{i}", [x, y, z])


class RawUDPSender:
    """고성능 바이너리 UDP 전송 (OSC 오버헤드 없음)"""
    
    HEADER = b'VPFR'  # VP Frame 매직 바이트
    
    def __init__(self, host: str = "127.0.0.1", port: int = 7000):
        import socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.target = (host, port)
    
    def send_tracking_frame(self, frame: TrackingFrame):
        # 패킷 포맷: [HEADER 4B][TIMESTAMP 8B][NUM_BS 2B][BS_DATA...][NUM_POSE 2B][POSE_DATA...]
        data = bytearray(self.HEADER)
        data += struct.pack('<d', frame.timestamp)
        
        # 블렌드쉐이프 (이름 고정 순서, float만 전송)
        bs_values = list(frame.blendshapes.values())
        data += struct.pack('<H', len(bs_values))
        for v in bs_values:
            data += struct.pack('<f', v)
        
        # 포즈 랜드마크
        data += struct.pack('<H', len(frame.pose_landmarks))
        for x, y, z in frame.pose_landmarks:
            data += struct.pack('<fff', x, y, z)
        
        self.sock.sendto(bytes(data), self.target)


def main():
    """트래킹 + 네트워크 발신 통합 실행"""
    tracker = UnifiedTracker(camera_id=0)
    sender = RawUDPSender(host="127.0.0.1", port=7000)
    # OSC가 필요하면: sender = OSCSender(host="127.0.0.1", port=7000)
    
    tracker.start()
    print("🎥 Tracking started. Sending to UE at 127.0.0.1:7000")
    
    try:
        while True:
            frame = tracker.get_latest()
            if frame:
                sender.send_tracking_frame(frame)
            time.sleep(1/60)  # ~60Hz
    except KeyboardInterrupt:
        tracker.stop()
        print("⏹ Tracking stopped")

if __name__ == "__main__":
    main()
```

### 1-4. Week 1 검증 체크리스트

```
[ ] 웹캠 캡처 정상 작동 (720p, 30fps+)
[ ] FaceLandmarker ARKit 52 블렌드쉐이프 출력 확인 (console log)
[ ] PoseLandmarker 33 랜드마크 출력 확인
[ ] 동시 추론 시 프레임레이트 20fps 이상 유지
[ ] UDP 패킷 발신 확인 (Wireshark 또는 netcat으로 수신 테스트)
```

**AI 자동화 활용 포인트:**
- `Claude Desktop + MCP`: "웹캠 열기 → FaceLandmarker/PoseLandmarker 초기화 → 추론 → 결과 출력" 전 과정을 자연어로 요청하여 프로토타입 빠르게 생성
- `Cursor`: tracker.py / sender.py 코드 작성 및 디버깅
- `MCP check_webcam 도구`: 웹캠 접근 가능 여부 사전 진단

---

## Phase 2: UE5.7 C++ 수신 플러그인 (Week 2~3)

### 2-1. C++ 플러그인 프로젝트 구조

```
VPPipeline/
├── Plugins/
│   └── VPTrackerReceiver/
│       ├── VPTrackerReceiver.uplugin
│       ├── Source/
│       │   └── VPTrackerReceiver/
│       │       ├── VPTrackerReceiver.Build.cs
│       │       ├── Public/
│       │       │   ├── VPTrackerReceiver.h        // 모듈 헤더
│       │       │   ├── VPUDPReceiver.h             // UDP 수신 컴포넌트
│       │       │   ├── VPTrackingData.h            // 트래킹 데이터 구조체
│       │       │   └── VPAnimInstance.h             // 커스텀 AnimInstance
│       │       └── Private/
│       │           ├── VPTrackerReceiver.cpp
│       │           ├── VPUDPReceiver.cpp
│       │           ├── VPTrackingData.cpp
│       │           └── VPAnimInstance.cpp
```

### 2-2. 핵심 C++ 구현 가이드

#### 트래킹 데이터 구조체 (`VPTrackingData.h`)

```cpp
#pragma once
#include "CoreMinimal.h"
#include "VPTrackingData.generated.h"

USTRUCT(BlueprintType)
struct FVPBlendshapeData
{
    GENERATED_BODY()
    
    // ARKit 52 블렌드쉐이프 가중치 (0.0 ~ 1.0)
    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> Blendshapes;
};

USTRUCT(BlueprintType)
struct FVPPoseLandmark
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    FVector Position;  // Normalized (0~1)
    
    UPROPERTY(BlueprintReadOnly)
    float Visibility;
};

USTRUCT(BlueprintType)
struct FVPTrackingFrame
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    double Timestamp;
    
    UPROPERTY(BlueprintReadOnly)
    FVPBlendshapeData FaceData;
    
    UPROPERTY(BlueprintReadOnly)
    TArray<FVPPoseLandmark> PoseLandmarks;  // 33개
    
    UPROPERTY(BlueprintReadOnly)
    bool bIsValid = false;
};
```

#### UDP 수신 컴포넌트 (`VPUDPReceiver.h`)

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Common/UdpSocketReceiver.h"
#include "VPTrackingData.h"
#include "VPUDPReceiver.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrackingDataReceived, const FVPTrackingFrame&, Data);

UCLASS(ClassGroup=(VPPipeline), meta=(BlueprintSpawnableComponent))
class VPTRACKERRECEIVER_API UVPUDPReceiver : public UActorComponent
{
    GENERATED_BODY()
    
public:
    UVPUDPReceiver();
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VP Pipeline")
    int32 ListenPort = 7000;
    
    UPROPERTY(BlueprintAssignable, Category="VP Pipeline")
    FOnTrackingDataReceived OnTrackingDataReceived;
    
    UFUNCTION(BlueprintCallable, Category="VP Pipeline")
    FVPTrackingFrame GetLatestTrackingData() const;
    
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

private:
    FSocket* Socket;
    FUdpSocketReceiver* UDPReceiver;
    
    // Thread-safe 데이터 큐
    TQueue<FVPTrackingFrame, EQueueMode::Spsc> DataQueue;
    
    // 게임 스레드에서 사용할 최신 데이터
    FVPTrackingFrame LatestFrame;
    
    // 메모리 풀 (Pre-allocated)
    FVPTrackingFrame ParseBuffer;
    
    void OnDataReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);
    FVPTrackingFrame ParsePacket(const FArrayReaderPtr& Data);
};
```

### 2-3. Build.cs 의존성

```csharp
// VPTrackerReceiver.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "Sockets",
    "Networking",
    "AnimGraphRuntime"
});
```

### 2-4. Week 2~3 검증 체크리스트

```
[ ] 플러그인 컴파일 성공 (UE 에디터 로드)
[ ] UDP 포트 7000 바인딩 성공
[ ] Python 트래커에서 발신한 패킷 수신 확인 (UE_LOG)
[ ] 패킷 파싱 → FVPTrackingFrame 구조체 정상 변환
[ ] TQueue 기반 게임 스레드 전달 → 프레임 드랍 없음 확인
[ ] Unreal Insights에서 네트워크 스레드 CPU 부하 확인
```

**AI 자동화 활용 포인트:**
- `Unreal MCP Server`: Claude에게 "UDP 수신 컴포넌트를 레벨에 배치해줘" → 자연어로 에디터 제어
- `Cursor`: C++ 플러그인 코드 생성 (USTRUCT, UCLASS 보일러플레이트 자동 생성)
- `MCP check_port_available`: 포트 충돌 사전 진단
- `Claude Desktop`: "FRunnable 워커 스레드 패턴으로 UDP 수신기 코드 작성해줘" → 복잡한 멀티스레딩 코드 AI 생성

---

## Phase 3: 애니메이션 리타겟팅 + 프로파일링 (Week 4~5)

### 3-1. 커스텀 AnimInstance

```cpp
// VPAnimInstance.h - 수신된 트래킹 데이터를 아바타에 매핑
UCLASS()
class UVPAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
    
public:
    // 블렌드쉐이프 → Morph Target 매핑
    UPROPERTY(BlueprintReadWrite, Category="VP Face")
    TMap<FName, float> FaceBlendshapes;
    
    // 포즈 → Bone Transform 매핑
    UPROPERTY(BlueprintReadWrite, Category="VP Pose")
    TArray<FTransform> PoseBoneTransforms;
    
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    
    UFUNCTION(BlueprintCallable, Category="VP Pipeline")
    void ApplyTrackingData(const FVPTrackingFrame& Frame);
};
```

### 3-2. 블렌드쉐이프 → Morph Target 매핑 전략

| ARKit 블렌드쉐이프 | UE Morph Target (예시) | 설명 |
|---------------------|----------------------|------|
| `eyeBlinkLeft` | `EyeBlink_L` | 왼쪽 눈 깜빡임 |
| `eyeBlinkRight` | `EyeBlink_R` | 오른쪽 눈 깜빡임 |
| `mouthSmileLeft` | `MouthSmile_L` | 왼쪽 입꼬리 올림 |
| `jawOpen` | `JawOpen` | 입 벌리기 |
| ... (52개 전체) | ... | 아바타 모델에 맞춰 매핑 |

> **중요:** 아바타 모델의 Morph Target 이름이 ARKit 표준과 다를 수 있음. 매핑 테이블을 DataTable로 관리하여 런타임 교체 가능하게 설계.

### 3-3. Unreal Insights 프로파일링 워크플로우

```bash
# 1. Unreal Insights 트레이싱 활성화 후 에디터 실행
UnrealEditor.exe VPPipeline.uproject -trace=cpu,frame,bookmark

# 2. 트레이싱 데이터 분석
UnrealInsights.exe
```

**프로파일링 체크포인트:**
| 지표 | 목표값 | 초과 시 대응 |
|------|--------|-------------|
| Frame Time | < 16.6ms (60fps) | 드로우콜 감소, LOD 조정 |
| Game Thread | < 8ms | Tick 로직 최적화 |
| Render Thread | < 10ms | 머티리얼 복잡도 감소 |
| GPU Time | < 14ms | Lumen → SSGI 전환 |
| Network Parse (커스텀) | < 0.5ms | 메모리 풀 확인 |

### 3-4. CVar 성능 튜닝 명령어

```
// Lumen 품질 조절
r.Lumen.DiffuseIndirect.Allow 0          // Lumen GI 비활성화
r.Lumen.Reflections.Allow 0               // Lumen 리플렉션 비활성화

// Screen Space 대체
r.ScreenSpaceReflections.Quality 2        // SSGR 품질
r.AmbientOcclusion.Method 1              // SSAO 활성화

// 텍스처 스트리밍
r.Streaming.PoolSize 512                  // 텍스처 풀 512MB 제한
r.Streaming.LimitPoolSizeToVRAM 1         // VRAM 기반 자동 제한

// 렌더링 해상도
r.ScreenPercentage 80                     // 80% 해상도 렌더링 (업스케일)
```

### 3-5. Week 4~5 검증 체크리스트

```
[ ] 아바타 Morph Target에 블렌드쉐이프 실시간 반영 (표정 변화 확인)
[ ] Bone Transform 매핑으로 전신 움직임 반영
[ ] 60fps 안정 유지 (Unreal Insights 확인)
[ ] Game Thread < 8ms
[ ] 메모리 풀링 효과 확인 (GC 스파이크 없음)
[ ] Lumen/SSGI 전환 시 시각 품질 비교
```

**AI 자동화 활용 포인트:**
- `Unreal MCP Server`: "현재 씬의 머티리얼 복잡도 확인해줘" → 에디터 내 조회
- `Claude Desktop`: "ARKit 블렌드쉐이프 52개 전체 목록과 UE Morph Target 매핑 테이블 만들어줘"
- `Cursor`: AnimInstance 코드 작성 및 리팩터링

---

## Phase 4: 방송 자동화 + 실전 테스트 (Week 6)

### 4-1. OBS WebSocket 자동화 스크립트

**핵심 파일: `obs_controller.py`**

```python
"""OBS Studio 자동 제어 - obsws-python (WebSocket v5)"""
import obsws_python as obs
import time

class OBSController:
    def __init__(self, host='localhost', port=4455, password=''):
        self.client = obs.ReqClient(host=host, port=port, password=password, timeout=5)
    
    def setup_streaming_scene(self):
        """방송용 씬 자동 구성"""
        # UE 게임 캡처 소스 확인
        sources = self.client.get_input_list()
        print(f"📺 Available sources: {[s['inputName'] for s in sources.inputs]}")
        
        # 씬 전환
        self.client.set_current_program_scene('VP_Main')
        print("🎬 Switched to VP_Main scene")
    
    def start_streaming(self):
        """치지직 RTMP 스트리밍 시작"""
        self.client.start_stream()
        print("🔴 Streaming started (CHZZK RTMP)")
    
    def stop_streaming(self):
        """스트리밍 중지"""
        self.client.stop_stream()
        print("⏹ Streaming stopped")
    
    def get_status(self) -> dict:
        """방송 상태 조회"""
        status = self.client.get_stream_status()
        return {
            "active": status.output_active,
            "duration": status.output_duration if status.output_active else 0,
            "bytes": status.output_bytes if status.output_active else 0
        }
    
    def disconnect(self):
        self.client.disconnect()
```

### 4-2. 원클릭 파이프라인 런처

**핵심 파일: `launcher.py`**

```python
"""
VP Pipeline 원클릭 런처
모든 프로세스를 순서대로 실행하고 상태를 모니터링
"""
import subprocess
import time
import sys
import os
import socket

class PipelineLauncher:
    def __init__(self):
        self.processes = {}
    
    def preflight_check(self) -> bool:
        """실행 전 사전 점검"""
        checks = {
            "웹캠 접근": self._check_webcam(),
            "UDP 포트 7000": self._check_port(7000),
            "OBS WebSocket 4455": self._check_obs(),
            "MediaPipe 모델": self._check_models(),
        }
        
        all_ok = True
        for name, (ok, msg) in checks.items():
            status = "✅" if ok else "❌"
            print(f"  {status} {name}: {msg}")
            if not ok:
                all_ok = False
        
        return all_ok
    
    def launch(self):
        """전체 파이프라인 순차 실행"""
        print("🚀 VP Pipeline Launcher")
        print("=" * 50)
        
        # 1. 사전 점검
        print("\n📋 Preflight Check...")
        if not self.preflight_check():
            print("\n❌ Preflight failed. Fix issues above and retry.")
            return
        
        print("\n✅ All checks passed!")
        
        # 2. Unreal Engine 실행 (백그라운드)
        print("\n🎮 Launching Unreal Engine...")
        self.processes['ue'] = subprocess.Popen([
            r"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe",
            r"C:\UnrealWork\VP\VPPipeline\VPPipeline.uproject",
            "-game", "-windowed", "-ResX=1920", "-ResY=1080"
        ])
        time.sleep(10)  # UE 로딩 대기
        
        # 3. 트래킹 모듈 실행
        print("\n🎥 Launching Tracker...")
        self.processes['tracker'] = subprocess.Popen(
            [sys.executable, "sender.py"],
            cwd=os.path.join(os.path.dirname(__file__), "vp-tracker")
        )
        time.sleep(3)
        
        # 4. OBS 씬 전환 및 스트리밍 준비
        print("\n📺 Configuring OBS...")
        from obs_controller import OBSController
        obs_ctrl = OBSController()
        obs_ctrl.setup_streaming_scene()
        
        print("\n✅ Pipeline Ready!")
        print("   - Press 'S' to start streaming")
        print("   - Press 'Q' to quit")
        
        # 5. 모니터링 루프
        try:
            while True:
                cmd = input("> ").strip().upper()
                if cmd == 'S':
                    obs_ctrl.start_streaming()
                elif cmd == 'Q':
                    break
                elif cmd == 'STATUS':
                    print(obs_ctrl.get_status())
        except KeyboardInterrupt:
            pass
        
        # 6. 정리
        print("\n🧹 Shutting down...")
        obs_ctrl.stop_streaming()
        obs_ctrl.disconnect()
        for name, proc in self.processes.items():
            proc.terminate()
        print("👋 Pipeline stopped")
    
    def _check_webcam(self):
        try:
            import cv2
            cap = cv2.VideoCapture(0)
            ok = cap.isOpened()
            cap.release()
            return (ok, "Available" if ok else "Not found")
        except:
            return (False, "OpenCV error")
    
    def _check_port(self, port):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.bind(('127.0.0.1', port))
            s.close()
            return (True, "Available")
        except:
            return (False, "In use")
    
    def _check_obs(self):
        try:
            import obsws_python as obs
            c = obs.ReqClient(host='localhost', port=4455, password='', timeout=2)
            c.disconnect()
            return (True, "Connected")
        except:
            return (False, "Not running or wrong password")
    
    def _check_models(self):
        models = ["models/face_landmarker.task", "models/pose_landmarker_heavy.task"]
        missing = [m for m in models if not os.path.exists(m)]
        if not missing:
            return (True, "All models found")
        return (False, f"Missing: {', '.join(missing)}")

if __name__ == "__main__":
    PipelineLauncher().launch()
```

### 4-3. MCP 서버를 통한 런타임 제어

Claude Desktop에서 다음과 같이 자연어로 파이프라인 제어 가능:

```
사용자: "파이프라인 상태 진단해줘"
→ MCP 도구 run_pipeline_diagnostics() 호출
→ 웹캠, 포트, 모델, OBS 상태 일괄 보고

사용자: "UDP 포트 7000이 사용 중인지 확인해줘"
→ MCP 도구 check_port_available(7000) 호출

사용자: "OBS 연결 상태 확인"
→ MCP 도구 check_obs_connection() 호출

사용자: "언리얼 에디터에서 VP_Avatar 액터의 위치를 (0, 0, 100)으로 옮겨줘"
→ Unreal MCP Server를 통해 원격 제어
```

### 4-4. Week 6 검증 체크리스트 (최종)

```
[ ] 원클릭 런처로 전체 파이프라인 실행 성공
[ ] OBS WebSocket으로 씬 전환 자동화 확인
[ ] 치지직 RTMP 실전 송출 성공
[ ] 30분+ 연속 방송 안정성 테스트 통과 (프레임 드랍 < 1%)
[ ] MCP 도구로 자연어 파이프라인 진단 성공
[ ] 전체 파이프라인 레이턴시 < 100ms (카메라→아바타)
[ ] CPU 사용률 80% 미만 안정 유지
[ ] GPU 사용률 90% 미만 안정 유지
```

---

## 🔧 트러블슈팅 가이드

### 자주 발생하는 문제

| 문제 | 원인 | 해결 |
|------|------|------|
| MediaPipe 모델 로드 실패 | `.task` 파일 경로 오류 | `check_mediapipe_models()` MCP 도구로 확인 |
| UDP 패킷 미수신 | 방화벽 차단 | Windows 방화벽에 Python/UE 예외 추가 |
| UE 프레임 드랍 | Lumen GPU 과부하 | CVar로 Lumen 비활성화 또는 SSGI 전환 |
| OBS 연결 실패 | WebSocket 미활성화 | OBS > Tools > WebSocket Server Settings 활성화 |
| 아바타 표정 매핑 오류 | Morph Target 이름 불일치 | 매핑 DataTable 확인 및 수정 |
| 1PC 인코딩 병목 | GPU 공유 부하 | OBS에서 NVENC 대신 x264(CPU) 시도 |
| 트래킹 FPS 저하 | CPU 과부하 | PoseLandmarker를 Lite 모델로 교체 |

### 성능 최적화 우선순위

```
1. [높음] Lumen → Screen Space GI 전환
2. [높음] 텍스처 스트리밍 풀 제한 (512MB)
3. [중간] r.ScreenPercentage 80% + TSR 업스케일
4. [중간] PoseLandmarker Heavy → Full 모델 다운그레이드
5. [낮음] 렌더링 해상도 1080p → 720p
```

---

## 📁 최종 프로젝트 구조

```
c:\UnrealWork\VP\
├── guide.md                         # 프로젝트 가이드 문서
├── taskReport.md                    # 작업 이력 보고서
├── .agents/
│   └── workflows/
│       └── workflow.md              # 이 문서
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
| **Cursor/Gemini CLI** | 코드 편집, 리팩터링, 문서 생성 | 전 과정 |
