## 树莓派保存终端输出的IMU数据并画图

使用script命令记录终端会话

`script -f imu_session.log`

`sudo ./imu_example /dev/i2c-1`

按 `Ctrl+D` 或输入 `exit` 结束录制，所有内容都会保存在 `imu_session.log` 中。

#### 提取.log数据并保存为 CSV

![image-20250625153342738](C:\Users\bisho\AppData\Roaming\Typora\typora-user-images\image-20250625153342738.png)

#### 创建转换脚本 `convert_log_to_csv.py`



## C语言版本IMU数据读取并输出

`gcc -o imu_date_save wit_c_sdk.c i2c.c imu_date_save.c -li2c`

`sudo ./imu_date_save /dev/i2c-1`

输出的数据保存在imu_data_2025xxxxx.csv