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

#include "le_vfd_drv.h"

#include "protocols/spi_3w.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend fd628_early_suspend;
#endif

static struct fd628_platform_data *pdata = NULL;
struct kp {
	struct led_classdev cdev;
};

static struct kp *kp;

static struct protocol_interface *spi = NULL;

/****************************************************************
 *	Function Name:		FD628_Command
 *	Description:		Send control command
 *	Parameters:		INT8U control command
 *	Return value:		void
****************************************************************/
static void FD628_Command(u_int8 cmd, struct fd628_dev *dev)
{
	spi->write_byte(cmd);
}

/****************************************************************
 *	Function Name:		FD628_GetKey
 *	Description:		Read key code value
 *	Parameters:		void
 *	Return value:		INT32U returns the key value
 **************************************************************************************************************************************
Key value encoding
	| 0	| 0	| 0	| 0	| 0	| 0	| KS10	| KS9	| KS8	| KS7	| KS6	| KS5	| KS4	| KS3	| KS2	| KS1	|
KEYI1 	| bit15	| bit14	| bit13	| bit12	| bit11	| bit10	| bit9	| bit8	| bit7	| bit6	| bit5	| bit4	| bit3	| bit2	| bit1	| bit0	|
KEYI2 	| bit31	| bit30	| bit29	| bit28	| bit27	| bit26	| bit25	| bit24	| bit23	| bit22	| bit21	| bit20	| bit19	| bit18	| bit17	| bit16	|
***************************************************************************************************************************************/
static u_int32 FD628_GetKey(struct fd628_dev *dev)
{
	u_int8 i, keyDataBytes[5];
	u_int32 FD628_KeyData = 0;
	spi->write_byte(FD628_KEY_RDCMD);
	spi->read_data(keyDataBytes, sizeof(keyDataBytes));
	for (i = 0; i != 5; i++) {			/* Pack 5 bytes of key code values into 2 words */
		if (keyDataBytes[i] & 0x01)
			FD628_KeyData |= (0x00000001 << i * 2);
		if (keyDataBytes[i] & 0x02)
			FD628_KeyData |= (0x00010000 << i * 2);
		if (keyDataBytes[i] & 0x08)
			FD628_KeyData |= (0x00000002 << i * 2);
		if (keyDataBytes[i] & 0x10)
			FD628_KeyData |= (0x00020000 << i * 2);
	}

	return (FD628_KeyData);
}

/****************************************************************
 *	Function Name:		FD628_WrDisp_AddrINC
 *	Description:		Write to display in address increment mode
 *	Parameters:		INT8U Addr - Start address in FD628 RAM; See address table in controller datasheet.
 *				INT8U DataLen - Length of data to be written in bytes.
 *	Return value:		BOOLEAN; Returns 0 if successful; Returns 1 if data will not fit in RAM at a given address.
 *	Usage:			First, write data into the buffer (dev->wbuf), afterwards call FD628_WrDisp_AddrINC() function.
****************************************************************/
static int FD628_WrDisp_AddrINC(u_int8 Addr, u_int8 DataLen,
				struct fd628_dev *dev)
{
	u_int8 buf[15];

	if (DataLen + Addr > 14)
		return (1);
	buf[0] = FD628_DIGADDR_WRCMD | Addr;
	memcpy(buf+1, dev->wbuf, DataLen);

	FD628_Command(FD628_ADDR_INC_DIGWR_CMD, dev);
	spi->write_data(buf, DataLen);
	return (0);
}

/****************************************************************
 *	Function Name:		FD628_SET_DISPLAY_MODE
 *	Description:		Sets display mode
 *	Parameters:		cmd
					FD628_4DIG_CMD		0x00	4 digits
					FD628_5DIG_CMD		0x01	5 digits
					FD628_6DIG_CMD		0x02	6 digits
					FD628_7DIG_CMD		0x03	7 digits
					Segment count is controller dependent
 *	Return value:		void
****************************************************************/
static void FD628_SET_DISPLAY_MODE(u_int8 cmd, struct fd628_dev *dev)
{
	FD628_Command(cmd, dev);
}

