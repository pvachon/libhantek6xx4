#pragma once

/**
 * Software reset register. Set bit 0 to 1 to reset all registers to defaults
 */
#define HMCAD1511_REG_RST                   0x0
#define HMCAD1511_REG_RST_RST               0x1



#define HMCAD1511_REG_SLEEP_PD              0x0f
#define HMCAD1511_REG_SLEEP_PD_PD           (1 << 9)

#define HMCAD1511_REG_LVDS_TERM             0x12
#define HMCAD1511_REG_CGAIN4                0x2a
#define HMCAD1511_REG_CGAIN2_1              0x2b
#define HMCAD1511_REG_JITTER_CTRL           0x30
#define HMCAD1511_REG_CHAN_NUM_CLK_DIV      0x31
#define HMCAD1511_REG_GAIN_CONTROL          0x33
#define HMCAD1511_REG_INP_SEL_CH_LO         0x3a
#define HMCAD1511_REG_INP_SEL_CH_HI         0x3b

#define HMCAD1511_REG_FS_CNTRL              0x55

