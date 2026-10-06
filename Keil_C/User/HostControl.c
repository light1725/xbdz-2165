/*****************************************************************************************************
 * @file    HostControl.c
 * @brief   兴邦电器 0010902165 按键板与 0010902163 主电脑板通信与状态控制引擎实现
 * @details 10字节 2400bps 协议打包、CRC-16/X-25校验、300ms心跳、风机调速、15分钟延时、独立照明
 *****************************************************************************************************/
#include "HostControl.h"
#include "Display.h"

// 全局业务状态
WindGear_t g_current_gear = WIND_GEAR_OFF; // 上电默认关机停风
bit g_bPowerOn = 0;                         // 系统电源状态 (0: 关机, 1: 开机)
bit g_bLightOn = 0;                         // 独立照明状态 (0: 关, 1: 开)
bit g_bDelayActive = 0;                     // 15分钟延时关机模式激活状态
unsigned int g_delay_sec_remain = 0;        // 延时倒计时剩余秒数
static unsigned char s_poweroff_delay_sec = 0; // 关机后电机电源保持使能 30 秒倒计时

/**
 * @brief  工业级 CRC-16 / X-25 校验算法
 * @param  pData: 数据源指针
 * @param  len: 校验数据字节长度 (本协议为前 8 个字节)
 * @retval 16位校验码 (高字节送 Byte 8, 低字节送 Byte 9)
 * @note   多项式: 0x1021 (倒序 0x8408), 初始值: 0xFFFF, 异或输出: 0xFFFF
 */
