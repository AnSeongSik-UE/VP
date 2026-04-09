---
description: "Phase 0: 개발 환경 셋업 - Python, UE5.7, MCP 서버 구성 (Day 1~2)"
---

# Phase 0: 개발 환경 셋업 (Day 1~2)

## 0-1. Python 환경 구성

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

## 0-2. Unreal Engine 5.7 프로젝트 구성

1. Epic Games Launcher에서 UE 5.7 설치 확인
2. 새 C++ 프로젝트 생성: `File > New Project > Games > Blank > C++`
   - 프로젝트명: `VPPipeline`
   - 경로: `c:\UnrealWork\VP\VPPipeline`
3. 필수 플러그인 활성화:
   - `Remote Control API` (MCP 연동용)
   - `Python Editor Script Plugin` (UE Python 스크립팅)
   - `OSC` (빌트인 OSC 플러그인, 참고용)
4. Project Settings > Python > `Allow Python Remote Execution` 활성화

## 0-3. MCP 서버 셋업 (AI 자동화 핵심)

### Claude Desktop 연동용 MCP 서버 구성

```bash
# Unreal MCP Server 클론 (VP 레포와 분리된 별도 위치)
cd C:\UnrealWork\tools
git clone https://github.com/chongdashu/unreal-mcp.git

# pyproject.toml이 Python/ 하위 디렉토리에 위치
cd unreal-mcp\Python
uv sync
```

### `claude_desktop_config.json` 설정

```json
{
  "mcpServers": {
    "unreal-mcp": {
      "command": "uv",
      "args": ["--directory", "C:/UnrealWork/tools/unreal-mcp/Python", "run", "unreal_mcp_server.py"],
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

### 프로젝트 전용 MCP 서버 (vp-tracker/mcp_server.py)

```python
"""VP Pipeline 전용 MCP 서버 - AI 어시스턴트가 파이프라인 상태를 진단/제어"""
from mcp.server.fastmcp import FastMCP
import socket
import os

mcp = FastMCP("VP Pipeline Tools")

@mcp.tool()
def check_port_available(port: int) -> str:
    """UDP/OSC 포트가 사용 가능한지 확인"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind(('127.0.0.1', port))
        s.close()
        return f"[OK] Port {port} is available"
    except OSError as e:
        return f"[FAIL] Port {port} is already in use: {e}"

@mcp.tool()
def check_webcam() -> str:
    """노트북 웹캠 접근 가능 여부 확인"""
    try:
        import cv2
        cap = cv2.VideoCapture(0)
        if cap.isOpened():
            ret, frame = cap.read()
            h, w = frame.shape[:2] if ret else (0, 0)
            cap.release()
            return f"[OK] Webcam OK - Resolution: {w}x{h}"
        return "[FAIL] Webcam not accessible"
    except ImportError:
        return "[FAIL] OpenCV not installed"
    except Exception as e:
        return f"[FAIL] Webcam error: {type(e).__name__}"

@mcp.tool()
def check_obs_connection(port: int = 4455, password: str = "") -> str:
    """OBS WebSocket 연결 상태 확인"""
    try:
        import obsws_python as obs
        client = obs.ReqClient(host='localhost', port=port, password=password, timeout=3)
        ver = client.get_version()
        client.disconnect()
        return f"[OK] OBS Connected - Version: {ver.obs_version}"
    except ImportError:
        return "[FAIL] obsws-python not installed"
    except ConnectionRefusedError:
        return "[FAIL] OBS not running or WebSocket disabled"
    except Exception as e:
        return f"[FAIL] OBS Connection Failed: {type(e).__name__}"

@mcp.tool()
def check_mediapipe_models() -> str:
    """MediaPipe Task 모델 파일 존재 여부 확인"""
    models = {
        "face_landmarker.task": "FaceLandmarker",
        "pose_landmarker_heavy.task": "PoseLandmarker"
    }
    results = []
    for filename, name in models.items():
        path = os.path.join("models", filename)
        if os.path.exists(path):
            size_mb = os.path.getsize(path) / (1024*1024)
            results.append(f"[OK] {name}: {path} ({size_mb:.1f}MB)")
        else:
            results.append(f"[FAIL] {name}: {path} NOT FOUND")
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
