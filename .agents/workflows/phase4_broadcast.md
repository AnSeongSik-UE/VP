---
description: "Phase 4: 방송 자동화 + 실전 테스트 + 트러블슈팅 (Week 6)"
---

# Phase 4: 방송 자동화 + 실전 테스트 (Week 6)

## 4-1. OBS WebSocket 자동화 스크립트

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
        print(f"[OBS] Available sources: {[s['inputName'] for s in sources.inputs]}")
        
        # 씬 전환
        self.client.set_current_program_scene('VP_Main')
        print("[OBS] Switched to VP_Main scene")
    
    def start_streaming(self):
        """치지직 RTMP 스트리밍 시작"""
        self.client.start_stream()
        print("[STREAM] Streaming started (CHZZK RTMP)")
    
    def stop_streaming(self):
        """스트리밍 중지"""
        self.client.stop_stream()
        print("[STREAM] Streaming stopped")
    
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

## 4-2. 원클릭 파이프라인 런처

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
            status = "[OK]" if ok else "[FAIL]"
            print(f"  {status} {name}: {msg}")
            if not ok:
                all_ok = False
        
        return all_ok
    
    def launch(self):
        """전체 파이프라인 순차 실행"""
        print("[*] VP Pipeline Launcher")
        print("=" * 50)
        
        # 1. 사전 점검
        print("\n[*] Preflight Check...")
        if not self.preflight_check():
            print("\n[FAIL] Preflight failed. Fix issues above and retry.")
            return
        
        print("\n[OK] All checks passed!")
        
        # 2. Unreal Engine 실행 (백그라운드)
        print("\n[UE] Launching Unreal Engine...")
        self.processes['ue'] = subprocess.Popen([
            r"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe",
            r"C:\UnrealWork\VP\VPPipeline\VPPipeline.uproject",
            "-game", "-windowed", "-ResX=1920", "-ResY=1080"
        ])
        time.sleep(10)  # UE 로딩 대기
        
        # 3. 트래킹 모듈 실행
        print("\n[TRACK] Launching Tracker...")
        self.processes['tracker'] = subprocess.Popen(
            [sys.executable, "sender.py"],
            cwd=os.path.join(os.path.dirname(__file__), "vp-tracker")
        )
        time.sleep(3)
        
        # 4. OBS 씬 전환 및 스트리밍 준비
        print("\n[OBS] Configuring OBS...")
        from obs_controller import OBSController
        obs_ctrl = OBSController()
        obs_ctrl.setup_streaming_scene()
        
        print("\n[OK] Pipeline Ready!")
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
        print("\n[*] Shutting down...")
        obs_ctrl.stop_streaming()
        obs_ctrl.disconnect()
        for name, proc in self.processes.items():
            proc.terminate()
        print("[*] Pipeline stopped")
    
    def _check_webcam(self):
        try:
            import cv2
            cap = cv2.VideoCapture(0)
            ok = cap.isOpened()
            cap.release()
            return (ok, "Available" if ok else "Not found")
        except (ImportError, cv2.error) as e:
            return (False, f"OpenCV error: {e}")
    
    def _check_port(self, port):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.bind(('127.0.0.1', port))
            s.close()
            return (True, "Available")
        except OSError:
            return (False, "In use")
    
    def _check_obs(self):
        try:
            import obsws_python as obs
            c = obs.ReqClient(host='localhost', port=4455, password='', timeout=2)
            c.disconnect()
            return (True, "Connected")
        except ConnectionRefusedError:
            return (False, "OBS not running")
        except Exception as e:
            return (False, f"Connection failed: {type(e).__name__}")
    
    def _check_models(self):
        models = ["models/face_landmarker.task", "models/pose_landmarker_heavy.task"]
        missing = [m for m in models if not os.path.exists(m)]
        if not missing:
            return (True, "All models found")
        return (False, f"Missing: {', '.join(missing)}")

if __name__ == "__main__":
    PipelineLauncher().launch()
```

## 4-3. MCP 서버를 통한 런타임 제어

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

## 4-4. Week 6 검증 체크리스트 (최종)

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