unsigned int Get_CRC16_X25(unsigned char *pData, unsigned char len)
{
    unsigned int crc = 0xFFFF;
    unsigned char i, j;

    for (i = 0; i < len; i++)
    {
        crc ^= pData[i];
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0x8408;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return (crc ^ 0xFFFF);
}

/**
 * @brief  状态控制引擎初始化
 */
void Host_Init(void)
{
    g_current_gear = WIND_GEAR_OFF;
    g_bPowerOn = 0;
    g_bLightOn = 0;
    g_bDelayActive = 0;
    g_delay_sec_remain = 0;
    s_poweroff_delay_sec = 0;

    PIN_LIGHT_OUT = 0; // 默认照明关闭
    PIN_P50_OUT = 0;
    PIN_P14_OUT = 0;   // 默认关机电机电源使能关闭 (0V)
}

/**
 * @brief  构建标准的 10 字节控制帧
 * @param  gear: 目标档位 (0: 关机, 1: 1档, 2: 2档, 3: 3档)
 * @param  out_buf: 接收组包的 10 字节输出缓冲区
 */
void Host_BuildFrame(WindGear_t gear, unsigned char *out_buf)
{
    unsigned int target_rpm = 0;
    unsigned int crc_val = 0;

    if (gear == WIND_GEAR_OFF)
    {
        // 关机帧: 10 00 00 00 00 00 00 41 8B 86
        out_buf[0] = 0x10;
        target_rpm = MOTOR_RPM_OFF;
        out_buf[3] = 0x00;
        out_buf[4] = 0x00;
    }
    else
    {
        // 开机运行帧: 19 [RPM_H] [RPM_L] 07 D0 00 00 41 [CRC_H] [CRC_L]
        out_buf[0] = 0x19;
        
        switch (gear)
        {
            case WIND_GEAR_1:
                target_rpm = MOTOR_RPM_GEAR1; // 1050 RPM (0x041A)
                break;
            case WIND_GEAR_2:
                target_rpm = MOTOR_RPM_GEAR2; // 1350 RPM (0x0546)
                break;
            case WIND_GEAR_3:
                target_rpm = MOTOR_RPM_GEAR3; // 1800 RPM (0x0708)
                break;
            default:
                target_rpm = MOTOR_RPM_GEAR1;
                break;
        }

        // 最高转速限制固定 2000 RPM (0x07D0)
        out_buf[3] = 0x07;
        out_buf[4] = 0xD0;
    }

    out_buf[1] = (unsigned char)(target_rpm >> 8);
    out_buf[2] = (unsigned char)(target_rpm & 0xFF);
    out_buf[5] = 0x00;
    out_buf[6] = 0x00;
    out_buf[7] = 0x41; // 从机固定目标地址 'A'

    // 计算前 8 个字节的 CRC-16/X-25 校验码
    crc_val = Get_CRC16_X25(out_buf, 8);
    out_buf[8] = (unsigned char)(crc_val >> 8);   // CRC 高字节
    out_buf[9] = (unsigned char)(crc_val & 0xFF); // CRC 低字节
}

/**
 * @brief  向主电脑板发送当前档位的心跳控制帧 (每300ms调用)
 */
void Host_SendHeartbeat(void)
{
    unsigned char tx_buf[10];
    Host_BuildFrame(g_current_gear, tx_buf);
    UART1_SendBuffer(tx_buf, 10);
}

/**
 * @brief  按键触发时连续发送 3 帧报文 (间隔10ms), 确保主电脑板可靠接收并触发蜂鸣器确认
 */
void Host_SendKeyBurst(void)
{
    unsigned char i;
    for (i = 0; i < 3; i++)
    {
        Host_SendHeartbeat();
        if (i < 2)
        {
            Delay_ms(10);
        }
    }
}

/**
 * @brief  开机操作
 * @note   按键1为电源键: 按一下, 电机进入低速状态显示窗显示1, 电源键变白色
 */
void Host_PowerOn(void)
{
    g_bPowerOn = 1;
    g_current_gear = WIND_GEAR_1; // 开机默认启动 1 档低速 (1050 RPM)
    Host_CancelDelay();           // 清除延时状态
    s_poweroff_delay_sec = 0;     // 清除关机延时倒计时
    PIN_P14_OUT = 1;              // 核心使能: Pin 7 立即输出 5V 高电平打开电脑板电机电源
    Delay_ms(20);                 // 预留 20ms 给电脑板电源建压稳定
    Display_SetLed5(1);           // 点亮电源键白色背光
    Host_SendKeyBurst();          // 连续发送 3 帧通知主板开机运转
}

/**
 * @brief  关机操作
 * @note   再按一下电源键关机, 风机停转, 显示窗熄灭, 不影响照明
 */
void Host_PowerOff(void)
{
    g_bPowerOn = 0;
    g_current_gear = WIND_GEAR_OFF; // 电机关机 (0 RPM)
    Host_CancelDelay();             // 取消延时
    Display_SetLed5(0);             // 熄灭电源背光
    Host_SendKeyBurst();            // 连续发送 3 帧关机帧
    s_poweroff_delay_sec = 30;      // 启动 30 秒倒计时: 期间保持 Pin 7 (P1.4) 为 5V 供电机平稳减速停转
}

/**
 * @brief  升速操作 (按键 4)
 * @note   按一下电机升速: 1档->2档->3档; 处于3档(高速)状态按此键不起作用
 */
void Host_SpeedUp(void)
{
    if (!g_bPowerOn)
    {
        return;
    }

    if (g_current_gear == WIND_GEAR_1)
    {
        g_current_gear = WIND_GEAR_2; // 1档升至2档 (1350 RPM)
        Host_SendKeyBurst();
    }
    else if (g_current_gear == WIND_GEAR_2)
    {
        g_current_gear = WIND_GEAR_3; // 2档升至3档 (1800 RPM)
        Host_SendKeyBurst();
    }
    // 处于 3 档时不起作用
}

/**
 * @brief  降速操作 (按键 2)
 * @note   按一下电机降速: 3档->2档->1档; 处于1档(低速)状态按此键不起作用
 */
void Host_SpeedDown(void)
{
    if (!g_bPowerOn)
    {
        return;
    }

    if (g_current_gear == WIND_GEAR_3)
    {
        g_current_gear = WIND_GEAR_2; // 3档降至2档 (1350 RPM)
        Host_SendKeyBurst();
    }
    else if (g_current_gear == WIND_GEAR_2)
    {
        g_current_gear = WIND_GEAR_1; // 2档降至1档 (1050 RPM)
        Host_SendKeyBurst();
    }
    // 处于 1 档时风机不降速
}

/**
 * @brief  照明按键触发 (按键 5)
 * @note   此键独立, 不受电源键控制; 按一下全亮, 再按一下灭
 */
void Host_ToggleLight(void)
{
    g_bLightOn = !g_bLightOn;
    PIN_LIGHT_OUT = g_bLightOn ? 1 : 0; // 硬件电平翻转
}

/**
 * @brief  设置照明状态
 */
void Host_SetLight(bit on)
{
    g_bLightOn = on;
    PIN_LIGHT_OUT = g_bLightOn ? 1 : 0;
}

/**
 * @brief  启动 15 分钟延时模式 (长按开关键 1 两秒触发)
 * @note   延时15分钟后风速停止关机; 延时期间可随意调整风速, 倒计时不受阻碍
 */
void Host_StartDelay(void)
{
    if (g_bPowerOn && (g_current_gear != WIND_GEAR_OFF))
    {
        g_bDelayActive = 1;
        g_delay_sec_remain = DELAY_TOTAL_SECONDS; // 900 秒倒计时
    }
}

/**
 * @brief  取消延时模式
 */
void Host_CancelDelay(void)
{
    g_bDelayActive = 0;
    g_delay_sec_remain = 0;
}

/**
 * @brief  1 秒系统延时倒计时心跳 (由 Timer0 驱动)
 */
void Host_Delay1sTick(void)
{
    if (s_poweroff_delay_sec > 0)
    {
        s_poweroff_delay_sec--;
        if (s_poweroff_delay_sec == 0)
        {
            PIN_P14_OUT = 0; // 30秒倒计时结束, 拉低 Pin 7 (P1.4) 彻底切断电脑板电机电源
        }
    }

    if (g_bDelayActive)
    {
        if (g_delay_sec_remain > 0)
        {
            g_delay_sec_remain--;
            if (g_delay_sec_remain == 0)
            {
                // 15 分钟计时结束, 自动关机并停机
                Host_PowerOff();
                BSP_Beep(250); // 延时结束停机蜂鸣提示
            }
        }
    }
}
