import subprocess
import re
import time

timestamp = time.strftime("%Y%m%d_%H%M%S")
csv_filename = f"imu_data_{timestamp}.csv"

# 启动 IMU 程序（强制启用行缓冲）
process = subprocess.Popen(
    ["sudo", "./imu_example", "/dev/i2c-1"],
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
    bufsize=1
)

# 打开 CSV 文件
with open(csv_filename, "w") as csv_file:
    csv_file.write("Roll,Pitch,Yaw\n")  # 写入表头

    try:
        while True:
            line = process.stdout.readline()
            if not line:
                print("End of output")
                break
            print("Raw line:", line.strip())  # 调试：打印原始行
            
            # 匹配数据（更宽松的正则）
            match = re.search(r"angle:[^\d]*(-?\d+\.?\d*)[^\d]*(-?\d+\.?\d*)[^\d]*(-?\d+\.?\d*)", line)
            if match:
                roll, pitch, yaw = match.groups()
                print(f"Matched: {roll}, {pitch}, {yaw}")  # 调试：打印匹配结果
                csv_file.write(f"{roll},{pitch},{yaw}\n")
                csv_file.flush()  # 实时写入
    except KeyboardInterrupt:
        print("\nStopping data collection...")