/****************************************************************
 *	Function Name: 		FD628_SET_BRIGHTNESS
 *	Description:		Set display brightness
 *	Parameters:
cmd:
	FD628_Brightness_1	0x00	 1/16 pulse width
	FD628_Brightness_2	0x01	 2/16 pulse width
	FD628_Brightness_3	0x02	 4/16 pulse width
	FD628_Brightness_4	0x03	10/16 pulse width
	FD628_Brightness_5	0x04	11/16 pulse width
	FD628_Brightness_6	0x05	12/16 pulse width
	FD628_Brightness_7	0x06	13/16 pulse width
	FD628_Brightness_8	0x07	14/16 pulse width
status:
	FD628_DISP_ON		Turn display on
	FD628_DISP_OFF		Turn display off
 *	Return value:		void
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
 *	Function Name:		FD628_Init
 *	Description:		FD628 initialization sequence
 *	Parameters:		void
 *	Return value:		void
****************************************************************/
static void FD628_Init(struct fd628_dev *dev)
{
	spi = init_spi_3w(dev->clk_pin, dev->dat_pin, dev->stb_pin, SPI_DELAY_100KHz);
	gpio_direction_output(dev->clk_pin, 1);	/* Set CLK high */
	gpio_direction_output(dev->stb_pin, 1);	/* Set STB high */
	gpio_direction_output(dev->dat_pin, 1);	/* Set DIO high */
	gpio_direction_input(dev->dat_pin);	/* Set DIO as input */
	FD628_DELAY_BUF;			/* The interval between the end of the communication and the start of the next communication */
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

/**
 * Source for the transpose algorithm:
   http://www.hackersdelight.org/hdcodetxt/transpose8.c.txt
 */
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
 * @param buf: Incoming LED codes.
 * 		  [0]	Display indicators mask (wifi, eth, usb, etc.)
 * 		  [1-4]	7 segment characters, to be displayed left to right.
 * @return
 */
static ssize_t fd628_dev_write(struct file *filp, const char __user * buf,
				   size_t count, loff_t * f_pos)
{
	struct fd628_dev *dev;
	struct fd628_dtb_config *dtb = NULL;
	unsigned long missing;
	int status = 0;
	int i = 0;
	const int dataMaxLen = 7;
	unsigned short data[7];
	unsigned char trans[8];

	dev = filp->private_data;
	dtb = &dev->dtb_active;

	count /= sizeof(data[0]);
	if (count > dataMaxLen)
		count = dataMaxLen;

	memset(data, 0, sizeof(data));
	memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
	missing = copy_from_user(data, buf, count*sizeof(data[0]));
	if (missing == 0) {
		// Apply dot remap for colon.
		if (data[0] & ledDots[LED_DOT_SEC]) {
			data[0] &= ~ledDots[LED_DOT_SEC];
			data[0] |= dtb->led_dots[LED_DOT_SEC];
		}
		// Apply LED indicators mask (usb, eth, wifi etc.)
		data[0] |= dev->status_led_mask;

		switch (dtb->display.type) {
		case DISPLAY_TYPE_5D_7S_NORMAL:
		case DISPLAY_TYPE_5D_7S_T95:
		case DISPLAY_TYPE_5D_7S_X92:
		case DISPLAY_TYPE_5D_7S_ABOX:
		case DISPLAY_TYPE_TAP1:
		default:
			for (i = 0; i < count; i++)
				dev->wbuf[dtb->dat_index[i]] = data[i];
			break;
		case DISPLAY_TYPE_FD620_REF:
			for (i = 0; i < count; i++)
				dev->wbuf[dtb->dat_index[i]] = data[i];
			if (data[0] & dtb->led_dots[LED_DOT_SEC])
				dev->wbuf[dtb->dat_index[0]] |= 0x80;				// DP is the colon.
			break;
		}

		if (dtb->display.flags & DISPLAY_FLAG_TRANSPOSED) {
			count = dataMaxLen;
			memset(trans, 0, sizeof(trans));
			for (i = 0; i < count; i++)
				trans[i] = (unsigned char)dev->wbuf[i] << 1;
			transpose8rS64(trans, trans);
			memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
			for (i = 0; i < dataMaxLen; i++)
				dev->wbuf[i] = trans[i+1];
		}

		switch (dtb->display.controller) {
		case CONTROLLER_FD628:
			// Memory map:
			// S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 xx S12 S13 S14 xx xx
			// b0 b1 b2 b3 b4 b5 b6 b7 b0 b1  b2 b3  b4  b5  b6 b7
			for (i = 0; i < count; i++)
				dev->wbuf[i] |= (dev->wbuf[i] & 0xFC00) << 1;
			break;
		case CONTROLLER_FD620:
			// Memory map:
			// S1 S2 S3 S4 S5 S6 S7 xx xx xx xx xx xx S8 xx xx
			// b0 b1 b2 b3 b4 b5 b6 b7 b0 b1 b2 b3 b4 b5 b6 b7
			if (count > 5)	// This controller can hold 5 words.
				count = 5;
			for (i = 0; i < count; i++)
				dev->wbuf[i] |= (dev->wbuf[i] & 0x80) ? 0x2000 : 0;
			break;
		case CONTROLLER_TM1618:
			// Memory map:
			// S1 S2 S3 S4 S5 xx xx xx xx xx xx S12 S13 S14 xx xx
			// b0 b1 b2 b3 b4 b5 b6 b7 b0 b1 b2 b3  b4  b5  b6 b7
			for (i = 0; i < count; i++)
				dev->wbuf[i] |= (dev->wbuf[i] & 0xE0) << 6;
			break;
		}

		FD628_WrDisp_AddrINC(0x00, 2 * count, dev);
		status = count;
	}
	pr_dbg("fd628_dev_write count : %d\n", count);
	return status;
}

static int set_display_brightness(struct fd628_dev *dev, u_int8 new_brightness)
{
	int ret = 0;
	if (new_brightness >= FD628_Brightness_1 && new_brightness <= FD628_Brightness_8) {
		dev->brightness = (u_int8)new_brightness;
		FD628_SET_BRIGHTNESS(dev->brightness, dev, FD628_DISP_ON);
		ret = 1;
	}

	return ret;
}

static int set_display_type(struct fd628_dev *dev, int new_display_type)
{
	int ret = 0;
	struct fd628_display display;
	memcpy(&display, &new_display_type, sizeof(struct fd628_display));
	if (display.type < DISPLAY_TYPE_MAX && display.controller < CONTROLLER_MAX) {
		dev->dtb_active.display = display;
		switch (display.controller) {
			case CONTROLLER_FD628:
			default:
				FD628_SET_DISPLAY_MODE(FD628_7DIG_CMD, dev);
				break;
			case CONTROLLER_FD620:
				switch (display.type) {
				case DISPLAY_TYPE_FD620_REF:
					FD628_SET_DISPLAY_MODE(FD628_4DIG_CMD, dev);
					break;
				default:
					FD628_SET_DISPLAY_MODE(FD628_5DIG_CMD, dev);
					break;
				}
				break;
			case CONTROLLER_TM1618:
				switch (display.type) {
				case DISPLAY_TYPE_TAP1:
					FD628_SET_DISPLAY_MODE(FD628_7DIG_CMD, dev);
					break;
				case DISPLAY_TYPE_FD620_REF:
					FD628_SET_DISPLAY_MODE(FD628_4DIG_CMD, dev);
					break;
				default:
					FD628_SET_DISPLAY_MODE(FD628_5DIG_CMD, dev);
					break;
				}
				break;
		}
		FD628_SET_BRIGHTNESS(dev->brightness, dev, FD628_DISP_ON);
		memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
		ret = 1;
	}

	return ret;
}

static long fd628_dev_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	int err = 0, ret = 0, temp = 0;
	struct fd628_dev *dev;
	__u8 val = 1, icmd = FD628_Brightness_8;
	__u8 temp_chars_order[sizeof(dev->dtb_active.dat_index)];
	dev = filp->private_data;

	if (_IOC_TYPE(cmd) != FD628_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) >= FD628_IOC_MAXNR)
		return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch (cmd) {
	case FD628_IOC_USE_DTB_CONFIG:
		dev->dtb_active = dev->dtb_default;
		break;
	case FD628_IOC_GDISPLAY_TYPE:
		memcpy(&temp, &dev->dtb_active.display, sizeof(int));
		ret = __put_user(temp, (int __user *)arg);
		break;
	case FD628_IOC_SDISPLAY_TYPE:
		ret = __get_user(temp, (int __user *)arg);
		if (!ret && !set_display_type(dev, temp))
			ret = -ERANGE;
		break;
	case FD628_IOC_SCHARS_ORDER:
		ret = __copy_from_user(temp_chars_order, (__u8 __user *)arg, sizeof(dev->dtb_active.dat_index));
		if (!ret)
			memcpy(dev->dtb_active.dat_index, temp_chars_order, sizeof(dev->dtb_active.dat_index));
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
		if (!ret && !set_display_brightness(dev, (u_int8)temp))
			ret = -ERANGE;
		break;
	case FD628_IOC_GBRIGHT:
		ret = __put_user(dev->brightness, (int __user *)arg);
		break;
	case FD628_IOC_POWER:
		ret = __get_user(val, (int __user *)arg);
		if (val)
			icmd = FD628_DISP_ON | dev->brightness;
		else
			icmd = FD628_DISP_OFF;
		icmd &= 0x0f;
		icmd |= FD628_DISP_STATUE_WRCMD;
		FD628_Command(icmd, dev);
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
}

