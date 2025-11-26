# Robot_IMU_to_degrees

树莓派0上接收IMU通过I2C传输过来的姿态角数据，然后计算当前机器人所在平面坡度并进行输出。

#### 1. 编译

gcc main.c i2c.c wit_c_sdk.c -o imu_logger -lm

#### 2. 运行 

./imu_logger /dev/i2c-1



