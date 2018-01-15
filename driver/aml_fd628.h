/**
 * FD628地址范围为00~0D
 * 		地址		   显示字节
 * 		00	01		GRID1
 * 		02	03		GRID2
 * 		04	05		GRID3
 * 		06	07		GRID4
 * 		08	09		GRID5
 * 		0a	0b		GRID6
 * 		0c	0d		GRID7
 */

#ifndef __AML_FD628H__
#define __AML_FD628H__

#include <linux/delay.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/of.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/switch.h>
#include <linux/time.h>

typedef unsigned char   u_int8;
typedef unsigned short  u_int16;
typedef unsigned long 	u_int32;

#if 0
#define pr_dbg(args...) printk(KERN_ALERT "FD628: " args)
#else
#define pr_dbg(args...)
#endif

#define pr_error(args...) printk(KERN_ALERT "FD628: " args)

#ifndef CONFIG_OF
#define CONFIG_OF
#endif


#define  FD628_DRIVER_VERSION	"V1.0.0"


/*
 * Ioctl definitions
 */

/* Use 'K' as magic number */
#define FD628_IOC_MAGIC  'M'
#define FD628_IOC_SMODE _IOW(FD628_IOC_MAGIC,  1, int)
#define FD628_IOC_GMODE _IOR(FD628_IOC_MAGIC,  2, int)
#define FD628_IOC_SBRIGHT _IOW(FD628_IOC_MAGIC,  3, int)
#define FD628_IOC_GBRIGHT _IOR(FD628_IOC_MAGIC,  4, int)
#define FD628_IOC_POWER _IOW(FD628_IOC_MAGIC,  5, int)
#define FD628_IOC_GVER _IOR(FD628_IOC_MAGIC, 6, int)
#define FD628_IOC_STATUS_LED _IOW(FD628_IOC_MAGIC, 7, int)
#define FD628_IOC_MAXNR 8

#define MOD_NAME_CLK       "fd628"
#define MOD_NAME_DAT       "fd628_dat"
#define MOD_NAME_STB       "fd628_stb"
#define DEV_NAME           "fd628_dev"

struct fd628_dev{
	int clk_pin;
	int dat_pin;
	int stb_pin;
	char wbuf[14];
    __u8 mode;
	__u8 brightness;
	struct semaphore sem;
	wait_queue_head_t kb_waitq;       /* read and write queues */
	struct timer_list timer;
	int key_respond_status;
	int Keyboard_diskstatus;		/* 键盘状态:0-释放状态;1-检测到一个按键起始动作且已经消抖; */
	__u8 KeyPressCnt;
	__u8  key_fg;
	__u8  key_val;
	// 状态灯
	__u8 status_led_mask;
};

struct fd628_platform_data{
	struct fd628_dev *dev;
};

#ifdef  FD628_Drive_GLOBALS
#define FD628_Drive_EXT
#else
#define FD628_Drive_EXT  extern
#endif

#if 0
/* *************************************************************************************************************************************
 *            a
 *         -------
 *        |       |
 *      f |       | b
 *         ---g---
 *        |       |	c
 *      e |       |
 *         ---d---   dp
 * *************************************************************************************************************************************** *
 *码字		| 0		| 1		| 2		| 3		| 4		| 5		| 6		| 7		| 8		| 9		| A		| b		| C 	| d		| E		| F		|
 *编码		|0x3F	|0x06	|0x5B	|0x4F	|0x66	|0x6D	|0x7D	|0x07	|0x7F	|0x6F	|0x77	|0x7c	|0x39	|0x5E	|0x79	|0x71	|
 ************************************************************************************************************************************* */
#define		NEGA_LED_NONE 0X00
#define		NEGA_LED_0 0X3F
#define		NEGA_LED_1 0x40
#define		NEGA_LED_2 0x5b
#define		NEGA_LED_3 0x79
#define		NEGA_LED_4 0x74
#define		NEGA_LED_5 0X6b
#define		NEGA_LED_6 0x6f
#define		NEGA_LED_7 0x38
#define		NEGA_LED_8 0x7f
#define		NEGA_LED_9 0x7d

