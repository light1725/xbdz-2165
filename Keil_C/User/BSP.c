/*****************************************************************************************************
 * @file    BSP.c
 * @brief   兴邦电器 0010902165 按键板底层板级支持包实现
 * @details 硬件GPIO初始化、Timer0 1ms系统滴答、UART1 (SSI) 2400bps发送实现
 *****************************************************************************************************/
#include <intrins.h>
#include "BSP.h"
#include "Display.h"

// 全局滴答计时与事件标志
volatile unsigned int g_systick_ms = 0;
volatile bit g_b300ms_flag = 0;
volatile bit g_b10ms_flag = 0;
volatile bit g_b1s_flag = 0;

static unsigned int s_300ms_counter = 0;
static unsigned char s_10ms_counter = 0;
static unsigned int s_1s_counter = 0;

/**
 * @brief  系统硬件总初始化
 */
void BSP_Init(void)
{
    // 1. 关闭总中断
    EA = 0;

    // 2. 配置 P5.1 (照明控制) 为强推挽输出
    P5CON |= (1 << 1);      // P5.1 强推挽输出
    PIN_LIGHT_OUT = 0;      // 默认照明关闭 (0V)

    // Pin 6 (P5.0) 蜂鸣器应答驱动专线: 初始化为强推挽输出并配置 Timer1 产生 4.0kHz 方波
    BSP_Buzzer_Init();

    // Pin 7 (P1.4) 电机电源/变频使能专线: 配置为强推挽输出，默认关机低电平 (0V)
    P1CON |= (1 << 4);
    P1PH  &= ~(1 << 4);
    PIN_P14_OUT = 0;

    // 3. 配置 P1.1 (3528 LED 共阴极使能) 为强推挽输出
    P1CON |= (1 << 1);
    PIN_LED_CAT_COM = 1;    // 默认高电平, 3528 按键灯全部熄灭

    // 4. 配置 P2.2 (数码管公共COM) 为强推挽输出
    P2CON |= (1 << 2);
    PIN_SEG_COM = 1;        // 默认高电平截止关闭

    // 5. 配置 P0.1 ~ P0.7 (数码管段选 a~g & 3528 LED 阳极) 为强推挽输出
    //    配置 P0.0 (数码管小数点 dp): 高阻输入模式且禁用内部上拉 (P0CON.0 = 0, P0PH.0 = 0),
    //    彻底断开小数点引脚回路, 杜绝开机、关机及任何档位下小数点点亮
    P0CON = 0xFE;           // P0.1~P0.7 强推挽输出, P0.0 高阻输入
    P0PH  &= ~0x01;         // 禁用 P0.0 内部上拉 (浮空高阻)
    P0VO  = 0x00;           // 关闭 P0 LCD 偏置
    P0    = 0x00;           // 初始全低

    // 6. 配置触摸按键引脚为高阻输入模式 (禁用内部上拉，由赛元 TouchKeyInit 自动接管)
    P1CON &= ~(1 << 7);     // P1.7 (K1 电源键 TK16) 输入
    P1PH  &= ~(1 << 7);     // 禁用内部上拉，保证电容测量高灵敏度
    
    P2CON &= ~((1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3)); // P2.7(TK15), P2.6(TK14), P2.5(TK13), P2.4(TK12), P2.3(TK11) 输入
    P2PH  &= ~((1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3)); // 禁用内部上拉

    // 7. 初始化 Timer0 (1ms 中断基准)
    BSP_Timer0_Init();

    // 8. 初始化 UART1 (SSI 2400bps 异步串口)
    BSP_UART1_Init();

    // 9. 开启总中断
    EA = 1;
}

volatile unsigned int g_beep_ticks = 0; // 蜂鸣器半周期倒计时计数

/**
 * @brief  初始化 Pin 6 (P5.0) 蜂鸣器驱动与 Timer1 (4.0kHz 载波产生)
 * @note   Fsys=12MHz, 12T模式下计数时钟为 1MHz (1us/tick)
 *         4.0kHz 方波周期为 250us (半周期 125us)
 *         Timer1 工作在模式 2 (8位自动重装载), 重装载初值 = 256 - 125 = 131 (0x83)
 */
void BSP_Buzzer_Init(void)
{
    // 1. P5.0 配置为强推挽输出, 默认输出 0V (静音状态)
    P5CON |= (1 << 0);
    P5PH  &= ~(1 << 0);
    PIN_P50_OUT = 0;

    // 2. Timer1 12T 模式 (TMCON.1 = 0)
    TMCON &= ~0x02;

    // 3. Timer1 模式 2 (8位自动重装载): TMOD 高 4 位清零后置入 0x20
    TMOD &= 0x0F;
    TMOD |= 0x20;

    // 4. 重装初值 125us (1MHz 下 125 个计数脉冲)
    TH1 = 256 - 125; // 0x83
    TL1 = 256 - 125;

    TF1 = 0;
    ET1 = 1;  // 使能 Timer1 中断
    IPT1 = 1; // 关键: 设置 Timer1 为高优先级中断, 抢占 Timer0 的 100us 扫描延时, 彻底消除 1kHz 杂音与破音
    TR1 = 0;  // 初始停止
    g_beep_ticks = 0;
}

