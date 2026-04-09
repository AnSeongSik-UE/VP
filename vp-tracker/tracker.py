"""
VP Pipeline - Unified Webcam Tracker
MediaPipe Tasks API (FaceLandmarker + PoseLandmarker)를 동일 프레임에서 순차 실행
"""
import mediapipe as mp
import cv2
import time
import threading
from dataclasses import dataclass, field
from typing import Optional
from queue import Queue, Empty

@dataclass
class TrackingFrame:
    """하나의 프레임에서 추출된 전체 트래킹 데이터"""
    timestamp: float = 0.0
    blendshapes: dict[str, float] = field(default_factory=dict)      # ARKit 52 블렌드쉐이프
    pose_landmarks: list[tuple[float, float, float]] = field(default_factory=list)  # 33개 (x, y, z)

BaseOptions = mp.tasks.BaseOptions
FaceLandmarker = mp.tasks.vision.FaceLandmarker
FaceLandmarkerOptions = mp.tasks.vision.FaceLandmarkerOptions
PoseLandmarker = mp.tasks.vision.PoseLandmarker
PoseLandmarkerOptions = mp.tasks.vision.PoseLandmarkerOptions
VisionRunningMode = mp.tasks.vision.RunningMode


class UnifiedTracker:
    """단일 웹캠에서 Face + Pose를 동시에 추론하는 통합 트래커"""

    def __init__(self, camera_id: int = 0):
        self.camera_id = camera_id
        self.data_queue: Queue[TrackingFrame] = Queue(maxsize=2)
        self.running = False
        self._thread: Optional[threading.Thread] = None
        self._fps = 0.0

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

    @property
    def fps(self) -> float:
        return self._fps

    def start(self):
        """트래킹 시작"""
        self.running = True
        self._thread = threading.Thread(target=self._tracking_loop, daemon=True)
        self._thread.start()

    def stop(self):
        """트래킹 중지"""
        self.running = False
        if self._thread:
            self._thread.join(timeout=5)

    def _tracking_loop(self):
        cap = cv2.VideoCapture(self.camera_id)
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

        if not cap.isOpened():
            print("[ERROR] Cannot open webcam")
            return

        actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        print(f"[CAM] Webcam opened: {actual_w}x{actual_h}")

        with FaceLandmarker.create_from_options(self.face_options) as face_lm, \
             PoseLandmarker.create_from_options(self.pose_options) as pose_lm:

            prev_time = time.time()
            frame_count = 0

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

                # FPS 계산
                frame_count += 1
                elapsed = time.time() - prev_time
                if elapsed >= 1.0:
                    self._fps = frame_count / elapsed
                    frame_count = 0
                    prev_time = time.time()

        cap.release()
        print("[CAM] Webcam released")

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
        """최신 트래킹 데이터 반환 (없으면 None)"""
        try:
            return self.data_queue.get_nowait()
        except Empty:
            return None


def main():
    """트래커 단독 테스트 — 콘솔에 실시간 출력"""
    tracker = UnifiedTracker(camera_id=0)
    tracker.start()
    print("[*] Tracking started (Ctrl+C to stop)")

    try:
        while True:
            frame = tracker.get_latest()
            if frame:
                bs_count = len(frame.blendshapes)
                pose_count = len(frame.pose_landmarks)

                # 대표 블렌드쉐이프 몇 개 출력
                sample_bs = ""
                if frame.blendshapes:
                    items = list(frame.blendshapes.items())[:3]
                    sample_bs = " | ".join(f"{k}={v:.2f}" for k, v in items)

                print(
                    f"\r[{tracker.fps:.1f}fps] "
                    f"BS:{bs_count} Pose:{pose_count} "
                    f"| {sample_bs}",
                    end="", flush=True
                )
            time.sleep(1/60)
    except KeyboardInterrupt:
        print("\n[*] Stopping...")
        tracker.stop()
        print("[OK] Tracking stopped")


if __name__ == "__main__":
    main()