//#define		NEGA_LED_A 0X77
//#define		NEGA_LED_b 0x7c
//#define		NEGA_LED_C 0X39
//#define		NEGA_LED_c 0X58
//#define		NEGA_LED_d 0x5E
//#define		NEGA_LED_E 0X79
//#define		NEGA_LED_e 0X7b
//#define		NEGA_LED_F 0x71
//#define		NEGA_LED_I 0X60
//#define		NEGA_LED_L 0X38
//#define		NEGA_LED_r 0X72
//#define		NEGA_LED_n 0X54
//#define		NEGA_LED_N 0X37
//#define		NEGA_LED_O 0X3F
//#define		NEGA_LED_P 0XF3
//#define		NEGA_LED_S 0X6d
//#define		NEGA_LED_y 0X6e
//#define		NEGA_LED__ 0x08
#endif

#define KEYBOARD_SCAN_FRE      2
#define KEYBOARD_MARK         0x00

/** Character conversion of digital tube display code*/
typedef struct _led_bitmap
{
	u_int8 character;
	u_int8 bitmap;
} led_bitmap;

#if 0
/** Character conversion of digital tube display code array*/
#define LEDMAPNUM   (sizeof(LED_decode_tab)/sizeof(LED_decode_tab[0]))		///<Digital tube code array maximum
static const led_bitmap LED_decode_tab[] =
{
#if 0
#if 1
	{'0', 0x3F}, {'1', 0x06}, {'2', 0x5B}, {'3', 0x4F},
	{'4', 0x66}, {'5', 0x6D}, {'6', 0x7D}, {'7', 0x07},
	{'8', 0x7F}, {'9', 0x6F}, {'a', 0x77}, {'A', 0x77},
	{'b', 0x7C}, {'B', 0x7C}, {'c', 0x58}, {'C', 0x39},
	{'d', 0x5E}, {'D', 0x5E}, {'e', 0x79}, {'E', 0x79},
	{'f', 0x71}, {'F', 0x71}, {'I', 0X60}, {'i', 0x60},
	{'L', 0x38}, {'l', 0x38}, {'r', 0x38}, {'R', 0x38},
	{'n', 0x54}, {'N', 0x37}, {'O', 0x3F}, {'o', 0x3f},
	{'p', 0xf3}, {'P', 0x38}, {'S', 0x6D}, {'s', 0x6d},
	{'y', 0x6e}, {'Y', 0x6E}, {'_', 0x08}, {0,   0x3F},
	{1,   0x06}, {2,   0x5B}, {3,   0x4F}, {4,   0x66},
	{5,   0x6D}, {6,   0x7D}, {7,   0x07}, {8,   0x7F},
	{9,   0x6F}, {'!', 0X01}, {'@', 0X02}, {'#', 0X04},
	{'$', 0X08}, {':', 0X10}, {'^', 0X20}, {'&', 0X40},
	{0xC5,0X00}, {0x3b,0x18}, {0xc4,0x08}, {0x3c,0x14},
	{0xc3,0x04}, {0x3d,0x1c}, {0xc2,0x0c},
#else
    {'0', 0x3F}, {'1', 0x06}, {'2', 0x5B}, {'3', 0x4F},
	{'4', 0x66}, {'5', 0x6D}, {'6', 0x7D}, {'7', 0x07},
	{'8', 0x7F}, {'9', 0x6F}, {'a', 0x77}, {'A', 0x77},
	{'b', 0x7C}, {'B', 0x7C}, {'c', 0x58}, {'C', 0x39},
	{'d', 0x5E}, {'D', 0x5E}, {'e', 0x79}, {'E', 0x79},
	{'f', 0x71}, {'F', 0x71}

#endif
#else

/*
 * amlogic905的LED包含5个位，1-4位为第1-4个数码管，第5位其他的灯（5A-5G）
 * amlogic905的LED七段管顺序如下图
 *
 *  dp
 *	  +---d---+
 *    |       |
 *  c |       | e
 *    +---g---+
 *    |       |
 *  b |       | f
 *    +---a---+
 *
 * 暂时仅颠倒了0-9, '0'-'9'字符
 *	6 5 4  3 2 1 0
 *	g f e  d c b a
 *	0 1 1  1 1 1 1 	0 => 0x3f
 *	0 1 1  0 0 0 0 	1 => 0x30
 *	1 0 1  1 0 1 1 	2 => 0x5b
 *	1 1 1  1 0 0 1  3 => 0x79
 *	1 1 1  0 1 0 0  4 => 0x74
 *	1 1 0  1 1 0 1  5 => 0x6d
 *	1 1 0  1 1 1 1  6 => 0x6f
 *	0 1 1  1 0 0 0  7 => 0x38
 *	1 1 1  1 1 1 1  8 => 0x7F
 *	1 1 1  1 1 0 1  9 => 0x7d
 *
 * 'boot'
 *  1 1 0  0 1 1 1  b => 0x67
 *  1 1 0  0 0 1 1  o => 0x63
 *  1 0 0  0 1 1 1  t => 0x47
 *
 */
	{'0', 0x3F}, {'1', 0x30}, {'2', 0x5B}, {'3', 0x79},
	{'4', 0x74}, {'5', 0x6d}, {'6', 0x6f}, {'7', 0x38},
	{'8', 0x7F}, {'9', 0x7d},

    {'a', 0x77}, {'A', 0x77},
	{'b', 0x7C}, {'B', 0x7C}, {'c', 0x58}, {'C', 0x39},
	{'d', 0x5E}, {'D', 0x5E}, {'e', 0x79}, {'E', 0x79},
	{'f', 0x71}, {'F', 0x71}, {'I', 0X60}, {'i', 0x60},
	{'L', 0x38}, {'l', 0x38}, {'r', 0x38}, {'R', 0x38},
	{'n', 0x54}, {'N', 0x37}, {'O', 0x3F}, {'o', 0x3f},
	{'p', 0xf3}, {'P', 0x38}, {'S', 0x6D}, {'s', 0x6d},
	{'y', 0x6e}, {'Y', 0x6E}, {'_', 0x08},

    {0,   0x3F},
	{1,   0x30}, {2,   0x5B}, {3,   0x79}, {4,   0x74},
	{5,   0x6d}, {6,   0x6f}, {7,   0x38}, {8,   0x7F},
	{9,   0x7d},

    {'!', 0X01}, {'@', 0X02}, {'#', 0X04},
	{'$', 0X08}, {':', 0X10}, {'^', 0X20}, {'&', 0X40},

    /* ? what's this ? */
    {0xC5,0X00}, {0x3b,0x18}, {0xc4,0x08}, {0x3c,0x14},
	{0xc3,0x04}, {0x3d,0x1c}, {0xc2,0x0c}
#endif
};//BCD码字映射
#endif