/**
 * @brief  触发按键应答蜂鸣声 (4.0kHz 方波)
 * @param  duration_ms: 发声持续时间 (毫秒, 原厂实测为 155ms)
 */
void BSP_Beep(unsigned int duration_ms)
{
    // 每毫秒包含 8 个 125us 半周期 (1ms / 0.125ms = 8)
    // 155ms 对应 155 * 8 = 1240 次翻转
    g_beep_ticks = duration_ms * 8;
    TR1 = 1; // 启动 Timer1 产生 4.0kHz 方波
}

/**
 * @brief  初始化 Timer0 为 1ms 自动定时中断 (Fsys=12MHz, 1T 模式)
 */
void BSP_Timer0_Init(void)
{
    // 配置 Timer0 为工作模式 1 (16位定时器)
    TMOD &= 0xF0;
    TMOD |= 0x01;

    // 配置 Timer0 时钟源为 Fsys (1T 模式: 16MHz)
    TMCON |= 0x01;

    // 1T 模式下 1ms 定时初值装载
    TH0 = (unsigned char)(TIMER0_RELOAD_VAL >> 8);
    TL0 = (unsigned char)(TIMER0_RELOAD_VAL & 0xFF);

    // 清除中断标志，使能 Timer0 中断
    TF0 = 0;
    ET0 = 1;
    TR0 = 1; // 启动定时器
}

/**
 * @brief  初始化 UART1 (基于 SC92F8463B 的 SSI 串口模式)
 * @note   通信引脚: P2.0 (TX1), P2.1 (RX1)
 *         参数: 2400 bps, 8数据位, 1停止位, 无校验
 */
void BSP_UART1_Init(void)
{
    // P2.0 (TX1) 配置为强推挽输出 (P2CON.0 = 1, P2PH.0 = 0), 保证强劲输出驱动能力与标准陡峭数字边沿 (彻底杜绝电平浮空与波形畸变)
    // P2.1 (RX1) 配置为输入带上拉 (P2CON.1 = 0, P2PH.1 = 1)
    P2CON |= (1 << 0);
    P2PH  &= ~(1 << 0);
    P2CON &= ~(1 << 1);
    P2PH  |= (1 << 1);

    // 启用 SSI 的 UART1 映射到 P2.0 / P2.1
    OTCON |= 0xC0;

    // SSCON0: 模式一 (8位异步UART通信模式: 1位起始位(低), 8位数据位, 1位停止位(高))
    SSCON0 = 0x50;

    // 波特率分频寄存器配置:
    // 分频值 = Fsys / BaudRate = 12000000 / 2400 = 5000 = 0x1388 (0.000% 误差)
    SSCON2 = (unsigned char)(UART1_BAUD_DIV_VAL >> 8);
    SSCON1 = (unsigned char)(UART1_BAUD_DIV_VAL & 0xFF);
}

/**
 * @brief  UART1 发送单字节 (硬件轮询)
 * @param  dat: 待发送字节
 */
void UART1_SendByte(unsigned char dat)
{
    SSCON0 &= ~0x02; // 清除发送完成标志位 TI
    SSDAT = dat;     // 写入数据寄存器启动发送
    while ((SSCON0 & 0x02) == 0); // 等待发送完毕
    SSCON0 &= ~0x02; // 清除 TI 标志位
}

/**
 * @brief  UART1 连续发送数据缓冲区
 * @param  buf: 数据指针
 * @param  len: 发送长度 (本协议固定 10 字节)
 */
void UART1_SendBuffer(unsigned char *buf, unsigned char len)
{
    unsigned char i;
    for (i = 0; i < len; i++)
    {
        UART1_SendByte(buf[i]);
    }
}

/**
 * @brief  微秒级轻量延时
 */
void Delay_10us(void)
{
    unsigned char i;
    for (i = 0; i < 20; i++)
    {
        _nop_();
    }
}

/**
 * @brief  毫秒级阻塞延时
 */
void Delay_ms(unsigned int ms)
{
    unsigned int i, j;
    for (i = 0; i < ms; i++)
    {
        for (j = 0; j < 1600; j++)
        {
            _nop_();
        }
    }
}

/**
 * @brief  Timer0 1ms 定时器中断处理函数 (由 SC_it.c 的 Timer0Interrupt 调用)
 */
void BSP_Timer0_IRQHandler(void)
{
    // 重装载 1ms 初值
    TH0 = (unsigned char)(TIMER0_RELOAD_VAL >> 8);
    TL0 = (unsigned char)(TIMER0_RELOAD_VAL & 0xFF);

    // 1ms 系统滴答累加
    g_systick_ms++;

    // 驱动数码管与3528 LED分时扫描
    Display_Scan_Tick();

    // 10ms 逻辑扫描调度
    if (++s_10ms_counter >= 10)
    {
        s_10ms_counter = 0;
        g_b10ms_flag = 1;
    }

    // 300ms 心跳广播调度 (油烟机标准通信周期)
    if (++s_300ms_counter >= 300)
    {
        s_300ms_counter = 0;
        g_b300ms_flag = 1;
    }

    // 1秒滴答计时 (用于 15 分钟延时关机倒计时)
    if (++s_1s_counter >= 1000)
    {
        s_1s_counter = 0;
        g_b1s_flag = 1;
    }
}
