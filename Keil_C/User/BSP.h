/*****************************************************************************************************
 * @file    BSP.h
 * @brief   兴邦电器 0010902165 按键板底层板级支持包头文件
 * @details 硬件引脚宏定义、GPIO配置、Timer0 1ms定时器及延时函数
 *****************************************************************************************************/
#ifndef _BSP_H_
#define _BSP_H_

#include "sc92f.h"

/*******************************************************************************
 * 1. 硬件引脚定义 (依据 0010902165 按键板原理图)
 *******************************************************************************/
// 外接线束 H2 (PZ200V-11-07P)
// Pin 1: VDD (+5V)
// Pin 2: GND
// Pin 3: RX1 (P2.1)
// Pin 4: TX1 (P2.0) - 电机控制串口发送
// Pin 5: P5.1 - 照明控制硬件输出
// Pin 6: P5.0 - 硬件控制输出
// Pin 7: P1.4 - 外部信号输入
sbit PIN_TX1         = P2^0;
sbit PIN_RX1         = P2^1;
sbit PIN_LIGHT_OUT   = P5^1;    // 照明输出控制
sbit PIN_P50_OUT     = P5^0;    // 辅助硬件输出
sbit PIN_P14_OUT     = P1^4;    // Pin 7: 电机主电源/变频使能输出 (开机5V, 关机延时30s回0V)

// 3528 按键背光灯公共阴极控制引脚
sbit PIN_LED_CAT_COM = P1^1;    // 3528灯共阴极: 置0导通使能, 置1全部关闭

// 数码管共极公共端 (COM)
sbit PIN_SEG_COM     = P2^2;    // 数码管公共COM端 (Pin 7)

// 触摸按键输入引脚 (SC92F8463B 物理引脚定义: K1=P1.7, K2=P2.7, K3=P2.6, K4=P2.3, K5=P2.4, K6=P2.5)
sbit PIN_KEY_K1      = P1^7;    // K1: P1.7 (TK16)
sbit PIN_KEY_K2      = P2^7;    // K2: P2.7 (TK15)
sbit PIN_KEY_K3      = P2^6;    // K3: P2.6 (TK14)
sbit PIN_KEY_K4      = P2^3;    // K4: P2.3 (TK11)
sbit PIN_KEY_K5      = P2^4;    // K5: P2.4 (TK12)
sbit PIN_KEY_K6      = P2^5;    // K6: P2.5 (TK13)

#define PIN_KEY_PWR   PIN_KEY_K1
#define PIN_KEY_DEC   PIN_KEY_K2
#define PIN_KEY_INC   PIN_KEY_K3
#define PIN_KEY_LIGHT PIN_KEY_K4

/*******************************************************************************
 * 2. 系统时钟与定时常量定义
 *******************************************************************************/
#define FSYS_CLK            12000000UL  // 内部高频 RC (Fosc/2 = 12MHz, 严格对应烧录器 Option 配置)
#define UART1_BAUD          2400UL      // 通信波特率 2400 bps

#define TIMER0_RELOAD_VAL   (65536UL - (FSYS_CLK / 1000UL)) // 1ms 定时重装载初值
#define UART1_BAUD_DIV_VAL  (FSYS_CLK / UART1_BAUD)         // 串口分频初值 (12MHz / 2400 = 5000 = 0x1388)

/*******************************************************************************
 * 3. 全局滴答计时与状态声明
 *******************************************************************************/
extern volatile unsigned int g_systick_ms;  // 1ms 软件滴答计数
extern volatile bit g_b300ms_flag;          // 300ms 心跳发送触发标志
extern volatile bit g_b10ms_flag;           // 10ms 按键与逻辑扫描标志
extern volatile bit g_b1s_flag;            // 1秒滴答标志 (用于15分钟倒计时)

/*******************************************************************************
 * 4. 函数声明
 *******************************************************************************/
void BSP_Init(void);
void BSP_Timer0_Init(void);
void BSP_Timer0_IRQHandler(void);
void BSP_UART1_Init(void);
void UART1_SendByte(unsigned char dat);
void UART1_SendBuffer(unsigned char *buf, unsigned char len);

void Delay_10us(void);
void Delay_ms(unsigned int ms);

void BSP_Buzzer_Init(void);
void BSP_Beep(unsigned int duration_ms);
extern volatile unsigned int g_beep_ticks;

#endif /* _BSP_H_ */