static int led_cmd_ioc = 0;

static ssize_t led_cmd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	*buf = '\0';

	switch(led_cmd_ioc) {
		case FD628_IOC_GMODE:
			ret = scnprintf(buf, PAGE_SIZE, "%d", pdata->dev->mode);
			break;
		case FD628_IOC_GBRIGHT:
			ret = scnprintf(buf, PAGE_SIZE, "%d", pdata->dev->brightness);
			break;
		case FD628_IOC_GVER:
			ret = scnprintf(buf, PAGE_SIZE, "%s", FD628_DRIVER_VERSION);
			break;
		case FD628_IOC_GDISPLAY_TYPE:
			ret = scnprintf(buf, PAGE_SIZE, "0x%02X%02X%02X%02X", pdata->dev->dtb_active.display.reserved, pdata->dev->dtb_active.display.flags,
				pdata->dev->dtb_active.display.controller, pdata->dev->dtb_active.display.type);
			break;
	}

	led_cmd_ioc = 0;
	return ret;
}

static ssize_t led_cmd_store(struct device *_dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct fd628_dev *dev = pdata->dev;
	int cmd, temp;
	u_int8 icmd;
	led_cmd_ioc = 0;
	
	if (size < 2*sizeof(int))
		return -EFAULT;
	memcpy(&cmd, buf, sizeof(int));
	if (_IOC_TYPE(cmd) != FD628_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) >= FD628_IOC_MAXNR)
		return -ENOTTY;

	buf += sizeof(int);
	memcpy(&temp, buf, sizeof(int));
	switch (cmd) {
		case FD628_IOC_SMODE:
			dev->mode = (u_int8)temp;
			FD628_SET_DISPLAY_MODE(dev->mode, dev);
			break;
		case FD628_IOC_SBRIGHT:
			if (!set_display_brightness(dev, (u_int8)temp))
				size = -ERANGE;
			break;
		case FD628_IOC_POWER:
			if (temp)
				icmd = FD628_DISP_ON | dev->brightness;
			else
				icmd = FD628_DISP_OFF;
			icmd &= 0x0f;
			icmd |= FD628_DISP_STATUE_WRCMD;
			FD628_Command(icmd, dev);
			break;
		case FD628_IOC_STATUS_LED:
			dev->status_led_mask = (u_int8)temp;
			break;
		case FD628_IOC_SDISPLAY_TYPE:
			if (!set_display_type(dev, temp))
				size = -ERANGE;
			break;
		case FD628_IOC_SCHARS_ORDER:
			if (size >= sizeof(dev->dtb_active.dat_index)+sizeof(int))
				memcpy(dev->dtb_active.dat_index, buf, sizeof(dev->dtb_active.dat_index));
			else
				size = -EFAULT;
			break;
		case FD628_IOC_USE_DTB_CONFIG:
			pdata->dev->dtb_active = pdata->dev->dtb_default;
			break;
		case FD628_IOC_GMODE:
		case FD628_IOC_GBRIGHT:
		case FD628_IOC_GVER:
		case FD628_IOC_GDISPLAY_TYPE:
			led_cmd_ioc = cmd;
			break;
	}

	return size;
}

