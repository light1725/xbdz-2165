/*****************************************************************************************************
 * @file    main.c
 * @brief   兴邦电器 0010902165 按键板主控程序
 * @details 业务逻辑:
 *          - K2 (TK15 / P2.7): 独立照明控制 (点按开，再次点按关，翻转 LED2)
 *          - K3 (TK16 / P1.7): 加档 (开机: 1档->2档->3档; 关机时点按也可直接开机进入1档)
 *          - K4 (TK12 / P2.4): 减档 (开机: 3档->2档->1档; 关机时点按也可直接开机进入1档)
 *          - K5 (TK13 / P2.5): 电源键 (关机点按开机进入1档，开机点按关机停转)
 *          - 串口通信: 周期 300ms 持续发送当前档位控制帧 (UART1 2400bps, TX1 / P2.0 强推挽)
 *****************************************************************************************************/
#include "BSP.h"
#include "HostControl.h"
#include "Display.h"
#include "Key.h"
#include "TKDriver.h"

// 触摸按键实时扫描状态掩码
static unsigned long int g_touch_key_flag = 0;

// K5 按键长按延时模式计时变量
static bit s_k5_is_pressed = 0;
static bit s_k5_long_triggered = 0;
static unsigned int s_k5_press_ms = 0;

/**
 * @brief  系统主函数入口
 */
