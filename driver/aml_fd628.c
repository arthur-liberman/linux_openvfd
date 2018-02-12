/*
 * drivers/amlogic/fdchip/fd628/aml_fd628.c
 *
 * FD628 Driver
 *
 * Copyright (C) 2015 Fdhisi, Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/leds.h>
#include <linux/string.h>

#include <linux/ioctl.h>
#include <linux/device.h>

#include <linux/errno.h>
#include <linux/mutex.h>

#include <linux/miscdevice.h>
#include <linux/fs.h>

#include <linux/fcntl.h>
#include <linux/poll.h>

#include <linux/sched.h>

#include <linux/gpio.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/iomap.h>
//#include <linux/input/vfd.h>

#include "aml_fd628.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend fd628_early_suspend;
#endif


static struct fd628_platform_data *pdata = NULL;
struct kp {
	struct led_classdev cdev;
};

static struct kp *kp;

/****************************************************************
 *	函数的名称:			FD628_Start
 *	描述:				FD628通信的起始准备
 *	参数：				void
 *	返回值:				void
****************************************************************/
static void FD628_Start(struct fd628_dev *dev)
{
	//FD628_STB_CLR;                                  /* 设置STB为低电平 */
	gpio_direction_output(dev->stb_pin, 0);
	//FD628_STB_D_OUT;                                /* 设置STB为输出方向 */
	//FD628_CLK_D_OUT;                                /* 设置CLK为输出方向 */
	FD628_DELAY_STB;
}

/****************************************************************
 *	函数的名称:			FD628_Stop
 *	描述:				FD628通信的结束准备
 *	参数：				void
 *	返回值:				void
****************************************************************/
static void FD628_Stop(struct fd628_dev *dev)
{
	//FD628_CLK_SET;				/* 设置CLK为高电平 */
	gpio_direction_output(dev->clk_pin, 1);
	FD628_DELAY_STB;
	//FD628_STB_SET;				/* 设置STB为高电平 */
	gpio_direction_output(dev->stb_pin, 1);
	//FD628_DIO_SET;				/* 设置DIO为高电平 */
	gpio_direction_output(dev->dat_pin, 1);
	//FD628_DIO_D_IN;				/* 设置DIO为输入方向 */
	gpio_direction_input(dev->dat_pin);
	FD628_DELAY_BUF;				/* 通信结束到下一次通信开始的间隔 */
}

/****************************************************************
 *	函数的名称:			FD628_WrByte
 *	描述:				向FD628写入一个字节的数据
 *	参数：				INT8U  发送的数据
 *	返回值:				void
 *	注意:				数据从低位到高位传输
****************************************************************/
static void FD628_WrByte(u_int8 dat, struct fd628_dev *dev)
{
	u_int8 i;						/* 移位控制变量 */
	//FD628_DIO_D_OUT;                                	/* 设置DIO为输出方向 */
	for (i = 0; i != 8; i++) {				/* 输出8 bit的数据 */
		//FD628_CLK_CLR;				/* 设置CLK为低电平 */
		gpio_direction_output(dev->clk_pin, 0);
		if (dat & 0x01) {				/* 数据从低位到高位输出 */
			//FD628_DIO_SET;			/* 设置DIO为高电平 */
			gpio_direction_output(dev->dat_pin, 1);

		} else {
			//FD628_DIO_CLR;			/* 设置DIO为低电平 */
			gpio_direction_output(dev->dat_pin, 0);
		}
		FD628_DELAY_LOW;				/* 时钟低电平时间 */
		//FD628_CLK_SET;
		//设置SCL为高电平 上升沿写入
		gpio_direction_output(dev->clk_pin, 1);
		dat >>= 1;					/* 输出数据右移一位，数据从低到高的输出 */
		FD628_DELAY_HIGH;				/* 时钟高电平时间 */
	}
}

/****************************************************************
 *	函数的名称:			FD628_RdByte
 *	描述:				从FD628读一个字节的数据
 *	参数：				void
 *	返回值:				INT8U 读到的数据
 *	注意:				数据从低位到高位传输
****************************************************************/
static u_int8 FD628_RdByte(struct fd628_dev *dev)
{
	u_int8 i, dat = 0;					/* 移位控制变量i;读取数据暂存变量dat */
	//FD628_DIO_SET;					/* 设置DIO为高电平 */
	gpio_direction_output(dev->dat_pin, 1);
	//FD628_DIO_D_IN;					/* 设置DIO为输出方向 */
	gpio_direction_input(dev->dat_pin);
	for (i = 0; i != 8; i++) {				/* 输出8 bit的数据 */
		//FD628_CLK_CLR;				/* 设置CLK为低电平 */
		gpio_direction_output(dev->clk_pin, 0);
		FD628_DELAY_LOW;				/* 时钟低电平时间 */
		dat >>= 1;					/* 读入数据右移一位，数据从低到高的读入 */
		//if( FD628_DIO_IN ) dat|=0X80;			/* 读入1 bit值 */
		if (gpio_get_value(dev->dat_pin))
			dat |= 0X80;				/* 读入1 bit值 */
		//FD628_CLK_SET;				/* 设置CLK为高电平 */
		gpio_direction_output(dev->clk_pin, 1);
		FD628_DELAY_HIGH;				/* 时钟高电平时间 */
	}
	return dat;						/* 返回接收到的数据 */
}