static ssize_t led_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "led status is 0x%x\n", pdata->dev->status_led_mask);
}

static ssize_t led_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct fd628_dtb_config *dtb = NULL;
	if(pdata == NULL)
		return size;

	dtb = &pdata->dev->dtb_active;
	switch (dtb->display.type) {
	case DISPLAY_TYPE_5D_7S_NORMAL:
	case DISPLAY_TYPE_5D_7S_T95:
	default:
		if (strncmp(buf,"alarm",5) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT1_ALARM];
		} else if (strncmp(buf,"usb",3) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT1_USB];
		} else if (strncmp(buf,"play",4) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT1_PLAY];
		} else if (strncmp(buf,"pause",5) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT1_PAUSE];
		} else if (strncmp(buf,"colon",5) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT1_SEC];
		} else if (strncmp(buf,"eth",3) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT1_ETH];
		} else if (strncmp(buf,"wifi",4) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT1_WIFI];
		}
		break;
	case DISPLAY_TYPE_5D_7S_X92:
		if (strncmp(buf,"apps",4) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT2_APPS];
		} else if (strncmp(buf,"setup",5) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT2_SETUP];
		} else if (strncmp(buf,"usb",3) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT2_USB];
		} else if (strncmp(buf,"sd",2) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT2_CARD];
		} else if (strncmp(buf,"colon",5) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT2_SEC];
		} else if (strncmp(buf,"hdmi",4) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT2_HDMI];
		} else if (strncmp(buf,"cvbs",4) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT2_CVBS];
		}
		break;
	case DISPLAY_TYPE_5D_7S_ABOX:
		if (strncmp(buf,"power",5) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT3_POWER];
		} else if (strncmp(buf,"eth",3) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT3_LAN];
		} else if (strncmp(buf,"colon",5) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT3_SEC];
		} else if (strncmp(buf,"wifi",4) == 0) {
			pdata->dev->status_led_mask |= dtb->led_dots[LED_DOT3_WIFIHI] | dtb->led_dots[LED_DOT3_WIFILO];
		}
		break;
	}

	return size;
}

