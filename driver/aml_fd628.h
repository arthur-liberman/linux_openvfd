/**
 * FD628 address range is 00 ~ 0D
 *	Address		display byte
 *	00	01	GRID1
 *	02	03	GRID2
 *	04	05	GRID3
 *	06	07	GRID4
 *	08	09	GRID5
 *	0a	0b	GRID6
 *	0c	0d	GRID7
 */

#ifndef __AML_FD628H__
#define __AML_FD628H__

#ifdef MODULE
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/of.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/switch.h>
#include <linux/time.h>
#endif
#include "glyphs.h"

#ifdef MODULE
#if 0
#define pr_dbg(args...) printk(KERN_ALERT "FD628: " args)
#else
#define pr_dbg(args...)
#endif
#endif

#define pr_error(args...) printk(KERN_ALERT "FD628: " args)
#define pr_dbg2(args...) printk(KERN_DEBUG "FD628: " args)

#ifndef CONFIG_OF
#define CONFIG_OF
#endif

#define  FD628_DRIVER_VERSION	"V1.1.0"

/*
 * Ioctl definitions
 */

/* Use 'M' as magic number */
#define FD628_IOC_MAGIC			'M'
#define FD628_IOC_SMODE			_IOW(FD628_IOC_MAGIC,  1, int)
#define FD628_IOC_GMODE			_IOR(FD628_IOC_MAGIC,  2, int)
#define FD628_IOC_SBRIGHT		_IOW(FD628_IOC_MAGIC,  3, int)
#define FD628_IOC_GBRIGHT		_IOR(FD628_IOC_MAGIC,  4, int)
#define FD628_IOC_POWER			_IOW(FD628_IOC_MAGIC,  5, int)
#define FD628_IOC_GVER			_IOR(FD628_IOC_MAGIC,  6, int)
#define FD628_IOC_STATUS_LED		_IOW(FD628_IOC_MAGIC,  7, int)
#define FD628_IOC_GDISPLAY_TYPE		_IOR(FD628_IOC_MAGIC,  8, int)
#define FD628_IOC_SDISPLAY_TYPE		_IOW(FD628_IOC_MAGIC,  9, int)
#define FD628_IOC_SCHARS_ORDER		_IOW(FD628_IOC_MAGIC, 10, u_int8[7])
#define FD628_IOC_USE_DTB_CONFIG	_IOW(FD628_IOC_MAGIC, 11, int)
#define FD628_IOC_MAXNR			12

#ifdef MODULE

#define MOD_NAME_CLK       "fd628_gpio_clk"
#define MOD_NAME_DAT       "fd628_gpio_dat"
#define MOD_NAME_STB       "fd628_gpio_stb"
#define MOD_NAME_CHARS     "fd628_chars"
#define MOD_NAME_DOTS      "fd628_dot_bits"
#define MOD_NAME_TYPE      "fd628_display_type"
#define DEV_NAME           "fd628_dev"

#endif

struct fd628_display {
	u_int8 type;
	u_int8 reserved;
	u_int8 flags;
	u_int8 controller;
};

#ifdef MODULE

struct fd628_dtb_config {
	u_int8 dat_index[7];
	u_int8 led_dots[8];
	struct fd628_display display;
};

struct fd628_dev {
	int clk_pin;
	int dat_pin;
	int stb_pin;
	u_int16 wbuf[7];
	struct fd628_dtb_config dtb_active;
	struct fd628_dtb_config dtb_default;
	u_int8 mode;
	u_int8 brightness;
	struct semaphore sem;
	wait_queue_head_t kb_waitq;	/* read and write queues */
	struct timer_list timer;
	int key_respond_status;
	int Keyboard_diskstatus;
	u_int8 KeyPressCnt;
	u_int8  key_fg;
	u_int8  key_val;
	u_int8 status_led_mask;		/* Indicators mask */
};

struct fd628_platform_data {
	struct fd628_dev *dev;
};

#endif

enum {
	CONTROLLER_FD628,
	CONTROLLER_FD620,
	CONTROLLER_TM1618,
	CONTROLLER_MAX
};

enum {
	DISPLAY_TYPE_5D_7S_NORMAL,
	DISPLAY_TYPE_5D_7S_T95,		// T95K is different.
	DISPLAY_TYPE_5D_7S_X92,
	DISPLAY_TYPE_5D_7S_ABOX,
	DISPLAY_TYPE_FD620_REF,
	DISPLAY_TYPE_MAX,
};

#define DISPLAY_FLAG_TRANSPOSED		0x01
#define DISPLAY_FLAG_TRANSPOSED_INT	0x00010000

