import subprocess
import time

# 打开文件用于写入
with open("imu_data_log.txt", "w") as f:
    # 启动你的 imu_example 程序
    process = subprocess.Popen(
        ["sudo", "./imu_example", "/dev/i2c-1"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )

    try:
        while True:
            line = process.stdout.readline()
            if not line:
                break
            timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
            print(line.strip())  # 打印到终端（可选）
            f.write(f"{timestamp} | {line}")  # 写入文件
            f.flush()  # 确保实时写入磁盘
    except KeyboardInterrupt:
        print("\nStopping logging...")