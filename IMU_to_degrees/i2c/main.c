#include "i2c.h"
#include "wit_c_sdk.h"
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <time.h>       // 【新增】用于获取日期
#include <sys/time.h>   // 【新增】用于获取毫秒级时间

#define TIMEOUT	3
#define RETRY	3
#define PI 3.14159265358979323846f

//零位补偿角度
#define ROLL_INSTALL_OFFSET  4.5f 
#define PITCH_INSTALL_OFFSET -1.0f

static int fd;
char *i2c_dev = "/dev/i2c-1";

// 全局文件指针
FILE *csv_file = NULL;

#define ACC_UPDATE		0x01
#define GYRO_UPDATE		0x02
#define ANGLE_UPDATE	0x04
#define MAG_UPDATE		0x08
#define READ_UPDATE		0x80
static volatile char s_cDataUpdate = 0;

static void AutoScanSensor(void);
static void CopeSensorData(uint32_t uiReg, uint32_t uiRegNum);
static void Delayms(uint16_t ucMs);
static int i2c_read(u8 addr, u8 reg, u8 *data, u32 len);
static int i2c_write(u8 addr, u8 reg, u8 *data, u32 len);

// 【新增】获取当前毫秒级时间戳的辅助函数
long long get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int main(int argc,char* argv[]){
    if(argc < 2)
    {
        printf("please input dev name\n");
        return 0;
    }
    
    float fAcc[3], fGyro[3], fAngle[3];
    int i;
    
    // 1. 生成带时间戳的文件名
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char filename[64];
    // 文件名格式：slope_data_20231027_153000.csv
    sprintf(filename, "slope_data_%04d%02d%02d_%02d%02d%02d.csv", 
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, 
            tm.tm_hour, tm.tm_min, tm.tm_sec);

    // 2. 打开文件
    csv_file = fopen(filename, "w");
    if (csv_file == NULL) {
        printf("Error: Could not create CSV file!\n");
        return -1;
    }
    printf(">>> Data will be saved to: %s <<<\n", filename);

    // 3. 写入表头 (Header)
    // Time_ms: 系统毫秒时间, Roll/Pitch/Yaw: 姿态角, Slope: 估计坡度
    fprintf(csv_file, "Time_ms,Roll_deg,Pitch_deg,Yaw_deg,Slope_deg\n");
    
    fd = i2c_open(argv[1], 3, 3);
    if( fd < 0) printf("open %s fail\n", argv[1]);
    else printf("open %s success\n", argv[1]);
    
    WitInit(WIT_PROTOCOL_I2C, 0x50);
    WitI2cFuncRegister(i2c_write, i2c_read);
    WitRegisterCallBack(CopeSensorData);
    WitDelayMsRegister(Delayms);
    
    printf("\r\n********************** wit-motion IIC Slope Est App ************************\r\n");
    AutoScanSensor();
    
    while (1)
    {
        WitReadReg(AX, 12);
        usleep(50000); // 50ms (20Hz)
        
        if(s_cDataUpdate)
        {
            for(i = 0; i < 3; i++)
            {
                fAcc[i] = sReg[AX+i] / 32768.0f * 16.0f;
                fGyro[i] = sReg[GX+i] / 32768.0f * 2000.0f;
                fAngle[i] = sReg[Roll+i] / 32768.0f * 180.0f;
            }
            
            if(s_cDataUpdate & ACC_UPDATE) s_cDataUpdate &= ~ACC_UPDATE;
            if(s_cDataUpdate & GYRO_UPDATE) s_cDataUpdate &= ~GYRO_UPDATE;
            if(s_cDataUpdate & MAG_UPDATE) s_cDataUpdate &= ~MAG_UPDATE;
            
            if(s_cDataUpdate & ANGLE_UPDATE)
            {
                // 1. 先进行软件补偿，把安装误差扣掉
                // 这样 calibrated_roll 和 calibrated_pitch 才是相对于机器人底盘的真实角度
                float calibrated_roll = fAngle[0] - ROLL_INSTALL_OFFSET;
                float calibrated_pitch = fAngle[1] - PITCH_INSTALL_OFFSET;

                // 2. 转弧度 (注意：用补偿后的角度去转)
                float roll_rad = calibrated_roll * PI / 180.0f;
                float pitch_rad = calibrated_pitch * PI / 180.0f;

                // 3. 计算综合倾斜角余弦值
                float cos_slope = cos(roll_rad) * cos(pitch_rad);
                
                // 4. 限幅保护
                if(cos_slope > 1.0f) cos_slope = 1.0f;
                if(cos_slope < -1.0f) cos_slope = -1.0f;
                
                // 5. 反余弦求坡度
                float slope_angle = acos(cos_slope) * 180.0f / PI;

                // 打印的时候可以把原始值和校准值都打出来，方便调试
                printf("Raw[R:%.1f P:%.1f] -> Calib[R:%.1f P:%.1f] -> Slope: %.2f\r\n", 
                       fAngle[0], fAngle[1], 
                       calibrated_roll, calibrated_pitch, 
                       slope_angle);

                // 【核心修改】写入 CSV 文件
                // 格式：时间戳, Roll, Pitch, Yaw, Slope
                fprintf(csv_file, "%lld,%.2f,%.2f,%.2f,%.2f\n", 
                        get_timestamp_ms(), fAngle[0], fAngle[1], fAngle[2], slope_angle);
                
                // 【强制刷新】关键步骤！确保每一行都立刻写进磁盘，防止断电丢数据
                fflush(csv_file);

                s_cDataUpdate &= ~ANGLE_UPDATE;
            }
        }
    }
    
    // 正常退出时的清理（虽然 while(1) 很难走到这，但写上是好习惯）
    if (csv_file) fclose(csv_file);
    close(fd);
    return 0;
}

static int i2c_read(u8 addr, u8 reg, u8 *data, u32 len)
{
    if(i2c_read_data(addr>>1, reg, data, len) < 0) return 0;
    return 1;
}

static int i2c_write(u8 addr, u8 reg, u8 *data, u32 len)
{
    if(i2c_write_data(addr>>1, reg, data, len) < 0) return 0;
    return 1;
}

static void CopeSensorData(uint32_t uiReg, uint32_t uiRegNum)
{
    int i;
    for(i = 0; i < uiRegNum; i++)
    {
        switch(uiReg)
        {
            case AZ:
                s_cDataUpdate |= ACC_UPDATE;
                break;
            case GZ:
                s_cDataUpdate |= GYRO_UPDATE;
                break;
            case HZ:
                s_cDataUpdate |= MAG_UPDATE;
                break;
            case Yaw:
                s_cDataUpdate |= ANGLE_UPDATE;
                break;
            default:
                s_cDataUpdate |= READ_UPDATE;
                break;
        }
        uiReg++;
    }
}

static void Delayms(uint16_t ucMs)
{
    usleep(ucMs*1000);
}

static void AutoScanSensor(void)
{
    int i, iRetry;
    for(i = 0; i < 0x7F; i++)
    {
        WitInit(WIT_PROTOCOL_I2C, i);
        iRetry = 2;
        do
        {
            s_cDataUpdate = 0;
            WitReadReg(AX, 3);
            usleep(5);
            if(s_cDataUpdate != 0)
            {
                printf("find %02X addr sensor\r\n", i);
                return ;
            }
            iRetry--;
        }while(iRetry);     
    }
    printf("can not find sensor\r\n");
    printf("please check your connection\r\n");
}