static ssize_t led_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "led status is 0x%x\n", pdata->dev->status_led_mask);
}

static ssize_t led_off_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct fd628_dtb_config *dtb = NULL;
	if(pdata == NULL)
		return size;

	dtb = &pdata->dev->dtb_active;
	switch (dtb->display.type) {
	case DISPLAY_TYPE_5D_7S_NORMAL:
	case DISPLAY_TYPE_5D_7S_T95:
	default:
		if (strncmp(buf,"alarm",5) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT1_ALARM];
		} else if (strncmp(buf,"usb",3) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT1_USB];
		} else if (strncmp(buf,"play",4) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT1_PLAY];
		} else if (strncmp(buf,"pause",5) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT1_PAUSE];
		} else if (strncmp(buf,"colon",5) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT1_SEC];
		} else if (strncmp(buf,"eth",3) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT1_ETH];
		} else if (strncmp(buf,"wifi",4) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT1_WIFI];
		}
		break;
	case DISPLAY_TYPE_5D_7S_X92:
		if (strncmp(buf,"apps",4) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT2_APPS];
		} else if (strncmp(buf,"setup",5) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT2_SETUP];
		} else if (strncmp(buf,"usb",3) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT2_USB];
		} else if (strncmp(buf,"sd",2) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT2_CARD];
		} else if (strncmp(buf,"colon",5) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT2_SEC];
		} else if (strncmp(buf,"hdmi",4) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT2_HDMI];
		} else if (strncmp(buf,"cvbs",4) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT2_CVBS];
		}
		break;
	case DISPLAY_TYPE_5D_7S_ABOX:
		if (strncmp(buf,"power",5) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT3_POWER];
		} else if (strncmp(buf,"eth",3) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT3_LAN];
		} else if (strncmp(buf,"colon",5) == 0) {
			pdata->dev->status_led_mask &= ~dtb->led_dots[LED_DOT3_SEC];
		} else if (strncmp(buf,"wifi",4) == 0) {
			pdata->dev->status_led_mask &= ~(dtb->led_dots[LED_DOT3_WIFIHI] | dtb->led_dots[LED_DOT3_WIFILO]);
		}
		break;
	}

	return size;
}

static DEVICE_ATTR(led_cmd , 0666, led_cmd_show , led_cmd_store);
static DEVICE_ATTR(led_on , 0666, led_on_show , led_on_store);
static DEVICE_ATTR(led_off , 0666, led_off_show , led_off_store);

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

