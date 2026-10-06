/*****************************************************************************************************
 * @file    Display.h
 * @brief   兴邦电器 0010902165 按键板 3528按键灯与数码管显示驱动头文件
 * @details 支持按下K2翻转LED2, 按下K3翻转LED3, 按下K4翻转LED4, 按下K5翻转LED5
 *****************************************************************************************************/
#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include "BSP.h"
#include "HostControl.h"

/*******************************************************************************
 * 外部状态变量声明
 *******************************************************************************/
extern bit g_bLed2On; // LED2 点亮状态 (P0.7, 受 K2 / P2.7 触摸按键翻转控制, 调试校准)
extern bit g_bLed3On; // LED3 点亮状态 (P0.6, 受 K3 / P2.6 触摸按键翻转控制, 调试校准)
extern bit g_bLed4On; // LED4 点亮状态 (P0.3, 受 K4 / P2.3 触摸按键翻转控制)
extern bit g_bLed5On; // LED5 点亮状态 (P0.1, 受 K5 / P2.4 触摸按键翻转控制)

/*******************************************************************************
 * 函数声明
 *******************************************************************************/
void Display_Init(void);
void Display_Update(void);
void Display_Scan_Tick(void);

void Display_ToggleLed2(void);
void Display_SetLed2(bit state);

void Display_ToggleLed3(void);
void Display_SetLed3(bit state);

void Display_ToggleLed4(void);
void Display_SetLed4(bit state);

void Display_ToggleLed5(void);
void Display_SetLed5(bit state);

#endif /* _DISPLAY_H_ */
