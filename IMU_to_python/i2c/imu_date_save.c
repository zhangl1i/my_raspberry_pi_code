#include "i2c.h"
#include "wit_c_sdk.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#define TIMEOUT	3
#define RETRY	3

static int fd;
FILE *data_file = NULL;
static volatile char s_cDataUpdate = 0;
static volatile int keep_running = 1;

#define ACC_UPDATE		0x01
#define GYRO_UPDATE		0x02
#define ANGLE_UPDATE	0x04
#define MAG_UPDATE		0x08
#define READ_UPDATE		0x80

// ---------- 提前定义 ---------- //
static void handle_sigint(int sig) {
    keep_running = 0;
}

static void AutoScanSensor(void);
static void CopeSensorData(uint32_t uiReg, uint32_t uiRegNum);
static void Delayms(uint16_t ucMs);
static int i2c_read(u8 addr, u8 reg, u8 *data, u32 len);
static int i2c_write(u8 addr, u8 reg, u8 *data, u32 len);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("please input dev name\n");
        return 0;
    }

    signal(SIGINT, handle_sigint);  // 注册 Ctrl+C 信号处理

    float fAcc[3], fGyro[3], fAngle[3];
    int i;

    fd = i2c_open(argv[1], TIMEOUT, RETRY);
    if (fd < 0) {
        printf("open %s fail\n", argv[1]);
        return -1;
    } else {
        printf("open %s success\n", argv[1]);
    }

    WitInit(WIT_PROTOCOL_I2C, 0x50);
    WitI2cFuncRegister(i2c_write, i2c_read);
    WitRegisterCallBack(CopeSensorData);
    WitDelayMsRegister(Delayms);
    printf("\r\n********************** wit-motion IIC example  ************************\r\n");
    AutoScanSensor();

    // 创建CSV文件
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[256];
    snprintf(filename, sizeof(filename), "imu_data_%04d%02d%02d_%02d%02d%02d.csv",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);

    data_file = fopen(filename, "w");
    if (!data_file) {
        perror("Failed to open data file");
        return -1;
    } else {
        fprintf(data_file, "Timestamp,AccX,AccY,AccZ,GyroX,GyroY,GyroZ,Roll,Pitch,Yaw\n");
        fflush(data_file);
    }

    // ---------- 主循环 ---------- //
    while (keep_running) {
        WitReadReg(AX, 12);

        for (int j = 0; j < 100; j++) {  // 共500ms，分成100次短睡眠
            if (!keep_running) break;
            usleep(5000);  // 每次5ms
        }

        if (!keep_running) break;

        if (s_cDataUpdate) {
            for (i = 0; i < 3; i++) {
                fAcc[i] = sReg[AX + i] / 32768.0f * 16.0f;
                fGyro[i] = sReg[GX + i] / 32768.0f * 2000.0f;
                fAngle[i] = sReg[Roll + i] / 32768.0f * 180.0f;
            }

            if (s_cDataUpdate & ANGLE_UPDATE) {
                printf("angle:%.3f %.3f %.3f\r\n", fAngle[0], fAngle[1], fAngle[2]);
                s_cDataUpdate &= ~ANGLE_UPDATE;

                if (data_file) {
                    time_t ts = time(NULL);
                    struct tm *tt = localtime(&ts);
                    fprintf(data_file, "%04d-%02d-%02d %02d:%02d:%02d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                            tt->tm_year + 1900, tt->tm_mon + 1, tt->tm_mday,
                            tt->tm_hour, tt->tm_min, tt->tm_sec,
                            fAcc[0], fAcc[1], fAcc[2],
                            fGyro[0], fGyro[1], fGyro[2],
                            fAngle[0], fAngle[1], fAngle[2]);
                    fflush(data_file);
                }
            }

            if (s_cDataUpdate & ACC_UPDATE) s_cDataUpdate &= ~ACC_UPDATE;
            if (s_cDataUpdate & GYRO_UPDATE) s_cDataUpdate &= ~GYRO_UPDATE;
            if (s_cDataUpdate & MAG_UPDATE) s_cDataUpdate &= ~MAG_UPDATE;
        }
    }

    if (data_file) fclose(data_file);
    i2c_close(fd);
    printf("\nProgram terminated. Data saved.\n");
    return 0;
}

// ---------- 辅助函数 ---------- //
static int i2c_read(u8 addr, u8 reg, u8 *data, u32 len) {
    return i2c_read_data(addr >> 1, reg, data, len) < 0 ? 0 : 1;
}

static int i2c_write(u8 addr, u8 reg, u8 *data, u32 len) {
    return i2c_write_data(addr >> 1, reg, data, len) < 0 ? 0 : 1;
}

static void CopeSensorData(uint32_t uiReg, uint32_t uiRegNum) {
    for (int i = 0; i < uiRegNum; i++) {
        switch (uiReg) {
            case AZ: s_cDataUpdate |= ACC_UPDATE; break;
            case GZ: s_cDataUpdate |= GYRO_UPDATE; break;
            case HZ: s_cDataUpdate |= MAG_UPDATE; break;
            case Yaw: s_cDataUpdate |= ANGLE_UPDATE; break;
            default: s_cDataUpdate |= READ_UPDATE; break;
        }
        uiReg++;
    }
}

static void Delayms(uint16_t ucMs) {
    usleep(ucMs * 1000);
}

static void AutoScanSensor(void) {
    for (int i = 0; i < 0x7F; i++) {
        WitInit(WIT_PROTOCOL_I2C, i);
        int retry = 2;
        do {
            s_cDataUpdate = 0;
            WitReadReg(AX, 3);
            usleep(5000);
            if (s_cDataUpdate != 0) {
                printf("find %02X addr sensor\r\n", i);
                return;
            }
        } while (--retry);
    }
    printf("can not find sensor\r\nplease check your connection\r\n");
}
