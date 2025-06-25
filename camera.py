import cv2
import threading
import queue

frame_queue = queue.Queue(maxsize=2)  # 双缓冲机制

def capture_thread():
    cap = cv2.VideoCapture(0)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        frame = cv2.rotate(frame, cv2.ROTATE_90_COUNTERCLOCKWISE)
        if not frame_queue.full():
            frame_queue.put(frame)
        else:
            # 主动丢弃旧帧保证实时性
            try: frame_queue.get_nowait()
            except: pass
            frame_queue.put(frame)

def display_thread():
    cv2.namedWindow('Camera', cv2.WINDOW_NORMAL)
    while True:
        try:
            frame = frame_queue.get(timeout=0.1)
            cv2.imshow('Camera', frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
        except queue.Empty:
            continue

if __name__ == "__main__":
    threading.Thread(target=capture_thread, daemon=True).start()
    display_thread()
    cv2.destroyAllWindows()
