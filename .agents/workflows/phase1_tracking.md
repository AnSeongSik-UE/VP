---
description: "Phase 1: MediaPipe 트래킹 + UDP 네트워크 발신 모듈 (Week 1)"
---

# Phase 1: MediaPipe Tracking 모듈 (Week 1)

## 1-1. MediaPipe Task 모델 다운로드

```bash
mkdir models
# FaceLandmarker 모델 (ARKit 52 블렌드쉐이프 포함)
curl -L -o models/face_landmarker.task \
  https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/latest/face_landmarker.task

# PoseLandmarker Heavy 모델 (33 랜드마크, 정확도 우선)
curl -L -o models/pose_landmarker_heavy.task \
  https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_heavy/float16/latest/pose_landmarker_heavy.task
```

## 1-2. 단일 웹캠 통합 트래킹 구현

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
                    except Empty:
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
        except Empty:
            return None
```

## 1-3. UDP 네트워크 발신 모듈

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

## 1-4. Week 1 검증 체크리스트

```
[ ] 웹캠 캡처 정상 작동 (720p, 30fps+)
[ ] FaceLandmarker ARKit 52 블렌드쉐이프 출력 확인 (console log)
[ ] PoseLandmarker 33 랜드마크 출력 확인
[ ] 동시 추론 시 프레임레이트 20fps 이상 유지
[ ] UDP 패킷 발신 확인 (Wireshark 또는 netcat으로 수신 테스트)
```

**AI 자동화 활용 포인트:**
- `Claude Desktop + MCP`: "웹캠 열기 → FaceLandmarker/PoseLandmarker 초기화 → 추론 → 결과 출력" 전 과정을 자연어로 요청하여 프로토타입 빠르게 생성
- `Antigravity`: tracker.py / sender.py 코드 작성 및 디버깅
- `MCP check_webcam 도구`: 웹캠 접근 가능 여부 사전 진단