/****************************************FD628操作函数*********************************************/
/****************************************************************
 *	函数的名称:				FD628_Command
 *	描述:					发送控制命令
 *	参数:					INT8U 控制命令
 *	返回值:					void
****************************************************************/
static void FD628_Command(u_int8 cmd, struct fd628_dev *dev)
{
	FD628_Start(dev);
	FD628_WrByte(cmd, dev);
	FD628_Stop(dev);
}

/****************************************************************
 *	函数的名称:				FD628_GetKey
 *	描述:					读按键码值
 *	参数:					void
 *	返回值:					INT32U 返回按键值
 **************************************************************************************************************************************
返回的按键值编码
				| 0			| 0			| 0			| 0			| 0			| 0			| KS10	| KS9		| KS8		| KS7		| KS6		| KS5		| KS4		| KS3		| KS2		| KS1		|
KEYI1 	| bit15	| bit14	| bit13	| bit12	| bit11	| bit10	| bit9	| bit8	| bit7	| bit6	| bit5	| bit4	| bit3	| bit2	| bit1	| bit0	|
KEYI2 	| bit31	| bit30	| bit29	| bit28	| bit27	| bit26	| bit25	| bit24	| bit23	| bit22	| bit21	| bit20	| bit19	| bit18	| bit17	| bit16	|
***************************************************************************************************************************************/
static u_int32 FD628_GetKey(struct fd628_dev *dev)
{
	u_int8 i, KeyDataTemp;
	u_int32 FD628_KeyData = 0;
	FD628_Start(dev);
	FD628_WrByte(FD628_KEY_RDCMD, dev);
	for (i = 0; i != 5; i++) {
		KeyDataTemp = FD628_RdByte(dev);		/*将5字节的按键码值转化成2字节的码值 */
		if (KeyDataTemp & 0x01)
			FD628_KeyData |= (0x00000001 << i * 2);
		if (KeyDataTemp & 0x02)
			FD628_KeyData |= (0x00010000 << i * 2);
		if (KeyDataTemp & 0x08)
			FD628_KeyData |= (0x00000002 << i * 2);
		if (KeyDataTemp & 0x10)
			FD628_KeyData |= (0x00020000 << i * 2);
	}
	FD628_Stop(dev);
	return (FD628_KeyData);
}

/****************************************************************
 *	函数的名称:				FD628_WrDisp_AddrINC
 *	描述:					以地址递增模式发送显示内容
 *	参数:					INT8U Addr发送显示内容的起始地址；具体地址和显示对应的表格见datasheet
 *						INT8U DataLen 发送显示内容的位数
 *	返回值:					BOOLEAN；如果地址超出将返回1；如果执行成功返回0。
 *	使用方法：				先将数据写入FD628_DispData[]的相应位置，再调用FD628_WrDisp_AddrINC（）函数。
****************************************************************/
static int FD628_WrDisp_AddrINC(u_int8 Addr, u_int8 DataLen,
				struct fd628_dev *dev)
{
	u_int8 i;
	u_int8 val;
	u_int8 *buf;

	val = FD628_DIGADDR_WRCMD;
	val |= Addr;
	buf = (u_int8*)dev->wbuf;

	if (DataLen + Addr > 14)
		return (1);

	FD628_Command(FD628_ADDR_INC_DIGWR_CMD, dev);
	FD628_Start(dev);
	FD628_WrByte(val, dev);
	for (i = Addr; i != (Addr + DataLen); i++) {
		FD628_WrByte(buf[i], dev);
		pr_dbg("FD628_WrDisp_AddrINC buf: %x \r\n", buf[i]);
	}
	FD628_Stop(dev);
	return (0);
}

/****************************************************************
 *	函数的名称:	FD628_SET_DISPLAY_MODE
 *	描述:		FD628设置显示模式
 *	参数:		cmd
				FD628_4DIG_CMD		0x00
				FD628_5DIG_CMD		0x01
				FD628_6DIG_CMD		0x02
				FD628_7DIG_CMD		0x03
 *	返回值:		void
****************************************************************/
static void FD628_SET_DISPLAY_MODE(u_int8 cmd, struct fd628_dev *dev)
{
	FD628_Command(cmd, dev);
}