static unsigned char vfd_gpio_clk[3];
static unsigned char vfd_gpio_dat[3];
static unsigned char vfd_gpio_stb[3];
static unsigned char vfd_chars[7] = { 0, 1, 2, 3, 4, 5, 6 };
static unsigned char vfd_dot_bits[8] = { 0, 1, 2, 3, 4, 5, 6, 0 };
static unsigned char vfd_display_type[4] = { 0x00, 0x00, 0x00, 0x00 };
static int vfd_gpio_clk_argc = 0;
static int vfd_gpio_dat_argc = 0;
static int vfd_gpio_stb_argc = 0;
static int vfd_chars_argc = 0;
static int vfd_dot_bits_argc = 0;
static int vfd_display_type_argc = 0;

module_param_array(vfd_gpio_clk, byte, &vfd_gpio_clk_argc, 0000);
module_param_array(vfd_gpio_dat, byte, &vfd_gpio_dat_argc, 0000);
module_param_array(vfd_gpio_stb, byte, &vfd_gpio_stb_argc, 0000);
module_param_array(vfd_chars, byte, &vfd_chars_argc, 0000);
module_param_array(vfd_dot_bits, byte, &vfd_dot_bits_argc, 0000);
module_param_array(vfd_display_type, byte, &vfd_display_type_argc, 0000);

static void print_param_debug(const char *label, int argc, unsigned char param[])
{
	int i, len = 0;
	char buffer[1024];
	len = scnprintf(buffer, sizeof(buffer), "%s", label);
	if (argc)
		for (i = 0; i < argc; i++)
			len += scnprintf(buffer + len, sizeof(buffer), "#%d = 0x%02X; ", i, param[i]);
	else
		len += scnprintf(buffer + len, sizeof(buffer), "Empty.");
	pr_dbg2("%s\n", buffer);
}

static int is_right_chip(struct gpio_chip *chip, void *data)
{
	pr_dbg("is_right_chip %s | %s | %d\n", chip->label, (char*)data, strcmp(data, chip->label));
	if (strcmp(data, chip->label) == 0)
		return 1;
	return 0;
}

static int get_chip_pin_number(const unsigned char gpio[])
{
	int pin = -1;
	if (gpio[0] < 6) {
		struct gpio_chip *chip;
		const char *pin_banks[] = { "banks", "ao-bank", "gpio0", "gpio1", "gpio2", "gpio3" };
		const char *bank_name = pin_banks[gpio[0]];

		chip = gpiochip_find((char *)bank_name, is_right_chip);
		if (chip) {
			if (chip->ngpio > gpio[1])
				pin = chip->base + gpio[1];
			pr_dbg2("\"%s\" chip found.\tbase = %d, pin count = %d, pin = %d, offset = %d\n", bank_name, chip->base, chip->ngpio, gpio[1], pin);
		} else {
			pr_dbg2("\"%s\" chip was not found\n", bank_name);
		}
	}

	return pin;
}

static int verify_module_params(struct fd628_dev *dev)
{
	int ret = (vfd_gpio_clk_argc == 3 && vfd_gpio_dat_argc == 3 && vfd_gpio_stb_argc == 3 &&
			vfd_chars_argc >= 5 && vfd_dot_bits_argc >= 7 && vfd_display_type_argc == 4) ? 1 : -1;

	print_param_debug("vfd_gpio_clk:\t\t", vfd_gpio_clk_argc, vfd_gpio_clk);
	print_param_debug("vfd_gpio_dat:\t\t", vfd_gpio_dat_argc, vfd_gpio_dat);
	print_param_debug("vfd_gpio_stb:\t\t", vfd_gpio_stb_argc, vfd_gpio_stb);
	print_param_debug("vfd_chars:\t\t", vfd_chars_argc, vfd_chars);
	print_param_debug("vfd_dot_bits:\t\t", vfd_dot_bits_argc, vfd_dot_bits);
	print_param_debug("vfd_display_type:\t", vfd_display_type_argc, vfd_display_type);

	if (ret >= 0)
		if ((ret = dev->clk_pin = get_chip_pin_number(vfd_gpio_clk)) < 0)
			pr_error("Could not get pin number for vfd_gpio_clk\n");
	if (ret >= 0)
		if ((ret = dev->dat_pin = get_chip_pin_number(vfd_gpio_dat)) < 0)
			pr_error("Could not get pin number for vfd_gpio_dat\n");
	if (ret >= 0)
		if ((ret = dev->stb_pin = get_chip_pin_number(vfd_gpio_stb)) < 0)
			pr_error("Could not get pin number for vfd_gpio_stb\n");

	if (ret >= 0) {
		int i;
		for (i = 0; i < 7; i++)
			dev->dtb_active.dat_index[i] = vfd_chars[i];
		for (i = 0; i < 8; i++)
			dev->dtb_active.led_dots[i] = ledDots[vfd_dot_bits[i]];
		dev->dtb_active.display.type = vfd_display_type[0];
		dev->dtb_active.display.reserved = vfd_display_type[1];
		dev->dtb_active.display.flags = vfd_display_type[2];
		dev->dtb_active.display.controller = vfd_display_type[3];
	}

	return ret >= 0;
}

