#include "../protocols/i2c.h"
#include "../protocols/spi_3w.h"
#include "fd650.h"

/* ****************************** Define FD650 Commands ****************************** */
#define FD650_KEY_RDCMD		0x49	/* Read keys command			*/
#define FD650_MODE_WRCMD		0x48	/* Write mode command			*/
#define FD650_DISP_ON			0x01	/* FD650 Display On			*/
#define FD650_DISP_OFF			0x00	/* FD650 Display Off			*/
#define FD650_7SEG_CMD			0x40	/* Set FD650 to work in 7-segment mode	*/
#define FD650_8SEG_CMD			0x00	/* Set FD650 to work in 8-segment mode	*/
#define FD650_BASE_ADDR		0x68	/* Base data address			*/
#define FD650_DISP_STATE_WRCMD		0x00	/* Set display modw command		*/
/* *********************************************************************************** */

static void init(void);
static unsigned short get_brightness_levels_count(void);
static unsigned short get_brightness_level(void);
static unsigned char set_brightness_level(unsigned short level);
static unsigned char get_power(void);
static void set_power(unsigned char state);
static struct fd628_display *get_display_type(void);
static unsigned char set_display_type(struct fd628_display *display);
static void set_icon(const char *name, unsigned char state);
static size_t read_data(unsigned char *data, size_t length);
static size_t write_data(const unsigned char *data, size_t length);

static struct controller_interface fd650_interface = {
	.init = init,
	.get_brightness_levels_count = get_brightness_levels_count,
	.get_brightness_level = get_brightness_level,
	.set_brightness_level = set_brightness_level,
	.get_power = get_power,
	.set_power = set_power,
	.get_display_type = get_display_type,
	.set_display_type = set_display_type,
	.set_icon = set_icon,
	.read_data = read_data,
	.write_data = write_data,
};

static struct fd628_dev *dev = NULL;
static struct protocol_interface *protocol = NULL;
static unsigned char ram_grid_count = 4;
static unsigned char ram_size = 8;

struct controller_interface *init_fd650(struct fd628_dev *_dev)
{
	dev = _dev;
	init();
	return &fd650_interface;
}

static size_t fd650_write_data(unsigned char address, const unsigned char *data, size_t length)
{
	unsigned char cmd = FD650_BASE_ADDR | (address & 0x07), i;
	if (length + address > ram_size)
		return (-1);

	protocol->write_byte(FD650_MODE_WRCMD);
	for (i = 0; i < length; i += 2, cmd += 2)
		protocol->write_cmd_data(&cmd, 1, data + i, 1);
	return (0);
}

static void init(void)
{
	protocol = init_i2c(0, dev->clk_pin, dev->dat_pin, I2C_DELAY_100KHz);
	memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
	set_brightness_level(dev->brightness);
}

static unsigned short get_brightness_levels_count(void)
{
	return 8;
}

static unsigned short get_brightness_level(void)
{
	return dev->brightness;
}

static unsigned char set_brightness_level(unsigned short level)
{
	dev->brightness = level & 0x7;
	protocol->write_byte(FD650_MODE_WRCMD);
	protocol->write_byte(FD650_DISP_STATE_WRCMD | (dev->brightness << 4) | FD650_DISP_ON);
	dev->power = 1;
	return 1;
}

static unsigned char get_power(void)
{
	return dev->power;
}

static void set_power(unsigned char state)
{
	dev->power = state;
	if (state)
		set_brightness_level(dev->brightness);
	else {
		protocol->write_byte(FD650_MODE_WRCMD);
		protocol->write_byte(FD650_DISP_STATE_WRCMD | FD650_DISP_OFF);
	}
}

static struct fd628_display *get_display_type(void)
{
	return &dev->dtb_active.display;
}

static unsigned char set_display_type(struct fd628_display *display)
{
	unsigned char ret = 0;
	if (display->type < DISPLAY_TYPE_MAX && display->controller == CONTROLLER_FD650)
	{
		dev->dtb_active.display = *display;
		init();
		ret = 1;
	}

	return ret;
}

static void set_icon(const char *name, unsigned char state)
{
	if (strncmp(name,"colon",5) == 0)
		dev->status_led_mask = state ? (dev->status_led_mask | ledDots[LED_DOT_SEC]) : (dev->status_led_mask & ~ledDots[LED_DOT_SEC]);
}

static size_t read_data(unsigned char *data, size_t length)
{
	unsigned char cmd = FD650_KEY_RDCMD;
	return protocol->read_cmd_data(&cmd, 1, data, length > 1 ? 1 : length) == 0 ? 1 : -1;
}

static size_t write_data(const unsigned char *_data, size_t length)
{
	size_t i;
	struct fd628_dtb_config *dtb = &dev->dtb_active;
	unsigned short *data = (unsigned short *)_data;

	memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
	length = length > ram_size ? ram_grid_count : (length / sizeof(unsigned short));
	for (i = 1; i < length; i++)
		dev->wbuf[dtb->dat_index[i]] = data[i];
	if ((data[0] | dev->status_led_mask) & ledDots[LED_DOT_SEC])
		dev->wbuf[dtb->dat_index[0]] |= 0x80;

	length *= sizeof(unsigned short);
	return fd650_write_data(0, (unsigned char *)dev->wbuf, length) == 0 ? length : 0;
}
