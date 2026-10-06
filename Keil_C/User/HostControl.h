/*****************************************************************************************************
 * @file    HostControl.h
 * @brief   兴邦电器 0010902165 按键板与 0010902163 主电脑板通信与状态控制引擎
 * @details 10字节 2400bps 协议打包、CRC-16/X-25校验、300ms心跳、风机调速、15分钟延时、独立照明
 *****************************************************************************************************/
#ifndef _HOST_CONTROL_H_
#define _HOST_CONTROL_H_

#include "BSP.h"

/*******************************************************************************
 * 1. 电机档位与转速定义 (基于官方样机实测逆向数据)
 *******************************************************************************/
typedef enum {
    WIND_GEAR_OFF = 0,      // 关机停风 (0 RPM)
    WIND_GEAR_1   = 1,      // 1 档低速 (1050 RPM: 0x041A)
    WIND_GEAR_2   = 2,      // 2 档中速 (1350 RPM: 0x0546)
    WIND_GEAR_3   = 3       // 3 档高速 (1800 RPM: 0x0708)
} WindGear_t;

#define MOTOR_RPM_OFF           0
#define MOTOR_RPM_GEAR1         1050
#define MOTOR_RPM_GEAR2         1350
#define MOTOR_RPM_GEAR3         1800
#define MOTOR_RPM_MAX_LIMIT     2000    // 最高转速上限 (0x07D0)

#define DELAY_TOTAL_SECONDS     900     // 15 分钟倒计时 = 900 秒

/*******************************************************************************
 * 2. 外部状态变量声明
 *******************************************************************************/
extern WindGear_t g_current_gear;        // 当前风机档位
extern bit g_bPowerOn;                   // 开机状态标志 (0: 关机, 1: 开机)
extern bit g_bLightOn;                   // 照明状态标志 (0: 关, 1: 开)
extern bit g_bDelayActive;               // 15分钟延时模式使能标志
extern unsigned int g_delay_sec_remain;  // 延时倒计时剩余秒数 (初始900秒)

/*******************************************************************************
 * 3. 核心 API 函数声明
 *******************************************************************************/
// 工业级 CRC-16 / X-25 校验算法 (多项式 0x1021, 倒序 0x8408, 初值 0xFFFF)
unsigned int Get_CRC16_X25(unsigned char *pData, unsigned char len);

// 通信控制与帧组装
void Host_Init(void);
void Host_BuildFrame(WindGear_t gear, unsigned char *out_buf);
void Host_SendHeartbeat(void);
void Host_SendKeyBurst(void);

// 业务功能操作
void Host_PowerOn(void);
void Host_PowerOff(void);
void Host_SpeedUp(void);
void Host_SpeedDown(void);
void Host_ToggleLight(void);
void Host_SetLight(bit on);
void Host_StartDelay(void);
void Host_CancelDelay(void);
void Host_Delay1sTick(void);

#endif /* _HOST_CONTROL_H_ */
