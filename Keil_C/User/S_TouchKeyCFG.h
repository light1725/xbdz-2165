#ifndef __S_TOUCHKEYCFG_H__
#define __S_TOUCHKEYCFG_H__

// MySetting.tkp 导入配置参数 (4通道: TK12, TK13, TK15, TK16)
#define SOCAPI_SET_TOUCHKEY_TOTAL          4
#define SOCAPI_SET_TOUCHKEY_CHANNEL        0x0001B000
#define SOCAPI_SET_TOUCHKEY_CHANNEL2       0x00000000

/*
 * TKCFG[17]:
 * [1]  IsDoubleKey           = 0
 * [3]  CONFIRMTOUCHCNT       = 5
 * [4]  INIT_AUTO_UPDATE_TIME = 10
 * [5]  SET_KEY_CONTI_TIME    = 3000
 * [6]  SET_SYNC_UPDATE       = 200
 * [7]  SET_UPDATE_SPEED      = 100
 * [8]  AUTO_UPDATE_TIME      = 2
 * [16] NOISE                 = 118
 */
unsigned int code TKCFG[17] = {0,0,0,5,10,3000,200,100,2,0,0,4,0,1,65535,65535,118}; 

/*
 * TKChannelCfg[4][8]:
 * Ch0: TK12 (P2.4) -> ICHA: 0x14, 阈值: 0x0774 (1908)
 * Ch1: TK13 (P2.5) -> ICHA: 0x18, 阈值: 0x07D0 (2000)
 * Ch2: TK15 (P2.7) -> ICHA: 0x15, 阈值: 0x0760 (1888)
 * Ch3: TK16 (P1.7) -> ICHA: 0x12, 阈值: 0x07A8 (1960)
 */
unsigned char code TKChannelCfg[4][8] = {
    0x02,0x32,0x04,0x08,0x14,0x05,0x07,0x74, // TK12 (P2.4)
    0x02,0x32,0x04,0x08,0x18,0x05,0x07,0xd0, // TK13 (P2.5)
    0x02,0x32,0x04,0x08,0x15,0x05,0x07,0x60, // TK15 (P2.7)
    0x02,0x32,0x04,0x08,0x12,0x05,0x07,0xa8  // TK16 (P1.7)
};

#endif
