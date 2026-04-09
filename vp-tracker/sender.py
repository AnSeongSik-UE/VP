"""
VP Pipeline - UDP/OSC 네트워크 발신
트래킹 데이터를 언리얼 엔진으로 전송
"""
import struct
import time
import socket
from pythonosc import udp_client
from tracker import UnifiedTracker, TrackingFrame


class OSCSender:
    """python-osc를 활용한 OSC 프로토콜 전송"""

    def __init__(self, host: str = "127.0.0.1", port: int = 7000):
        self.client = udp_client.SimpleUDPClient(host, port)

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
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.target = (host, port)
        self._send_count = 0

    @property
    def send_count(self) -> int:
        return self._send_count

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
        self._send_count += 1

    def close(self):
        self.sock.close()


def main():
    """트래킹 + 네트워크 발신 통합 실행"""
    tracker = UnifiedTracker(camera_id=0)
    sender = RawUDPSender(host="127.0.0.1", port=7000)
    # OSC가 필요하면: sender = OSCSender(host="127.0.0.1", port=7000)

    tracker.start()
    print("[*] Tracking started. Sending to UE at 127.0.0.1:7000")
    print("    (Ctrl+C to stop)")

    try:
        while True:
            frame = tracker.get_latest()
            if frame:
                sender.send_tracking_frame(frame)

                # 상태 출력 (1초마다)
                if sender.send_count % 30 == 0:
                    bs_count = len(frame.blendshapes)
                    pose_count = len(frame.pose_landmarks)
                    print(
                        f"\r[{tracker.fps:.1f}fps] "
                        f"Sent:{sender.send_count} "
                        f"BS:{bs_count} Pose:{pose_count}",
                        end="", flush=True
                    )
            time.sleep(1/60)  # ~60Hz
    except KeyboardInterrupt:
        print("\n[*] Stopping...")
        tracker.stop()
        sender.close()
        print(f"[OK] Total {sender.send_count} packets sent")


if __name__ == "__main__":
    main()