static int fd628_driver_probe(struct platform_device *pdev)
{
	int state = -EINVAL;
	struct gpio_desc *clk_desc = NULL;
	struct gpio_desc *dat_desc = NULL;
	struct gpio_desc *stb_desc = NULL;
	struct property *chars_prop = NULL;
	struct property *dot_bits_prop = NULL;
	struct property *display_type_prop = NULL;
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

	if (!verify_module_params(pdata->dev)) {
		pr_error("Failed to verify FD628 configuration file, attempt using device tree as fallback.\n");
		if (of_find_property(pdev->dev.of_node, MOD_NAME_CLK, NULL)) {
			clk_desc = of_get_named_gpiod_flags(pdev->dev.of_node, MOD_NAME_CLK, 0, NULL);
			pdata->dev->clk_pin = desc_to_gpio(clk_desc);
			pr_dbg2("fd628_gpio_clk pin = %d\n", pdata->dev->clk_pin);
		} else {
			pdata->dev->clk_pin = -1;
			pr_dbg2("fd628_gpio_clk pin entry not found\n");
		}

		if (of_find_property(pdev->dev.of_node, MOD_NAME_DAT, NULL)) {
			dat_desc = of_get_named_gpiod_flags(pdev->dev.of_node, MOD_NAME_DAT, 0, NULL);
			pdata->dev->dat_pin = desc_to_gpio(dat_desc);
			pr_dbg2("fd628_gpio_dat pin = %d\n", pdata->dev->dat_pin);
		} else {
			pdata->dev->dat_pin = -1;
			pr_dbg2("fd628_gpio_dat pin entry not found\n");
		}

		if (of_find_property(pdev->dev.of_node, MOD_NAME_STB, NULL)) {
			stb_desc = of_get_named_gpiod_flags(pdev->dev.of_node, MOD_NAME_STB, 0, NULL);
			pdata->dev->stb_pin = desc_to_gpio(stb_desc);
			pr_dbg2("fd628_gpio_stb pin = %d\n", pdata->dev->stb_pin);
		} else {
			pdata->dev->stb_pin = -1;
			pr_dbg2("fd628_gpio_stb pin entry not found\n");
		}

		chars_prop = of_find_property(pdev->dev.of_node, MOD_NAME_CHARS, NULL);
		if (!chars_prop || !chars_prop->value) {
			pr_error("can't find %s list, falling back to defaults.", MOD_NAME_CHARS);
			chars_prop = NULL;
		}
		else if (chars_prop->length < 5) {
			pr_error("%s list is too short, falling back to defaults.", MOD_NAME_CHARS);
			chars_prop = NULL;
		}

		for (__u8 i = 0; i < (sizeof(pdata->dev->dtb_active.dat_index) / sizeof(char)); i++)
			pdata->dev->dtb_active.dat_index[i] = i;
		pr_dbg2("chars_prop = %p\n", chars_prop);
		if (chars_prop) {
			__u8 *c = (__u8*)chars_prop->value;
			const int length = min(chars_prop->length, (int)(sizeof(pdata->dev->dtb_active.dat_index) / sizeof(char)));
			pr_dbg2("chars_prop->length = %d\n", chars_prop->length);
			for (int i = 0; i < length; i++) {
				pdata->dev->dtb_active.dat_index[i] = c[i];
				pr_dbg2("char #%d: %d\n", i, c[i]);
			}
		}

		dot_bits_prop = of_find_property(pdev->dev.of_node, MOD_NAME_DOTS, NULL);
		if (!dot_bits_prop || !dot_bits_prop->value) {
			pr_error("can't find %s list, falling back to defaults.", MOD_NAME_DOTS);
			dot_bits_prop = NULL;
		}
		else if (dot_bits_prop->length < LED_DOT_MAX) {
			pr_error("%s list is too short, falling back to defaults.", MOD_NAME_DOTS);
			dot_bits_prop = NULL;
		}

		for (int i = 0; i < LED_DOT_MAX; i++)
			pdata->dev->dtb_active.led_dots[i] = ledDots[i];
		pr_dbg2("dot_bits_prop = %p\n", dot_bits_prop);
		if (dot_bits_prop) {
			__u8 *d = (__u8*)dot_bits_prop->value;
			pr_dbg2("dot_bits_prop->length = %d\n", dot_bits_prop->length);
			for (int i = 0; i < dot_bits_prop->length; i++) {
				pdata->dev->dtb_active.led_dots[i] = ledDots[d[i]];
				pr_dbg2("dot_bit #%d: %d\n", i, d[i]);
			}
		}

		memset(&pdata->dev->dtb_active.display, 0, sizeof(struct fd628_display));
		display_type_prop = of_find_property(pdev->dev.of_node, MOD_NAME_TYPE, NULL);
		if (display_type_prop && display_type_prop->value)
			of_property_read_u32(pdev->dev.of_node, MOD_NAME_TYPE, (int*)&pdata->dev->dtb_active.display);
		pr_dbg2("display.type = %d, display.controller = %d, pdata->dev->dtb_active.display.flags = 0x%02X\n",
			pdata->dev->dtb_active.display.type, pdata->dev->dtb_active.display.controller, pdata->dev->dtb_active.display.flags);
	}

	if (pdata->dev->clk_pin >= 0)
		ret = gpio_request(pdata->dev->clk_pin, DEV_NAME);
	if (ret) {
		pr_error("can't request gpio of gpio_clk");
		goto get_param_mem_fail;
	}

	if (pdata->dev->dat_pin >= 0)
		ret = gpio_request(pdata->dev->dat_pin, DEV_NAME);
	if (ret) {
		pr_error("can't request gpio of gpio_dat");
		goto get_param_mem_fail;
	}

	if (pdata->dev->stb_pin >= 0)
		ret = gpio_request(pdata->dev->stb_pin, DEV_NAME);
	if (ret) {
		pr_error("can't request gpio of gpio_stb");
		goto get_param_mem_fail;
	}

	pdata->dev->dtb_default = pdata->dev->dtb_active;
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
	device_create_file(kp->cdev.dev, &dev_attr_led_cmd);
	FD628_Init(pdata->dev);
	memcpy(&ret, &pdata->dev->dtb_active.display, sizeof(int));
	set_display_type(pdata->dev, ret);
#if 0
	// TODO: Display 'boot' during POST/boot.
	// 'boot'
	//  1 1 0  0 1 1 1  b => 0x7C
	//  1 1 0  0 0 1 1  o => 0x5C
	//  1 0 0  0 1 1 1  t => 0x78
	__u8 data[7];
	data[0] = 0x00;
	data[1] = pdata->dev->dtb_active.display.flags & DISPLAY_TYPE_TRANSPOSED ? 0x7C : 0x67;
	data[2] = pdata->dev->dtb_active.display.flags & DISPLAY_TYPE_TRANSPOSED ? 0x5C : 0x63;
	data[3] = pdata->dev->dtb_active.display.flags & DISPLAY_TYPE_TRANSPOSED ? 0x5C : 0x63;
	data[4] = pdata->dev->dtb_active.display.flags & DISPLAY_TYPE_TRANSPOSED ? 0x78 : 0x47;
	for (i = 0; i < 5; i++) {
		pdata->dev->wbuf[pdata->dev->dtb_active.dat_index[i]] = data[i];
	}
	// Write data in incremental mode
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
	{.compatible = "le,vfd",},
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

MODULE_AUTHOR("Arthur L., chenmy");
MODULE_DESCRIPTION("fd628 Driver");
MODULE_LICENSE("GPL");