/****************************************************************
 *	函数的名称: 		FD628_SET_BRIGHTNESS
 *	描述:			FD628设置显示亮度
 *	参数:
cmd:
	FD628_Brightness_1	0x00
	FD628_Brightness_2	0x01
	FD628_Brightness_3	0x02
	FD628_Brightness_4	0x03
	FD628_Brightness_5	0x04
	FD628_Brightness_6	0x05
	FD628_Brightness_7	0x06
	FD628_Brightness_8	0x07	显示亮度等级为8
status:
	FD628_DISP_ON		打开显示
	FD628_DISP_OFF		关闭显示
 *	返回值:			void
****************************************************************/
static void FD628_SET_BRIGHTNESS(u_int8 cmd, struct fd628_dev *dev,
				 u_int8 status)
{
	cmd |= status;
	cmd &= 0x0f;
	cmd |= FD628_DISP_STATUE_WRCMD;
	FD628_Command(cmd, dev);
}

/****************************************************************
 *	函数的名称:		FD628_Init
 *	描述:			FD628初始化，用户可以根据需要修改显示
 *	参数:			void
 *	返回值:			void
****************************************************************/
static void FD628_Init(struct fd628_dev *dev)
{
	/* 设置CLK为高电平 */
	gpio_direction_output(dev->clk_pin, 1);
	/* 设置STB为高电平 */
	gpio_direction_output(dev->stb_pin, 1);
	/* 设置DIO为高电平 */
	gpio_direction_output(dev->dat_pin, 1);
	/* 设置STB为输出方向 */
	/* 设置CLK为输出方向 */
	/* 设置DIO为输入方向 */
	gpio_direction_input(dev->dat_pin);
	/* 通信结束到下一次通信开始的间隔 */
	FD628_DELAY_BUF;
}

static int fd628_dev_open(struct inode *inode, struct file *file)
{
	struct fd628_dev *dev = NULL;
	file->private_data = pdata->dev;
	dev = file->private_data;
	memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
	FD628_SET_BRIGHTNESS(pdata->dev->brightness, dev, FD628_DISP_ON);
	pr_dbg("fd628_dev_open now.............................\r\n");
	return 0;
}

static int fd628_dev_release(struct inode *inode, struct file *file)
{
	struct fd628_dev *dev = file->private_data;
	//del_timer(&dev->timer);
	FD628_SET_BRIGHTNESS(pdata->dev->brightness, dev, FD628_DISP_OFF);
	file->private_data = NULL;
	pr_dbg("succes to close  fd628_dev.............\n");
	return 0;
}

static ssize_t fd628_dev_read(struct file *filp, char __user * buf,
				  size_t count, loff_t * f_pos)
{
	__u32 disk = 0;
	struct fd628_dev *dev = filp->private_data;
	__u32 diskvalue = 0;
	int ret = 0;
	int rbuf[2] = { 0 };
	//pr_dbg("start read keyboard value...............\r\n");
	if (dev->Keyboard_diskstatus == 1) {
		diskvalue = FD628_GetKey(dev);
		if (diskvalue == 0)
			return 0;
	}
	dev->key_respond_status = 0;
	rbuf[1] = dev->key_fg;
	if (dev->key_fg)
		rbuf[0] = disk;
	else
		rbuf[0] = diskvalue;
	//pr_dbg("Keyboard value:%d\n, status : %d\n",rbuf[0],rbuf[1]);
	ret = copy_to_user(buf, rbuf, sizeof(rbuf));
	if (ret == 0)
		return sizeof(rbuf);
	else
		return ret;
}

// Source for the transpose algorithm:
// http://www.hackersdelight.org/hdcodetxt/transpose8.c.txt
static void transpose8rS64(unsigned char* A, unsigned char* B) {
	unsigned long long x = 0, t;
	int i;

	for (i = 0; i <= 7; i++)	// Load 8 bytes from the input
		x = x << 8 | A[i];	// array and pack them into x.

	t = (x ^ (x >> 7)) & 0x00AA00AA00AA00AALL;
	x = x ^ t ^ (t << 7);
	t = (x ^ (x >> 14)) & 0x0000CCCC0000CCCCLL;
	x = x ^ t ^ (t << 14);
	t = (x ^ (x >> 28)) & 0x00000000F0F0F0F0LL;
	x = x ^ t ^ (t << 28);

	memcpy(B, &x, sizeof(x));	// Store result into output array B.
}

/**
 * @param buf:传入LED码
 * 		  [3-0]代表右往左LED1234灯显示的7段管掩码
 * 		  [4]  代表wifi灯、eth灯、usb灯、闹钟灯、play、pause灯等6个灯的掩码
 * @return
 */