enum {
	LED_DOT1_ALARM,
	LED_DOT1_USB,
	LED_DOT1_PLAY,
	LED_DOT1_PAUSE,
	LED_DOT1_SEC,
	LED_DOT1_ETH,
	LED_DOT1_WIFI,
	LED_DOT1_MAX
};

enum {
	LED_DOT2_APPS,
	LED_DOT2_SETUP,
	LED_DOT2_USB,
	LED_DOT2_CARD,
	LED_DOT2_SEC,
	LED_DOT2_HDMI,
	LED_DOT2_CVBS,
	LED_DOT2_MAX
};

enum {
	LED_DOT3_UNUSED1,
	LED_DOT3_UNUSED2,
	LED_DOT3_POWER,
	LED_DOT3_LAN,
	LED_DOT3_SEC,
	LED_DOT3_WIFIHI,
	LED_DOT3_WIFILO,
	LED_DOT3_MAX
};

#define LED_DOT_SEC	LED_DOT1_SEC
#define LED_DOT_MAX	LED_DOT1_MAX

static const u_int8 ledDots[LED_DOT_MAX] = {
	0x01,
	0x02,
	0x04,
	0x08,
	0x10,
	0x20,
	0x40
};

#define KEYBOARD_SCAN_FRE	2
#define KEYBOARD_MARK		0x00

/* ******************************************API******************************************* */
/* *******************************User needs to modify part******************************** */
/* **************Key name and the corresponding code value definition********************** */
#define FD628_KEY10		0x00000200
#define FD628_KEY9		0x00000100
#define FD628_KEY8		0x00000080
#define FD628_KEY7		0x00000040
#define FD628_KEY6		0x00000020
#define FD628_KEY5		0x00000010
#define FD628_KEY4		0x00000008
#define FD628_KEY3		0x00000004
#define FD628_KEY2		0x00000002
#define FD628_KEY1		0x00000001
#define FD628_KEY_NONE_CODE	0x00

/* *************************************************************************************************************************************** *
*	Status Description	| bit7	| bit6	| bit5	| bit4	| bit3		| bit2	| bit1	| bit0	| Display_EN: Display enable bit, 1: Turn on display; 0: Turn off display
*				| 0	| 0	| 0	| 0	| Display_EN	|    brightness[3:0]    | Brightness: display brightness control bits, 000 ~ 111 represents brightness of 1 (min) ~ 8 (max)
* *************************************************************************************************************************************** */
#define FD628_DISP_ON	0x08					/* FD628 Display On */
#define FD628_DISP_OFF	0x00					/* FD628 Display Off */

typedef enum  _Brightness {					/* FD628 Brightness levels */
	FD628_Brightness_1 = 0x00,
	FD628_Brightness_2,
	FD628_Brightness_3,
	FD628_Brightness_4,
	FD628_Brightness_5,
	FD628_Brightness_6,
	FD628_Brightness_7,
	FD628_Brightness_8
}Brightness;

#define		FD628_DELAY_1us		udelay(4)		/* Delay time >1us */
/* ************************************** Do not change *********************************************** */
/* ********************** Define FD628 write delays (see datasheet for details) *********************** */
#define		FD628_DELAY_LOW		FD628_DELAY_1us		/* Clock Low Time >500ns*/
#define		FD628_DELAY_HIGH	FD628_DELAY_1us		/* Clock High Time >500ns*/
#define		FD628_DELAY_BUF		FD628_DELAY_1us		/* Interval between the end of the communication and the start of the next communication >1us*/
#define		FD628_DELAY_STB		FD628_DELAY_1us

/* ***************************** Define FD628 Commands **************************** */
#define FD628_KEY_RDCMD			0x42			/* Read keys command */
#define FD628_4DIG_CMD			0x00			/* Set FD628 to work in 4-digit mode */
#define FD628_5DIG_CMD			0x01			/* Set FD628 to work in 5-digit mode */
#define FD628_6DIG_CMD			0x02			/* Set FD628 to work in 6-digit mode */
#define FD628_7DIG_CMD			0x03			/* Set FD628 to work in 7-digit mode */
#define FD628_DIGADDR_WRCMD		0xC0			/* Write FD628 address */
#define FD628_ADDR_INC_DIGWR_CMD	0x40			/* Set Address Increment Mode */
#define FD628_ADDR_STATIC_DIGWR_CMD	0x44			/* Set Static Address Mode */
#define FD628_DISP_STATUE_WRCMD		0x80			/* Set display brightness command */
/* **************************************************************************************************************************** */

#endif