/* **************************************API*********************************************** */
/* *************************用户需要修改部分************************** */
//typedef bit            int;       /* 布尔变量布尔变量*/
//typedef unsigned char  u_int8;         /* 无符号8位数*/
/* **************按键名和对应码值定义********************** */
#define FD628_KEY10 	0x00000200
#define FD628_KEY9 		0x00000100
#define FD628_KEY8 		0x00000080
#define FD628_KEY7 		0x00000040
#define FD628_KEY6  	0x00000020
#define FD628_KEY5 		0x00000010
#define FD628_KEY4  	0x00000008
#define FD628_KEY3  	0x00000004
#define FD628_KEY2  	0x00000002
#define FD628_KEY1  	0x00000001
#define FD628_KEY_NONE_CODE 0x00

/* *************************************************************************************************************************************** *
*	Status说明	| bit7	| bit6	| bit5	| bit4	| bit3			| bit2	| bit1	| bit0	| 		 Display_EN：显示使能位，1：打开显示；0：关闭显示
*				| 0		| 0		| 0		| 0		| Display_EN	|	brightness[3:0]		|		 brightness：显示亮度控制位，000～111 分别代表着1（min）～8（max）级亮度
* ************************************************************************************************************************************* */
/* ************** Status可以使用下面的宏 （之间用或的关系） ************ */
#define FD628_DISP_ON        					0x08		/*打开FD628显示*/
#define FD628_DISP_OFF        				0x00		/*关闭FD628显示*/