static ssize_t fd628_dev_write(struct file *filp, const char __user * buf,
				   size_t count, loff_t * f_pos)
{
	struct fd628_dev *dev;
	unsigned long missing;
	int status = 0;
	int i = 0;
	const int dataMaxLen = 7;
	unsigned short data[7];
	unsigned char trans[8];

	dev = filp->private_data;

	count /= sizeof(data[0]);
	if (count > dataMaxLen)
		count = dataMaxLen;

	memset(data, 0, sizeof(data));
	missing = copy_from_user(data, buf, count*sizeof(data[0]));
	if (missing == 0) {
		// Apply dot remap for column.
		if (data[0] & ledDots[LED_DOT_SEC]) {
			data[0] &= ~ledDots[LED_DOT_SEC];
			data[0] |= dev->led_dots[LED_DOT_SEC];
		}
		// Apply LED indicators mask (usb, eth, wifi etc.)
		data[0] |= dev->status_led_mask;

		switch (dev->display_type) {
			case DISPLAY_UNKNOWN:
			case DISPLAY_COMMON_CATHODE_FD628:
			default:
				for (i = 0; i < count; i++)
					dev->wbuf[dev->dat_index[i]] = data[i];
				break;
			case DISPLAY_COMMON_ANODE_FD628:
				// Common Anode displays require us
				// to transpose the memory in order for it
				// to display data correctly on the screen.
				count = dataMaxLen;
				memset(trans, 0, sizeof(trans));
				memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
				for (i = 0; i < count; i++)
					trans[dev->dat_index[i]] = (unsigned char)data[i] << 1;
				transpose8rS64(trans, trans);
				for (i = 0; i < dataMaxLen; i++) {
					dev->wbuf[i] = trans[i+1];
				}
				break;
			case DISPLAY_COMMON_CATHODE_5D_FD620:
				// Memory map:
				// S1 S2 S3 S4 S5 S6 S7 xx xx xx xx xx xx S8 xx xx
				// b0 b1 b2 b3 b4 b5 b6 b7 b0 b1 b2 b3 b4 b5 b6 b7
				if (count > 5)	// This controller can hold 5 words.
					count = 5;
				for (i = 0; i < count; i++)
					dev->wbuf[dev->dat_index[i]] = data[i];
				break;
			case DISPLAY_COMMON_CATHODE_4D_FD620:
				if (count > 5)
					count = 5;
				// Apply column LED state to 8th bit of each digit, as 'dp'.
				data[0] = (data[0] & dev->led_dots[LED_DOT_SEC] ? 0x2000 : 0);
				for (i = 1; i < count; i++)
					dev->wbuf[dev->dat_index[i]] = data[i] | data[0];
				break;
			case DISPLAY_COMMON_CATHODE_5D_TM1618:
				// Memory map:
				// S1 S2 S3 S4 S5 xx xx xx xx xx xx S12 S13 S14 xx xx
				// b0 b1 b2 b3 b4 b5 b6 b7 b0 b1 b2 b3  b4  b5  b6 b7
				for (i = 0; i < count; i++)
					dev->wbuf[dev->dat_index[i]] = data[i] | ((data[i] & 0xE0) << 6);
				break;
			case DISPLAY_COMMON_CATHODE_4D_TM1618:
				data[0] = (data[0] & dev->led_dots[LED_DOT_SEC] ? 0x2000 : 0);
				for (i = 1; i < count; i++)
					dev->wbuf[dev->dat_index[i]] = data[i] | ((data[i] & 0x60) << 6) | data[0];
				break;
		}

		FD628_WrDisp_AddrINC(0x00, 2 * count, dev);
		status = count;
	}
	pr_dbg("fd628_dev_write count : %d\n", count);
	return status;
}

static void applyDisplayType(struct fd628_dev *dev)
{
	switch (dev->display_type) {
		case DISPLAY_COMMON_CATHODE_FD628:
		case DISPLAY_COMMON_ANODE_FD628:
		default:
			FD628_SET_DISPLAY_MODE(FD628_7DIG_CMD, dev);
			break;
		case DISPLAY_COMMON_CATHODE_5D_FD620:
		case DISPLAY_COMMON_CATHODE_5D_TM1618:
			FD628_SET_DISPLAY_MODE(FD628_5DIG_CMD, dev);
			break;
		case DISPLAY_COMMON_CATHODE_4D_FD620:
		case DISPLAY_COMMON_CATHODE_4D_TM1618:
			FD628_SET_DISPLAY_MODE(FD628_4DIG_CMD, dev);
			break;
	}
	FD628_SET_BRIGHTNESS(dev->brightness, dev, FD628_DISP_ON);
	memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
}

