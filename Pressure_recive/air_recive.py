import serial
import csv
import time
from datetime import datetime
import os

# 串口配置
ser = serial.Serial('/dev/serial0', 115200, timeout=1)

# 生成带时间戳的文件名，防止覆盖之前的实验数据
# 比如: pressure_log_20231027_153000.csv
csv_filename = f"pressure_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

print(f"正在初始化... 数据将保存到: {csv_filename}")

try:
    # 打开文件，newline='' 是为了防止Windows/Linux换行符打架
    with open(csv_filename, 'w', newline='') as csvfile:
        # 创建 CSV 写入对象
        csv_writer = csv.writer(csvfile)
        
        # 写入表头 (Header)
        # Timestamp: 时间戳, LF/RF/LH/RH: 左前/右前/左后/右后 腿气压
        csv_writer.writerow(["Timestamp", "LF", "RF", "LH", "RH"])
        
        print("开始记录数据。按 Ctrl+C 停止。")
        
        while True:
            if ser.in_waiting > 0:
                try:
                    # decode加上 errors='ignore'，防止串口开头的一两个乱码字节让程序崩掉
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    if line.startswith("$PRS"):
                        # 解析数据: $PRS,101,102,99,100
                        parts = line.split(',')
                        
                        # 简单校验一下长度，防止空数据或者残缺帧
                        if len(parts) == 5:
                            # 提取4个气压值
                            pressure_values = parts[1:]
                            
                            # 获取当前毫秒级时间
                            current_time = datetime.now().strftime('%H:%M:%S.%f')[:-3]
                            
                            # 组合成一行数据
                            row = [current_time] + pressure_values
                            
                            # 写入文件
                            csv_writer.writerow(row)
                            
                            # 【专家建议】关键一步：强制刷新缓冲区
                            # 这样即使下一秒树莓派断电，这行数据也已经稳稳地在SD卡里了
                            csvfile.flush() 
                            
                            # 顺便在终端打印一下，让你知道它活着
                            print(f"[{current_time}] 已记录: {pressure_values}")
                            
                except ValueError:
                    # 偶尔可能会有转换错误，忽略这一帧就行，别让程序停
                    continue

except KeyboardInterrupt:
    print("\n程序已停止，数据已保存。")
except Exception as e:
    print(f"\n发生错误: {e}")
finally:
    if ser.is_open:
        ser.close()
