/*****************************************************************************************************
 * @file    Display.c
 * @brief   0010902165 LED & 7-Segment Display Driver
 *****************************************************************************************************/
#include "Display.h"

/* Display buffers */
static unsigned char s_seg_output = 0x00;  /* Current 7-segment output (to P0) */
static unsigned char s_led_output = 0x00;  /* Current 3528 LED output (to P0) */
static unsigned int  s_blink_timer = 0;    /* Delay mode blink timer (ms) */
static bit           s_blink_state = 1;    /* Blink toggle flag */

/* LED state flags */
bit g_bLed2On = 0; /* LED2: P0.7 (K2 Light Key indicator) */
bit g_bLed3On = 0; /* LED3: P0.6 (K3 Inc Key backlight) */
bit g_bLed4On = 0; /* LED4: P0.3 (K4 Dec Key backlight) */
bit g_bLed5On = 0; /* LED5: P0.1 (K5 Power Key backlight) */

#define SEG_CODE_BLANK      0x00

/*
 * 7-Segment Pin Mapping (100% verified):
 * a (Top horizontal):    P0.3 (bit 3, 0x08)
 * b (Top-right vertical): P0.7 (bit 7, 0x80)
 * c (Bot-right vertical): P0.4 (bit 4, 0x10)
 * d (Bottom horizontal):  P0.5 (bit 5, 0x20)
 * e (Bot-left vertical):  P0.1 (bit 1, 0x02)
 * f (Top-left vertical):  P0.2 (bit 2, 0x04)
 * g (Middle horizontal):  P0.6 (bit 6, 0x40)
 * dp (Decimal point):     P0.0 (bit 0, 0x01)
 */
#define SEG_SEG_A           (1 << 3) /* P0.3: a */
#define SEG_SEG_B           (1 << 7) /* P0.7: b */
#define SEG_SEG_C           (1 << 4) /* P0.4: c */
#define SEG_SEG_D           (1 << 5) /* P0.5: d */
#define SEG_SEG_E           (1 << 1) /* P0.1: e */
#define SEG_SEG_F           (1 << 2) /* P0.2: f */
#define SEG_SEG_G           (1 << 6) /* P0.6: g */
#define SEG_SEG_DP          (1 << 0) /* P0.0: dp */

/* Digit 1: b + c (0x80 | 0x10 = 0x90) */
#define SEG_CODE_1          (SEG_SEG_B | SEG_SEG_C)
/* Digit 2: a + b + g + e + d (0x08 | 0x80 | 0x40 | 0x02 | 0x20 = 0xEA) */
#define SEG_CODE_2          (SEG_SEG_A | SEG_SEG_B | SEG_SEG_G | SEG_SEG_E | SEG_SEG_D)
/* Digit 3: a + b + g + c + d (0x08 | 0x80 | 0x40 | 0x10 | 0x20 = 0xF8) */
#define SEG_CODE_3          (SEG_SEG_A | SEG_SEG_B | SEG_SEG_G | SEG_SEG_C | SEG_SEG_D)

/* 3528 LED Anode Mapping (P0) */
#define LED_MASK_LED1       (1 << 5) /* LED1: P0.5 */
#define LED_MASK_LED2       (1 << 7) /* LED2: P0.7 (K2) */
#define LED_MASK_LED3       (1 << 6) /* LED3: P0.6 (K3) */
#define LED_MASK_LED4       (1 << 3) /* LED4: P0.3 (K4) */
#define LED_MASK_LED5       (1 << 1) /* LED5: P0.1 (K5) */
#define LED_MASK_LED6       (1 << 2) /* LED6: P0.2 */

/**
 * @brief  Initialize display driver
 */
void Display_Init(void)
{
    s_seg_output = SEG_CODE_BLANK;
    s_led_output = 0x00;
    s_blink_timer = 0;
    s_blink_state = 1;
    g_bLed2On = 0;
    g_bLed3On = 0;
    g_bLed4On = 0;
    g_bLed5On = 0;

    /* P0.0 (数码管小数点) 强制设为高阻输入且禁用上拉, 硬件断开回路, 杜绝任何状态下误点亮 */
    P0CON &= ~0x01;
    P0PH  &= ~0x01;

    PIN_SEG_COM = 1;        /* 7-Seg COM off (high) */
    PIN_LED_CAT_COM = 1;    /* 3528 LED COM off (high) */
    P0 = 0x00;              /* P0 all low */
}

/**
 * @brief  Toggle / Set LED2 state (K2 Light Key)
 */
void Display_ToggleLed2(void)
{
    g_bLed2On = !g_bLed2On;
}

void Display_SetLed2(bit state)
{
    g_bLed2On = state;
}