static long fd628_dev_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	int err = 0, ret = 0, temp = 0;
	struct fd628_dev *dev;
	__u8 val = 1, icmd = FD628_Brightness_8;
	__u8 temp_chars_order[sizeof(dev->dat_index)];
	dev = filp->private_data;

	if (_IOC_TYPE(cmd) != FD628_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > FD628_IOC_MAXNR)
		return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch (cmd) {
	case FD628_IOC_GDISPLAY_TYPE:
		ret = __put_user(dev->display_type, (int __user *)arg);
		break;
	case FD628_IOC_SDISPLAY_TYPE:
		ret = __get_user(temp, (int __user *)arg);
		if (!ret) {
			if (temp >= 0 && temp < DISPLAY_TYPE_MAX) {
				dev->display_type = temp;
				applyDisplayType(dev);
			} else {
				ret = ERANGE;
			}
		}
		break;
	case FD628_IOC_SCHARS_ORDER:
		ret = __copy_from_user(temp_chars_order, (__u8 __user *)arg, sizeof(dev->dat_index));
		if (!ret)
			memcpy(dev->dat_index, temp_chars_order, sizeof(dev->dat_index));
		break;
	case FD628_IOC_SMODE:	/* Set: arg points to the value */
		ret = __get_user(dev->mode, (int __user *)arg);
		FD628_SET_DISPLAY_MODE(dev->mode, dev);
		break;
	case FD628_IOC_GMODE:	/* Get: arg is pointer to result */
		ret = __put_user(dev->mode, (int __user *)arg);
		break;
	case FD628_IOC_GVER:
		ret =
			copy_to_user((unsigned char __user *)arg,
				 FD628_DRIVER_VERSION,
				 sizeof(FD628_DRIVER_VERSION));
		break;
	case FD628_IOC_SBRIGHT:
		ret = __get_user(temp, (int __user *)arg);
		if (!ret) {
			if (temp >= FD628_Brightness_1 && temp <= FD628_Brightness_8) {
				dev->brightness = temp;
				FD628_SET_BRIGHTNESS(dev->brightness, dev, FD628_DISP_ON);
			} else {
				ret = ERANGE;
			}
		}
		break;
	case FD628_IOC_GBRIGHT:
		ret = __put_user(dev->brightness, (int __user *)arg);
		break;
	case FD628_IOC_POWER:
		ret = __get_user(val, (int __user *)arg);
		if (val == 1) {
			icmd = FD628_DISP_ON;
			icmd &= 0x0f;
			icmd |= FD628_DISP_STATUE_WRCMD;
			FD628_Command(icmd, dev);
		} else {
			icmd = FD628_DISP_OFF;
			icmd &= 0x0f;
			icmd |= FD628_DISP_STATUE_WRCMD;
			FD628_Command(icmd, dev);
		}
		break;
	case FD628_IOC_STATUS_LED:
		ret = __get_user(dev->status_led_mask, (int __user *)arg);
		break;
	default:		/* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}

	return ret;
}

static unsigned int fd628_dev_poll(struct file *filp, poll_table * wait)
{
	unsigned int mask = 0;
	struct fd628_dev *dev = filp->private_data;
	poll_wait(filp, &dev->kb_waitq, wait);
	if (dev->key_respond_status)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static struct file_operations fd628_fops = {
	.owner = THIS_MODULE,
	.open = fd628_dev_open,
	.release = fd628_dev_release,
	.read = fd628_dev_read,
	.write = fd628_dev_write,
	.unlocked_ioctl = fd628_dev_ioctl,
	.compat_ioctl = fd628_dev_ioctl,
	.poll = fd628_dev_poll,
};

static struct miscdevice fd628_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEV_NAME,
	.fops = &fd628_fops,
};

static int register_fd628_driver(void)
{
	int ret = 0;
	ret = misc_register(&fd628_device);
	if (ret)
		pr_dbg("%s: failed to add fd628 module\n", __func__);
	else
		pr_dbg("%s: Successed to add fd628  module \n", __func__);
	return ret;
}

static void deregister_fd628_driver(void)
{
	int ret = 0;
	ret = misc_deregister(&fd628_device);
	if (ret)
		pr_dbg("%s: failed to deregister fd628 module\n", __func__);
	else
		pr_dbg("%s: Successed to deregister fd628  module \n", __func__);
}


static void fd628_brightness_set(struct led_classdev *cdev,
	enum led_brightness brightness)
{
	pr_info("brightness = %d\n", brightness);

	if(pdata == NULL) 
		return;
	//pdata->dev->brightness = brightness;
	//FD628_SET_BRIGHTNESS(pdata->dev->brightness, pdata->dev, FD628_DISP_ON);
	//pdata->dev->wbuf[4] = brightness;
	//采用递增模式写入显示数据
	//FD628_WrDisp_AddrINC(0x00, 2*5, pdata->dev);
}

static ssize_t led_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "led status is 0x%x\n", pdata->dev->status_led_mask);
}

static ssize_t led_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	if(pdata == NULL) 
		return size;
	
	if (pdata->dev->display_type == DISPLAY_COMMON_ANODE_FD628) {
		if (strncmp(buf,"apps",4) == 0) {
			pdata->dev->status_led_mask |= pdata->dev->led_dots[LED_DOT_ALARM_APPS];
		} else if (strncmp(buf,"setup",5) == 0) {
			pdata->dev->status_led_mask |= pdata->dev->led_dots[LED_DOT_USB_SETUP];
		} else if (strncmp(buf,"usb",3) == 0) {
			pdata->dev->status_led_mask |= pdata->dev->led_dots[LED_DOT_PLAY_USB];
		} else if (strncmp(buf,"sd",2) == 0) {
			pdata->dev->status_led_mask |= pdata->dev->led_dots[LED_DOT_PAUSE_CARD];
		} else if (strncmp(buf,"hdmi",4) == 0) {
			pdata->dev->status_led_mask |= pdata->dev->led_dots[LED_DOT_ETH_HDMI];
		} else if (strncmp(buf,"cvbs",4) == 0) {
			pdata->dev->status_led_mask |= pdata->dev->led_dots[LED_DOT_WIFI_CVBS];
		} else {
			//pr_info("echo wifi | usb | play | pause | eth > led_on\n");
		}
	} else {
		if (strncmp(buf,"alarm",5) == 0) {
			pdata->dev->status_led_mask |= pdata->dev->led_dots[LED_DOT_ALARM_APPS];
		} else if (strncmp(buf,"usb",3) == 0) {
			pdata->dev->status_led_mask |= pdata->dev->led_dots[LED_DOT_USB_SETUP];
		} else if (strncmp(buf,"play",4) == 0) {
			pdata->dev->status_led_mask |= pdata->dev->led_dots[LED_DOT_PLAY_USB];
		} else if (strncmp(buf,"pause",5) == 0) {
			pdata->dev->status_led_mask |= pdata->dev->led_dots[LED_DOT_PAUSE_CARD];
		} else if (strncmp(buf,"eth",3) == 0) {
			pdata->dev->status_led_mask |= pdata->dev->led_dots[LED_DOT_ETH_HDMI];
		} else if (strncmp(buf,"wifi",4) == 0) {
			pdata->dev->status_led_mask |= pdata->dev->led_dots[LED_DOT_WIFI_CVBS];
		} else {
			//pr_info("echo wifi | usb | play | pause | eth > led_on\n");
		}
	}

	return size;
}

static ssize_t led_light_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", pdata->dev->brightness);
}

static ssize_t led_light_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 16, &val);
	if (ret)
		return ret;
	if(pdata == NULL) 
		return ret ;
	
	pr_info("val = %x\n", val);
	pdata->dev->brightness = val;
	FD628_SET_BRIGHTNESS(pdata->dev->brightness, pdata->dev, FD628_DISP_ON);
	
	return size;
}

static ssize_t led_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "led status is 0x%x\n", pdata->dev->status_led_mask);
}

static ssize_t led_off_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	if(pdata == NULL) 
		return size;

	if (pdata->dev->display_type == DISPLAY_COMMON_ANODE_FD628) {
		if (strncmp(buf,"apps",4) == 0) {
			pdata->dev->status_led_mask &= ~pdata->dev->led_dots[LED_DOT_ALARM_APPS];
		} else if (strncmp(buf,"setup",5) == 0) {
			pdata->dev->status_led_mask &= ~pdata->dev->led_dots[LED_DOT_USB_SETUP];
		} else if (strncmp(buf,"usb",3) == 0) {
			pdata->dev->status_led_mask &= ~pdata->dev->led_dots[LED_DOT_PLAY_USB];
		} else if (strncmp(buf,"sd",2) == 0) {
			pdata->dev->status_led_mask &= ~pdata->dev->led_dots[LED_DOT_PAUSE_CARD];
		} else if (strncmp(buf,"hdmi",4) == 0) {
			pdata->dev->status_led_mask &= ~pdata->dev->led_dots[LED_DOT_ETH_HDMI];
		} else if (strncmp(buf,"cvbs",4) == 0) {
			pdata->dev->status_led_mask &= ~pdata->dev->led_dots[LED_DOT_WIFI_CVBS];
		} else {
			//pr_info("echo wifi | usb | play | pause | eth > led_on\n");
		}
	} else {
		if (strncmp(buf,"alarm",5) == 0) {
			pdata->dev->status_led_mask &= ~pdata->dev->led_dots[LED_DOT_ALARM_APPS];
		} else if (strncmp(buf,"usb",3) == 0) {
			pdata->dev->status_led_mask &= ~pdata->dev->led_dots[LED_DOT_USB_SETUP];
		} else if (strncmp(buf,"play",4) == 0) {
			pdata->dev->status_led_mask &= ~pdata->dev->led_dots[LED_DOT_PLAY_USB];
		} else if (strncmp(buf,"pause",5) == 0) {
			pdata->dev->status_led_mask &= ~pdata->dev->led_dots[LED_DOT_PAUSE_CARD];
		} else if (strncmp(buf,"eth",3) == 0) {
			pdata->dev->status_led_mask &= ~pdata->dev->led_dots[LED_DOT_ETH_HDMI];
		} else if (strncmp(buf,"wifi",4) == 0) {
			pdata->dev->status_led_mask &= ~pdata->dev->led_dots[LED_DOT_WIFI_CVBS];
		} else {
			//pr_info("echo wifi | usb | play | pause | eth > led_on\n");
		}
	}

	return size;
}

static DEVICE_ATTR(led_on , 0666, led_on_show , led_on_store);
static DEVICE_ATTR(led_off , 0666, led_off_show , led_off_store);
static DEVICE_ATTR(led_light , 0666, led_light_show , led_light_store);

static void fd628_suspend(struct early_suspend *h)
{
	pr_info("%s!\n", __func__);
	FD628_SET_BRIGHTNESS(pdata->dev->brightness, pdata->dev, FD628_DISP_OFF);
}

static void fd628_resume(struct early_suspend *h)
{
	pr_info("%s!\n", __func__);
	FD628_SET_BRIGHTNESS(pdata->dev->brightness, pdata->dev, FD628_DISP_ON);
}

static int fd628_driver_probe(struct platform_device *pdev)
{
	int state = -EINVAL;
	struct gpio_desc *clk_desc = NULL;
	struct gpio_desc *dat_desc = NULL;
	struct gpio_desc *stb_desc = NULL;
	struct property *chars = NULL;
	struct property *dot_bits = NULL;
	struct property *display_type = NULL;
	int ret;

	pr_dbg("%s get in\n", __func__);

	if (!pdev->dev.of_node) {
		pr_error("fd628_driver: pdev->dev.of_node == NULL!\n");
		state = -EINVAL;
		goto get_fd628_node_fail;
	}

	pdata = kzalloc(sizeof(struct fd628_platform_data), GFP_KERNEL);
	if (!pdata) {
		pr_error("platform data is required!\n");
		state = -EINVAL;
		goto get_fd628_mem_fail;
	}
	memset(pdata, 0, sizeof(struct fd628_platform_data));

	pdata->dev = kzalloc(sizeof(*(pdata->dev)), GFP_KERNEL);
	if (!(pdata->dev)) {
		pr_error("platform dev is required!\n");
		goto get_param_mem_fail;
	}

	clk_desc = of_get_named_gpiod_flags(pdev->dev.of_node,
					    "fd628_gpio_clk", 0, NULL);
	pdata->dev->clk_pin = desc_to_gpio(clk_desc);
	ret = gpio_request(pdata->dev->clk_pin, DEV_NAME);
	if (ret) {
		pr_error("can't request gpio of fd628_clk");
		goto get_param_mem_fail;
	}

	dat_desc = of_get_named_gpiod_flags(pdev->dev.of_node,
					    "fd628_gpio_dat", 0, NULL);
	pdata->dev->dat_pin = desc_to_gpio(dat_desc);
	ret = gpio_request(pdata->dev->dat_pin, DEV_NAME);
	if (ret) {
		pr_error("can't request gpio of fd628_dat");
		goto get_param_mem_fail;
	}

	stb_desc = of_get_named_gpiod_flags(pdev->dev.of_node,
					    "fd628_gpio_stb", 0, NULL);
	pdata->dev->stb_pin = desc_to_gpio(stb_desc);
	ret = gpio_request(pdata->dev->stb_pin, DEV_NAME);
	if (ret) {
		pr_error("can't request gpio of fd628_stb");
		goto get_param_mem_fail;
	}

	chars = of_find_property(pdev->dev.of_node, "fd628_chars", NULL);
	if (!chars || !chars->value) {
		pr_error("can't find fd628_chars list, falling back to defaults.");
		chars = NULL;
	}
	else if (chars->length < 5) {
		pr_error("fd628_chars list is too short, falling back to defaults.");
		chars = NULL;
	}

	for (__u8 i = 0; i < (sizeof(pdata->dev->dat_index) / sizeof(char)); i++)
		pdata->dev->dat_index[i] = i;
	pr_dbg2("chars = %p\n", chars);
	if (chars) {
		__u8 *c = (__u8*)chars->value;
		const int length = min(chars->length, (int)(sizeof(pdata->dev->dat_index) / sizeof(char)));
		pr_dbg2("chars->length = %d\n", chars->length);
		for (int i = 0; i < length; i++) {
			pdata->dev->dat_index[i] = c[i];
			pr_dbg2("char #%d: %d\n", i, c[i]);
		}
	}

	dot_bits = of_find_property(pdev->dev.of_node, "fd628_dot_bits", NULL);
	if (!dot_bits || !dot_bits->value) {
		pr_error("can't find fd628_dot_bits list, falling back to defaults.");
		dot_bits = NULL;
	}
	else if (dot_bits->length < LED_DOT_MAX) {
		pr_error("fd628_dot_bits list is too short, falling back to defaults.");
		dot_bits = NULL;
	}

	for (int i = 0; i < LED_DOT_MAX; i++)
		pdata->dev->led_dots[i] = ledDots[i];
	pr_dbg2("dot_bits = %p\n", dot_bits);
	if (dot_bits) {
		__u8 *d = (__u8*)dot_bits->value;
		pr_dbg2("dot_bits->length = %d\n", dot_bits->length);
		for (int i = 0; i < dot_bits->length; i++) {
			pdata->dev->led_dots[i] = ledDots[d[i]];
			pr_dbg2("dot_bit #%d: %d\n", i, d[i]);
		}
	}

	pdata->dev->display_type = DISPLAY_UNKNOWN;
	display_type = of_find_property(pdev->dev.of_node, "fd628_display_type", NULL);
	if (display_type && display_type->value)
		of_property_read_u32(pdev->dev.of_node, "fd628_display_type", &pdata->dev->display_type);
	pr_dbg2("display_type = %d\n", pdata->dev->display_type);

	pdata->dev->brightness = FD628_Brightness_8;

	register_fd628_driver();
	kp = kzalloc(sizeof(struct kp) ,  GFP_KERNEL);
	if (!kp) {
		kfree(kp);
		return -ENOMEM;
	}
	kp->cdev.name = DEV_NAME;
	kp->cdev.brightness_set = fd628_brightness_set;
	ret = led_classdev_register(&pdev->dev, &kp->cdev);
	if (ret < 0) {
		kfree(kp);
		return ret;
	}

	device_create_file(kp->cdev.dev, &dev_attr_led_on);
	device_create_file(kp->cdev.dev, &dev_attr_led_off);
	device_create_file(kp->cdev.dev, &dev_attr_led_light);
	FD628_Init(pdata->dev);
	applyDisplayType(pdata->dev);
#if 0
	// TODO:初始化，boot阶段显示'boot字符'
	// 'boot'
	//  1 1 0  0 1 1 1  b => 0x7C
	//  1 1 0  0 0 1 1  o => 0x5C
	//  1 0 0  0 1 1 1  t => 0x78
	__u8 data[7];
	data[0] = 0x00;
	data[1] = pdata->dev->display_type != DISPLAY_COMMON_CATHODE_FD628 ? 0x7C : 0x67;
	data[2] = pdata->dev->display_type != DISPLAY_COMMON_CATHODE_FD628 ? 0x5C : 0x63;
	data[3] = pdata->dev->display_type != DISPLAY_COMMON_CATHODE_FD628 ? 0x5C : 0x63;
	data[4] = pdata->dev->display_type != DISPLAY_COMMON_CATHODE_FD628 ? 0x78 : 0x47;
	for (i = 0; i < 5; i++) {
		pdata->dev->wbuf[pdata->dev->dat_index[i]] = data[i];
	}
	//采用递增模式写入显示数据
	FD628_WrDisp_AddrINC(0x00, 2*5, pdata->dev);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	fd628_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	fd628_early_suspend.suspend = fd628_suspend;
	fd628_early_suspend.resume = fd628_resume;
	register_early_suspend(&fd628_early_suspend);
#endif

	return 0;

	  get_param_mem_fail:
	kfree(pdata->dev);
	  get_fd628_mem_fail:
	kfree(pdata);
	  get_fd628_node_fail:
	return state;
}

static int fd628_driver_remove(struct platform_device *pdev)
{
	FD628_SET_BRIGHTNESS(pdata->dev->brightness, pdata->dev, FD628_DISP_OFF);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&fd628_early_suspend);
#endif
	led_classdev_unregister(&kp->cdev);
	deregister_fd628_driver();
#ifdef CONFIG_OF
	gpio_free(pdata->dev->clk_pin);
	gpio_free(pdata->dev->dat_pin);
	gpio_free(pdata->dev->stb_pin);
	kfree(pdata->dev);
	kfree(pdata);
	pdata = NULL;
#endif
	return 0;
}

static void fd628_driver_shutdown(struct platform_device *dev)
{
	pr_dbg("fd628_driver_shutdown");
	FD628_SET_BRIGHTNESS(pdata->dev->brightness, pdata->dev, FD628_DISP_OFF);
}

static int fd628_driver_suspend(struct platform_device *dev, pm_message_t state)
{
	pr_dbg("fd628_driver_suspend");
	FD628_SET_BRIGHTNESS(pdata->dev->brightness, pdata->dev, FD628_DISP_OFF);
	return 0;
}

static int fd628_driver_resume(struct platform_device *dev)
{
	pr_dbg("fd628_driver_resume");
	FD628_SET_BRIGHTNESS(pdata->dev->brightness, pdata->dev, FD628_DISP_ON);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id fd628_dt_match[] = {
	{.compatible = "amlogic,fd628_dev",},
	{},
};
#else
#define fd628_dt_match NULL
#endif

static struct platform_driver fd628_driver = {
	.probe = fd628_driver_probe,
	.remove = fd628_driver_remove,
	.suspend = fd628_driver_suspend,
	.shutdown = fd628_driver_shutdown,
	.resume = fd628_driver_resume,
	.driver = {
		   .name = DEV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = fd628_dt_match,
		   },
};

static int __init fd628_driver_init(void)
{
	pr_dbg("Fd628 Driver init.\n");
	return platform_driver_register(&fd628_driver);
}

static void __exit fd628_driver_exit(void)
{
	pr_dbg("Fd628 Driver exit.\n");
	platform_driver_unregister(&fd628_driver);
}

module_init(fd628_driver_init);
module_exit(fd628_driver_exit);

MODULE_AUTHOR("chenmy");
MODULE_DESCRIPTION("fd628 Driver");
MODULE_LICENSE("GPL");
