/**
 * FD628地址范围为00~0D
 *	地址		显示字节
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


#define  FD628_DRIVER_VERSION	"V1.0.0"


/*
 * Ioctl definitions
 */

/* Use 'M' as magic number */
#define FD628_IOC_MAGIC  'M'
#define FD628_IOC_SMODE _IOW(FD628_IOC_MAGIC,  1, int)
#define FD628_IOC_GMODE _IOR(FD628_IOC_MAGIC,  2, int)
#define FD628_IOC_SBRIGHT _IOW(FD628_IOC_MAGIC,  3, int)
#define FD628_IOC_GBRIGHT _IOR(FD628_IOC_MAGIC,  4, int)
#define FD628_IOC_POWER _IOW(FD628_IOC_MAGIC,  5, int)
#define FD628_IOC_GVER _IOR(FD628_IOC_MAGIC, 6, int)
#define FD628_IOC_STATUS_LED _IOW(FD628_IOC_MAGIC, 7, int)
#define FD628_IOC_GDISPLAY_TYPE _IOR(FD628_IOC_MAGIC, 8, int)
#define FD628_IOC_SDISPLAY_TYPE _IOW(FD628_IOC_MAGIC, 9, int)
#define FD628_IOC_MAXNR 10

#ifdef MODULE

#define MOD_NAME_CLK       "fd628"
#define MOD_NAME_DAT       "fd628_dat"
#define MOD_NAME_STB       "fd628_stb"
#define DEV_NAME           "fd628_dev"

struct fd628_dev {
	int clk_pin;
	int dat_pin;
	int stb_pin;
	u_int16 wbuf[7];
	u_int8 dat_index[7];
	u_int8 led_dots[8];
	int display_type;
	u_int8 mode;
	u_int8 brightness;
	struct semaphore sem;
	wait_queue_head_t kb_waitq;	/* read and write queues */
	struct timer_list timer;
	int key_respond_status;
	int Keyboard_diskstatus;	/* 键盘状态:0-释放状态;1-检测到一个按键起始动作且已经消抖; */
	u_int8 KeyPressCnt;
	u_int8  key_fg;
	u_int8  key_val;
	// 状态灯
	u_int8 status_led_mask;
};

struct fd628_platform_data {
	struct fd628_dev *dev;
};

#endif

#ifdef  FD628_Drive_GLOBALS
#define FD628_Drive_EXT
#else
#define FD628_Drive_EXT extern
#endif

#define KEYBOARD_SCAN_FRE	2
#define KEYBOARD_MARK		0x00

/* **************************************API*********************************************** */
/* *************************用户需要修改部分************************** */
//typedef bit            int;		/* 布尔变量布尔变量*/
//typedef unsigned char  u_int8;	/* 无符号8位数*/
/* **************按键名和对应码值定义********************** */
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
*	Status说明	| bit7	| bit6	| bit5	| bit4	| bit3		| bit2	| bit1	| bit0	| Display_EN：显示使能位，1：打开显示；0：关闭显示
*			| 0	| 0	| 0	| 0	| Display_EN	|    brightness[3:0]    | brightness：显示亮度控制位，000～111 分别代表着1（min）～8（max）级亮度
* *************************************************************************************************************************************** */
/* ************************** Status可以使用下面的宏 （之间用或的关系） ************************** */
#define FD628_DISP_ON	0x08					/*打开FD628显示*/
#define FD628_DISP_OFF	0x00					/*关闭FD628显示*/

typedef enum  _Brightness {
	FD628_Brightness_1 = 0x00,				/*设置FD628显示亮度等级为1*/
	FD628_Brightness_2,					/*设置FD628显示亮度等级为2*/
	FD628_Brightness_3,					/*设置FD628显示亮度等级为3*/
	FD628_Brightness_4,					/*设置FD628显示亮度等级为4*/
	FD628_Brightness_5,					/*设置FD628显示亮度等级为5*/
	FD628_Brightness_6,					/*设置FD628显示亮度等级为6*/
	FD628_Brightness_7,					/*设置FD628显示亮度等级为7*/
	FD628_Brightness_8					/*设置FD628显示亮度等级为8*/
}Brightness;

#define		FD628_DELAY_1us		udelay(4)		/* 延时时间 >1us*/
/* ************************************** 用户不需要修改 *********************************************** */
/* ********************** 写入FD628延时部分（具体含义看Datasheet） ********************** */
#define		FD628_DELAY_LOW		FD628_DELAY_1us		/* 时钟低电平时间 >500ns*/
#define		FD628_DELAY_HIGH	FD628_DELAY_1us		/* 时钟高电平时间 >500ns*/
#define		FD628_DELAY_BUF		FD628_DELAY_1us		/* 通信结束到下一次通信开始的间隔 >1us*/
#define		FD628_DELAY_STB		FD628_DELAY_1us



/* ***************************** 写入FD628操作命令 ***************************** */
#define FD628_KEY_RDCMD			0x42			//按键读取命令
#define FD628_4DIG_CMD			0x00			/*设置FD628工作在4位模式的命令*/
#define FD628_5DIG_CMD			0x01			/*设置FD628工作在5位模式的命令*/
#define FD628_6DIG_CMD			0x02			/*设置FD628工作在6位模式的命令*/
#define FD628_7DIG_CMD			0x03			/*设置FD628工作在7位模式的命令*/
#define FD628_DIGADDR_WRCMD		0xC0			//显示地址写入命令
#define FD628_ADDR_INC_DIGWR_CMD	0x40			//地址递增方式显示数据写入
#define FD628_ADDR_STATIC_DIGWR_CMD	0x44			//地址不递增方式显示数据写入
#define FD628_DISP_STATUE_WRCMD		0x80			//显示亮度写入命令
/* **************************************************************************************************************************** */

#endif
