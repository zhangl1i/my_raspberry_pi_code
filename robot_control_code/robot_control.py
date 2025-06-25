#!/usr/bin/env python3
import serial
import struct
import time
import sys

SERIAL_PORT = '/dev/serial0'  # 根据实际情况修改
BAUD_RATE = 115200

def create_frame(cmd_type, value):
    """构造VOFA协议数据帧"""
    frame = bytearray()
    frame.append(0x23)  # Header1
    frame.append(0x02)  # Header2
    frame.append(cmd_type)
    frame.extend(struct.pack("<f", value))
    frame.append(0xAA)  # Footer
    return frame

def send_stop_command(ser):
    """仅发送速度归零指令，保持使能状态"""
    try:
        # 仅发送XY轴归零指令
        stop_x = create_frame(0x01, 0.0)
        stop_y = create_frame(0x02, 0.0)
        ser.write(stop_x + stop_y)
        ser.flush()
        print("[安全停止] 速度已归零")
    except Exception as e:
        print(f"停止指令发送失败: {str(e)}")

def main():
    # 初始化串口
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"串口已连接: {SERIAL_PORT}")
    except Exception as e:
        print(f"串口连接失败: {str(e)}")
        sys.exit(1)
    
    time.sleep(1.5)  # 等待硬件初始化
    
    started = False  # 机器人使能状态标志
    current_speed = 0.5
    speed_step = 0.1
    current_direction = None

    # 操作指引
    print("\n=== 机器人控制指令 ===")
    print("  W/S       - 前进/后退")
    print("  A/D       - 左移/右移")
    print("  SPACE     - 紧急停止")
    print("  +/-       - 增减速度")
    print("  Q         - 退出程序")
    print("========================")

    while True:
        try:
            key = input("控制指令 > ").strip().upper()
            if not key:
                key = " "  # 空输入视为停止

            # 退出程序
            if key == "Q":
                print("\n正在安全停止...")
                send_stop_command(ser)
                # 发送使能关闭指令
                disable_frame = create_frame(0x00, 0.0)
                ser.write(disable_frame)
                break

            # 紧急停止（仅归零速度，保持使能）
            if key == " ":
                send_stop_command(ser)
                current_direction = None
                continue

            # 速度调节
            if key == "+":
                current_speed = min(current_speed + speed_step, 2.0)
                print(f"当前速度: {current_speed:.1f}")
                if current_direction:  # 速度变化后立即生效
                    key = current_direction
                else:
                    continue

            elif key == "-":
                current_speed = max(current_speed - speed_step, 0.1)
                print(f"当前速度: {current_speed:.1f}")
                if current_direction:
                    key = current_direction
                else:
                    continue

            # 方向控制
            direction_map = {
                "W": (0x01, current_speed),   # X+
                "S": (0x01, -current_speed),  # X-
                "D": (0x02, current_speed),   # Y+
                "A": (0x02, -current_speed)   # Y-
            }

            if key in direction_map:
                if not started:
                    # 首次运动指令：发送使能命令
                    enable_frame = create_frame(0x00, 1.0)
                    ser.write(enable_frame)
                    started = True
                    print("[系统启动] 机器人已使能")
                    time.sleep(0.5)  # 关键延迟：等待使能生效

                # 发送运动指令
                cmd, speed = direction_map[key]
                frame = create_frame(cmd, speed)
                ser.write(frame)
                current_direction = key
                print(f"[运动控制] 方向: {key} 速度: {abs(speed):.1f}")
            else:
                print("无效指令，请按提示输入")

        except KeyboardInterrupt:
            print("\n检测到强制停止！")
            send_stop_command(ser)
            break

    ser.close()
    print("程序已退出")

if __name__ == '__main__':
    main()