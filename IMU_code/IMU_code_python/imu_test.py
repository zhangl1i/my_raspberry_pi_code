import smbus
import time

# I2C 配置
I2C_BUS = 1  # 树莓派上的 I2C 总线编号
IMU_ADDRESS = 0x50  # IMU 的 I2C 地址
YAW_REGISTER = 0x3F  # Yaw 数据寄存器地址（来自 REG.h 文件）

# 初始化 I2C 总线
bus = smbus.SMBus(I2C_BUS)

def read_yaw():
    """读取 IMU 的 Yaw 角数据"""
    try:
        # 从 Yaw 寄存器读取 2 个字节的数据
        data = bus.read_i2c_block_data(IMU_ADDRESS, YAW_REGISTER, 2)
        # 将高字节和低字节组合成一个 16 位有符号整数
        yaw_raw = (data[1] << 8) | data[0]
        if yaw_raw >= 0x8000:  # 处理负数
            yaw_raw -= 0x10000
        # 将原始数据转换为实际角度值（根据 WitMotion 的数据格式）
        yaw_angle = yaw_raw / 32768.0 * 180.0
        return yaw_angle
    except Exception as e:
        print(f"读取 Yaw 数据失败: {e}")
        return None

def main():
    print("开始读取 IMU 数据...")
    while True:
        yaw = read_yaw()
        if yaw is not None:
            print(f"当前 Yaw 角: {yaw:.2f}°")
        time.sleep(0.5)  # 每 0.5 秒读取一次

if __name__ == '__main__':
    main()