//#define FD628_Brightness_1        				0x00		/*设置FD628显示亮度等级为1*/
//#define FD628_Brightness_2        				0x01		/*设置FD628显示亮度等级为2*/
//#define FD628_Brightness_3        				0x02		/*设置FD628显示亮度等级为3*/
//#define FD628_Brightness_4        				0x03		/*设置FD628显示亮度等级为4*/
//#define FD628_Brightness_5        				0x04		/*设置FD628显示亮度等级为5*/
//#define FD628_Brightness_6        				0x05		/*设置FD628显示亮度等级为6*/
//#define FD628_Brightness_7        				0x06		/*设置FD628显示亮度等级为7*/
//#define FD628_Brightness_8        				0x07		/*设置FD628显示亮度等级为8*/

typedef enum  _Brightness{
	FD628_Brightness_1 = 0x00,
	FD628_Brightness_2,
	FD628_Brightness_3,
	FD628_Brightness_4,
	FD628_Brightness_5,
	FD628_Brightness_6,
	FD628_Brightness_7,
	FD628_Brightness_8
}Brightness;

#define 	FD628_DELAY_1us						udelay(4)					    	/* 延时时间 >1us*/
/* **************************************用户不需要修改*********************************************** */
/* **************写入FD628延时部分（具体含义看Datasheet）********************** */
#define 	FD628_DELAY_LOW		     	FD628_DELAY_1us                     		        /* 时钟低电平时间 >500ns*/
#define		FD628_DELAY_HIGH     	 	FD628_DELAY_1us 	   										 				/* 时钟高电平时间 >500ns*/
#define  	FD628_DELAY_BUF		 		 	FD628_DELAY_1us	                     				  	/* 通信结束到下一次通信开始的间隔 >1us*/
#define  	FD628_DELAY_STB					FD628_DELAY_1us



/* ***********************写入FD628操作命令***************************** */
#define FD628_KEY_RDCMD        					0x42                //按键读取命令
#define FD628_4DIG_CMD        				0x00		/*设置FD628工作在4位模式的命令*/
#define FD628_5DIG_CMD        				0x01		/*设置FD628工作在5位模式的命令*/
#define FD628_6DIG_CMD         				0x02	 	/*设置FD628工作在6位模式的命令*/
#define FD628_7DIG_CMD         				0x03	 	/*设置FD628工作在7位模式的命令*/
#define FD628_DIGADDR_WRCMD  				0xC0								//显示地址写入命令
#define FD628_ADDR_INC_DIGWR_CMD       	0x40								//地址递增方式显示数据写入
#define FD628_ADDR_STATIC_DIGWR_CMD    	0x44								//地址不递增方式显示数据写入
#define FD628_DISP_STATUE_WRCMD        	0x80								//显示亮度写入命令
/* **************************************************************************************************************************** */

enum {
	LED_DOT_ALARM,
	LED_DOT_USB,
	LED_DOT_PLAY,
	LED_DOT_PAUSE,
	LED_DOT_SEC,
	LED_DOT_ETH,
	LED_DOT_WIFI,
	LED_DOT_MAX
};

static const __u8 ledDots[LED_DOT_MAX] = {
	0x01,
	0x02,
	0x04,
	0x08,
	0x10,
	0x20,
	0x40
};



#endif
