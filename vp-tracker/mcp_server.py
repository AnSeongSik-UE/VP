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
