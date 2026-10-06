/*****************************************************************************************************
 * @file    Key.c
 * @brief   兴邦电器 0010902165 按键板按键检测驱动实现
 * @details 严格对应用户定义:
 *          按键2 (K2) = TK15 (P2.7) -> 控制 LED2 (P0.7)
 *          按键3 (K3) = TK16 (P1.7) -> 控制 LED3 (P0.6)
 *          按键4 (K4) = TK12 (P2.4) -> 控制 LED4 (P0.3)
 *          按键5 (K5) = TK13 (P2.5) -> 控制 LED5 (P0.1)
 *****************************************************************************************************/
#include "Key.h"

// 按键计数器与状态标志
static unsigned char s_k2_cnt = 0;
static bit           s_k2_latched = 0;

static unsigned char s_k3_cnt = 0;
static bit           s_k3_latched = 0;

static unsigned char s_k4_cnt = 0;
static bit           s_k4_latched = 0;

static unsigned char s_k5_cnt = 0;
static bit           s_k5_latched = 0;

#define DEBOUNCE_CNT        1       // 赛元硬件驱动已有滤波确认，上层首拍即刻响应

/**
 * @brief  初始化按键检测模块
 */
void Key_Init(void)
{
    s_k2_cnt = 0;
    s_k2_latched = 0;

    s_k3_cnt = 0;
    s_k3_latched = 0;

    s_k4_cnt = 0;
    s_k4_latched = 0;

    s_k5_cnt = 0;
    s_k5_latched = 0;
}

/**
 * @brief  按键状态机扫描检测
 * @param  tk_flag: 赛元触控驱动 TouchKeyScan() 返回的通道按压状态掩码
 * @retval 触发的按键事件
 */
KeyEvent_t Key_Scan_10ms(unsigned long int tk_flag)
{
    bit k2_active = ((tk_flag & TK_MASK_K2) != 0);  // 按键2: TK15 (P2.7)
    bit k3_active = ((tk_flag & TK_MASK_K3) != 0);  // 按键3: TK16 (P1.7)
    bit k4_active = ((tk_flag & TK_MASK_K4) != 0);  // 按键4: TK12 (P2.4)
    bit k5_active = ((tk_flag & TK_MASK_K5) != 0);  // 按键5: TK13 (P2.5)

    // 1. 按键2 (TK15 / P2.7) - 短按单次触发
    if (k2_active)
    {
        if (s_k2_cnt < 255) { s_k2_cnt++; }
        if ((s_k2_cnt >= DEBOUNCE_CNT) && (!s_k2_latched))
        {
            s_k2_latched = 1;
            return KEY_EVENT_K2_CLICK;
        }
    }
    else
    {
        s_k2_cnt = 0;
        s_k2_latched = 0;
    }

    // 2. 按键3 (TK16 / P1.7) - 短按单次触发
    if (k3_active)
    {
        if (s_k3_cnt < 255) { s_k3_cnt++; }
        if ((s_k3_cnt >= DEBOUNCE_CNT) && (!s_k3_latched))
        {
            s_k3_latched = 1;
            return KEY_EVENT_K3_CLICK;
        }
    }
    else
    {
        s_k3_cnt = 0;
        s_k3_latched = 0;
    }

    // 3. 按键4 (TK12 / P2.4) - 短按单次触发
    if (k4_active)
    {
        if (s_k4_cnt < 255) { s_k4_cnt++; }
        if ((s_k4_cnt >= DEBOUNCE_CNT) && (!s_k4_latched))
        {
            s_k4_latched = 1;
            return KEY_EVENT_K4_CLICK;
        }
    }
    else
    {
        s_k4_cnt = 0;
        s_k4_latched = 0;
    }

    // 4. 按键5 (TK13 / P2.5) - 短按单次触发
    if (k5_active)
    {
        if (s_k5_cnt < 255) { s_k5_cnt++; }
        if ((s_k5_cnt >= DEBOUNCE_CNT) && (!s_k5_latched))
        {
            s_k5_latched = 1;
            return KEY_EVENT_K5_CLICK;
        }
    }
    else
    {
        s_k5_cnt = 0;
        s_k5_latched = 0;
    }

    return KEY_EVENT_NONE;
}