/**
 * @brief  Toggle / Set LED3 state (K3 Inc Key)
 */
void Display_ToggleLed3(void)
{
    g_bLed3On = !g_bLed3On;
}

void Display_SetLed3(bit state)
{
    g_bLed3On = state;
}

/**
 * @brief  Toggle / Set LED4 state (K4 Dec Key)
 */
void Display_ToggleLed4(void)
{
    g_bLed4On = !g_bLed4On;
}

void Display_SetLed4(bit state)
{
    g_bLed4On = state;
}

/**
 * @brief  Toggle / Set LED5 state (K5 Power Key)
 */
void Display_ToggleLed5(void)
{
    g_bLed5On = !g_bLed5On;
}

void Display_SetLed5(bit state)
{
    g_bLed5On = state;
}

/**
 * @brief  Update display output buffers (called in main loop)
 */
void Display_Update(void)
{
    unsigned char seg_code = SEG_CODE_BLANK;
    unsigned char led_code = 0x00;

    /* 1. Blink timer in delay-off mode (toggle every 500ms) */
    if (g_bDelayActive)
    {
        s_blink_timer += 10;
        if (s_blink_timer >= 500)
        {
            s_blink_timer = 0;
            s_blink_state = !s_blink_state;
        }
    }
    else
    {
        s_blink_timer = 0;
        s_blink_state = 1;
    }

    /* 2. Power on state: 7-segment digit & 3528 key lights */
    if (g_bPowerOn)
    {
        led_code |= LED_MASK_LED1; /* LED1 (P0.5) Power indicator */
        led_code |= LED_MASK_LED3; /* LED3 (P0.6) K3 Inc key */
        led_code |= LED_MASK_LED4; /* LED4 (P0.3) K4 Dec key */

        /* LED5 (P0.1) K5 Power key: steady on, blinks in delay mode */
        if (!g_bDelayActive || s_blink_state)
        {
            led_code |= LED_MASK_LED5;
        }

        /* Gear digit display: 1, 2, 3 */
        switch (g_current_gear)
        {
            case WIND_GEAR_1:
                seg_code = SEG_CODE_1;
                break;

            case WIND_GEAR_2:
                seg_code = SEG_CODE_2;
                break;

            case WIND_GEAR_3:
                seg_code = SEG_CODE_3;
                break;

            default:
                seg_code = SEG_CODE_BLANK;
                break;
        }
    }
    else
    {
        /* Power off: 7-segment blank */
        seg_code = SEG_CODE_BLANK;
    }

    /* 3. K2 Light key indicator (LED2: P0.7) */
    if (g_bLed2On)
    {
        led_code |= LED_MASK_LED2;
    }

    /* 强制清零 bit 0, 确保任何时候 s_seg_output 与 s_led_output 都不会激活 P0.0 (小数点) */
    s_seg_output = seg_code & 0xFE;
    s_led_output = led_code & 0xFE;
}

/**
 * @brief  Time-division multiplexed display scan tick (called from Timer0 1ms ISR)
 * @note   500Hz refresh rate, 15% brightness (150us active pulse per 1ms slot, zero flicker)
 */
void Display_Scan_Tick(void)
{
    static unsigned char s_scan_slot = 0;
    unsigned char i;

    /* 1. Blanking: turn off all COM pins and clear P0 */
    PIN_SEG_COM = 1;
    PIN_LED_CAT_COM = 1;
    P0 = 0x00;

    /* 2. Time-division multiplexing with 10% brightness pulse */
    if (s_scan_slot == 0)
    {
        /* Slot 0: 7-segment display */
        if (s_seg_output != 0)
        {
            P0 = s_seg_output & 0xFE;  /* Output segment anodes (high), bit 0 strictly cleared */
            PIN_SEG_COM = 0;           /* Enable 7-seg COM (low) */

            /* 100us pulse = 10% of 1ms slot */
            for (i = 0; i < 10; i++)
            {
                Delay_10us();
            }

            /* Blanking for remainder of slot */
            PIN_SEG_COM = 1;
            P0 = 0x00;
        }
        s_scan_slot = 1;
    }
    else
    {
        /* Slot 1: 3528 key backlights */
        if (s_led_output != 0)
        {
            P0 = s_led_output & 0xFE;      /* Output LED anodes (high), bit 0 strictly cleared */
            PIN_LED_CAT_COM = 0;           /* Enable 3528 COM (low) */

            /* 100us pulse = 10% of 1ms slot */
            for (i = 0; i < 10; i++)
            {
                Delay_10us();
            }

            /* Blanking for remainder of slot */
            PIN_LED_CAT_COM = 1;
            P0 = 0x00;
        }
        s_scan_slot = 0;
    }
}