void main(void)
{
    // 1. 底层硬件总初始化 (GPIO, Timer0 1ms 分时扫描, UART1 2400bps 强推挽)
    BSP_Init();

    // 2. 显示模块与业务状态机初始化
    Display_Init();
    Key_Init();
    Host_Init();

    // 3. 赛元触摸库初始化 (初始化 4 通道: TK12, TK13, TK15, TK16)
    TouchKeyInit();

    // 4. 上电延时 50ms 等待电源彻底稳定与排线防抖，随后发出上电就绪“滴”声提示
    Delay_ms(50);
    BSP_Beep(155); // Pin 6 (P5.0) 输出 155ms 4.0kHz 方波蜂鸣器上电提示音

    // 5. 上电等待触摸基线自动校准稳定 (300ms 稳定期)
    Delay_ms(300);

    // 6. 上电广播首帧关机状态帧
    Host_SendHeartbeat();

    // 6. 主事件调度循环
    while (1)
    {
        //----------------------------------------------------------------------
        // 1. 触摸硬件扫描事件: 一次硬件扫描完成触发一次按键状态判定
        //----------------------------------------------------------------------
        if (SOCAPI_TouchKeyStatus & 0x80)
        {
            static bit s_k2_latched = 0;
            static bit s_k3_latched = 0;
            static bit s_k4_latched = 0;
            bit need_update = 0;

            SOCAPI_TouchKeyStatus &= 0x7F;      // 清除扫描完成标志位
            g_touch_key_flag = TouchKeyScan();  // 读取当前通道按键值掩码
            TouchKeyRestart();                  // 启动下一次电容扫描转换

            //------------------------------------------------------------------
            // K2 (TK15 / P2.7): 独立照明 -> 点按开，再次点按关，翻转 LED2
            //------------------------------------------------------------------
            if (g_touch_key_flag & TK_MASK_K2)
            {
                if (!s_k2_latched)
                {
                    s_k2_latched = 1;
                    Host_ToggleLight();          // 切换照明硬件专线 (P5.1)
                    Display_SetLed2(g_bLightOn); // 同时翻转对应按键背光 LED2 (P0.7)
                    BSP_Beep(155);               // 触发 155ms 4.0kHz 蜂鸣器应答
                    need_update = 1;
                }
            }
            else
            {
                s_k2_latched = 0;
            }

            //------------------------------------------------------------------
            // K3 (TK16 / P1.7): 加档 -> 1档 -> 2档 -> 3档 (仅开机状态下生效)
            //------------------------------------------------------------------
            if (g_touch_key_flag & TK_MASK_K3)
            {
                if (!s_k3_latched)
                {
                    s_k3_latched = 1;
                    if (g_bPowerOn)
                    {
                        Host_SpeedUp();   // 升档并突发发送 3 帧控制报文
                        BSP_Beep(155);    // 触发 155ms 4.0kHz 蜂鸣器应答
                        need_update = 1;
                    }
                }
            }
            else
            {
                s_k3_latched = 0;
            }

            //------------------------------------------------------------------
            // K4 (TK12 / P2.4): 减档 -> 3档 -> 2档 -> 1档 (仅开机状态下生效)
            //------------------------------------------------------------------
            if (g_touch_key_flag & TK_MASK_K4)
            {
                if (!s_k4_latched)
                {
                    s_k4_latched = 1;
                    if (g_bPowerOn)
                    {
                        Host_SpeedDown(); // 降档并突发发送 3 帧控制报文
                        BSP_Beep(155);    // 触发 155ms 4.0kHz 蜂鸣器应答
                        need_update = 1;
                    }
                }
            }
            else
            {
                s_k4_latched = 0;
            }

            //------------------------------------------------------------------
            // K5 (TK13 / P2.5): 电源键 -> 短按关机(或开机) / 长按2秒进入15分钟延时
            //------------------------------------------------------------------
            if (g_touch_key_flag & TK_MASK_K5)
            {
                if (!s_k5_is_pressed)
                {
                    s_k5_is_pressed = 1;
                    s_k5_press_ms = 0;

                    if (!g_bPowerOn)
                    {
                        // 关机状态下点按 K5: 立即开机进入 1 档
                        Host_PowerOn();
                        BSP_Beep(155);
                        s_k5_long_triggered = 1; // 本次开机按下不计入长按
                        need_update = 1;
                    }
                    else
                    {
                        // 开机状态下按下: 启动长按2秒计时
                        s_k5_long_triggered = 0;
                    }
                }
            }
            else
            {
                if (s_k5_is_pressed)
                {
                    // 手指离开释放按键
                    if (g_bPowerOn && !s_k5_long_triggered)
                    {
                        // 开机状态下，按住未满 2 秒即松开: 短按执行关机
                        Host_PowerOff();
                        BSP_Beep(155);
                        need_update = 1;
                    }
                    s_k5_is_pressed = 0;
                    s_k5_long_triggered = 0;
                    s_k5_press_ms = 0;
                }
            }

            // 状态改变时立即刷新指示灯与数码管缓冲
            if (need_update)
            {
                Display_Update();
            }
        }

        //----------------------------------------------------------------------
        // 2. 300ms 心跳: 周期向主电脑板广播当前状态帧
        //----------------------------------------------------------------------
        if (g_b300ms_flag)
        {
            g_b300ms_flag = 0;
            Host_SendHeartbeat();
        }

        //----------------------------------------------------------------------
        // 3. 10ms 周期: 维护数码管与 LED 刷新缓冲 & K5长按2秒判定
        //----------------------------------------------------------------------
        if (g_b10ms_flag)
        {
            g_b10ms_flag = 0;
            Display_Update();

            // K5 长按 2 秒判定 (仅在开机状态下有效)
            if (s_k5_is_pressed && g_bPowerOn && !s_k5_long_triggered)
            {
                s_k5_press_ms += 10;
                if (s_k5_press_ms >= 2000) // 持续按住达到 2000ms (2.0 秒)
                {
                    s_k5_long_triggered = 1;
                    Host_StartDelay(); // 启动 15 分钟延时模式 (900秒倒计时)
                    BSP_Beep(250);     // 蜂鸣器长鸣 250ms 提示成功进入延时模式
                    Display_Update();  // 立即刷新, 电源键 LED 进入闪烁
                }
            }
        }

        //----------------------------------------------------------------------
        // 4. 1秒滴答: 维护 15 分钟倒计时关机 (如激活)
        //----------------------------------------------------------------------
        if (g_b1s_flag)
        {
            g_b1s_flag = 0;
            Host_Delay1sTick();
        }
    }
}
