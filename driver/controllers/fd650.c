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

static unsigned char fd650_init(void);
static unsigned short fd650_get_brightness_levels_count(void);
static unsigned short fd650_get_brightness_level(void);
static unsigned char fd650_set_brightness_level(unsigned short level);
static unsigned char fd650_get_power(void);
static void fd650_set_power(unsigned char state);
static struct vfd_display *fd650_get_display_type(void);
static unsigned char fd650_set_display_type(struct vfd_display *display);
static void fd650_set_icon(const char *name, unsigned char state);
static size_t fd650_read_data(unsigned char *data, size_t length);
static size_t fd650_write_data(const unsigned char *data, size_t length);
static size_t fd650_write_display_data(const struct vfd_display_data *data);

static struct controller_interface fd650_interface = {
	.init = fd650_init,
	.get_brightness_levels_count = fd650_get_brightness_levels_count,
	.get_brightness_level = fd650_get_brightness_level,
	.set_brightness_level = fd650_set_brightness_level,
	.get_power = fd650_get_power,
	.set_power = fd650_set_power,
	.get_display_type = fd650_get_display_type,
	.set_display_type = fd650_set_display_type,
	.set_icon = fd650_set_icon,
	.read_data = fd650_read_data,
	.write_data = fd650_write_data,
	.write_display_data = fd650_write_display_data,
};

size_t seg7_write_display_data(const struct vfd_display_data *data, unsigned short *raw_wdata, size_t sz);

static struct vfd_dev *dev = NULL;
static struct protocol_interface *protocol = NULL;
static unsigned char ram_grid_count = 4;
static unsigned char ram_size = 8;
extern const led_bitmap *ledCodes;

struct controller_interface *init_fd650(struct vfd_dev *_dev)
{
	dev = _dev;
	return &fd650_interface;
}

static size_t fd650_write_data_real(unsigned char address, const unsigned char *data, size_t length)
{
	unsigned char cmd = FD650_BASE_ADDR | (address & 0x07), i;
	if (length + address > ram_size)
		return (-1);

	protocol->write_byte(FD650_MODE_WRCMD);
	for (i = 0; i < length; i += 2, cmd += 2)
		protocol->write_cmd_data(&cmd, 1, data + i, 1);
	return (0);
}

static unsigned char fd650_init(void)
{
	protocol = init_i2c(0, I2C_MSB_FIRST, dev->clk_pin, dev->dat_pin, I2C_DELAY_100KHz);
	memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
	fd650_set_brightness_level(dev->brightness);
	switch(dev->dtb_active.display.type) {
		case DISPLAY_TYPE_5D_7S_T95:
			ledCodes = LED_decode_tab1;
			break;
		default:
			ledCodes = LED_decode_tab2;
			break;
	}
	return 1;
}

static unsigned short fd650_get_brightness_levels_count(void)
{
	return 8;
}

static unsigned short fd650_get_brightness_level(void)
{
	return dev->brightness;
}

static unsigned char fd650_set_brightness_level(unsigned short level)
{
	dev->brightness = level & 0x7;
	protocol->write_byte(FD650_MODE_WRCMD);
	protocol->write_byte(FD650_DISP_STATE_WRCMD | (dev->brightness << 4) | FD650_DISP_ON);
	dev->power = 1;
	return 1;
}

static unsigned char fd650_get_power(void)
{
	return dev->power;
}

static void fd650_set_power(unsigned char state)
{
	dev->power = state;
	if (state)
		fd650_set_brightness_level(dev->brightness);
	else {
		protocol->write_byte(FD650_MODE_WRCMD);
		protocol->write_byte(FD650_DISP_STATE_WRCMD | FD650_DISP_OFF);
	}
}

static struct vfd_display *fd650_get_display_type(void)
{
	return &dev->dtb_active.display;
}

static unsigned char fd650_set_display_type(struct vfd_display *display)
{
	unsigned char ret = 0;
	if (display->type < DISPLAY_TYPE_MAX && display->controller == CONTROLLER_FD650)
	{
		dev->dtb_active.display = *display;
		fd650_init();
		ret = 1;
	}

	return ret;
}

static void fd650_set_icon(const char *name, unsigned char state)
{
	if (strncmp(name,"colon",5) == 0)
		dev->status_led_mask = state ? (dev->status_led_mask | ledDots[LED_DOT_SEC]) : (dev->status_led_mask & ~ledDots[LED_DOT_SEC]);
}

static size_t fd650_read_data(unsigned char *data, size_t length)
{
	unsigned char cmd = FD650_KEY_RDCMD;
	return protocol->read_cmd_data(&cmd, 1, data, length > 1 ? 1 : length) == 0 ? 1 : -1;
}

static size_t fd650_write_data(const unsigned char *_data, size_t length)
{
	size_t i;
	struct vfd_dtb_config *dtb = &dev->dtb_active;
	unsigned short *data = (unsigned short *)_data;

	memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
	length = length > ram_size ? ram_grid_count : (length / sizeof(unsigned short));
	for (i = 1; i <= length; i++)
		dev->wbuf[dtb->dat_index[i]] = data[i];
	if ((data[0] | dev->status_led_mask) & ledDots[LED_DOT_SEC])
		dev->wbuf[dtb->dat_index[0]] |= 0x80;

	length *= sizeof(unsigned short);
	return fd650_write_data_real(0, (unsigned char *)dev->wbuf, length) == 0 ? length : 0;
}

static size_t fd650_write_display_data(const struct vfd_display_data *data)
{
	unsigned short wdata[7];
	size_t status = seg7_write_display_data(data, wdata, sizeof(wdata));
	if (status && !fd650_write_data((unsigned char*)wdata, 5*sizeof(wdata[0])))
		status = 0;
	return status;
}
