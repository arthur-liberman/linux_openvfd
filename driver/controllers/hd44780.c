#include <linux/delay.h>
#include "../protocols/i2c.h"
#include "../protocols/spi_3w.h"
#include "hd44780.h"

/* **************************** Define HD44780 Constants ****************************** */
#define HD44780_CLEAR_RAM		0x01	/* Write mode command			*/
#define HD44780_HOME			0x02	/* Increment or decrement address	*/
#define HD44780_ENTRY_MODE		0x04	/* Write mode command			*/
#define HD44780_EM_ID			0x02	/* Increment or decrement address	*/
#define HD44780_EM_S			0x01	/* Accompanies display shift		*/
#define HD44780_DISPLAY_CONTROL		0x08	/* Write mode command			*/
#define HD44780_DC_D			0x04	/* Increment or decrement address	*/
#define HD44780_DC_C			0x02	/* Accompanies display shift		*/
#define HD44780_DC_B			0x01	/* Accompanies display shift		*/
#define HD44780_CURESOR_SHIFT		0x10	/* Write mode command			*/
#define HD44780_CS			0x08	/* FD650 Display On			*/
#define HD44780_RL			0x04	/* FD650 Display Off			*/
#define HD44780_FUNCTION		0x20	/* Set FD650 to work in 7-segment mode	*/
#define HD44780_F_DL			0x10	/* Set FD650 to work in 8-segment mode	*/
#define HD44780_F_N			0x08	/* Base data address			*/
#define HD44780_F_F			0x04	/* Set display brightness command	*/
#define HD44780_CGRA			0x40	/* Set CGRAM Address			*/
#define HD44780_DDRA			0x80	/* Set DDRAM Address			*/
/* ************************************************************************************ */
#define BACKLIGHT			0x08
#define ENABLE				0x04
#define READ				0x02
#define RS				0x01
#define CMD				0x00

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

static struct controller_interface hd47780_interface = {
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
static unsigned char columns = 16;
static unsigned char rows = 2;
static unsigned char backlight = BACKLIGHT;

struct controller_interface *init_hd47780(struct fd628_dev *_dev)
{
	dev = _dev;
	init();
	return &hd47780_interface;
}

static void write_4_bits(unsigned char data) {
	unsigned char buffer[3] = { data, (unsigned char)(data | ENABLE), data };
	protocol->write_data(buffer, 3);
}

static void write_lcd(unsigned char data, unsigned char mode) {
	unsigned char lo = (data & 0x0F) << 4;
	unsigned char hi = (data & 0xF0);
	mode |= backlight;
	write_4_bits(hi | mode);
	write_4_bits(lo | mode);
}

static void write_buf_lcd(const unsigned char *buf, unsigned int length)
{
	while (length--) {
		write_lcd(*buf, RS);
		buf++;
	}
}

static unsigned char read_4_bits(unsigned char mode) {
	unsigned char data[2] = { (unsigned char)(mode | 0xF0), (unsigned char)(mode | 0xF0 | ENABLE) };
	protocol->write_data(data, 2);
	protocol->read_byte(data + 1);
	protocol->write_data(&mode, 1);
	return data[1];
}

static unsigned char read_lcd(unsigned char mode) {
	unsigned char lo;
	unsigned char hi;
	mode &= ~ENABLE;
	mode |= backlight | READ;
	hi = read_4_bits(mode);
	lo = read_4_bits(mode);
	return ((hi & 0xF0) | (lo >> 4));
}

static void init(void)
{
	unsigned char cmd = 0;
	protocol = init_i2c(dev->dtb_active.display.reserved & 0x7F, dev->clk_pin, dev->dat_pin, I2C_DELAY_500KHz);
	columns = (dev->dtb_active.display.type & 0x1F) << 1;
	rows = (dev->dtb_active.display.type >> 5) & 0x07;
	rows++;

	write_4_bits(0x03 << 4);
	usleep_range(4300, 5000);
	write_4_bits(0x03 << 4);
	udelay(150);
	write_4_bits(0x03 << 4);
	udelay(150);
	write_4_bits(0x02 << 4);
	udelay(150);
	cmd = HD44780_FUNCTION;
	if (rows > 1)
		cmd |= HD44780_F_N;
	write_lcd(cmd, CMD);
	udelay(150);
	write_lcd(HD44780_DISPLAY_CONTROL, CMD);
	write_lcd(HD44780_CLEAR_RAM, CMD);
	usleep_range(1600, 2000);
	write_lcd(HD44780_ENTRY_MODE | HD44780_EM_ID, CMD);
	write_lcd(HD44780_DISPLAY_CONTROL | HD44780_DC_D, CMD);

	set_brightness_level(dev->brightness);
}

static unsigned short get_brightness_levels_count(void)
{
	return 2;
}

static unsigned short get_brightness_level(void)
{
	return dev->brightness;
}

static unsigned char set_brightness_level(unsigned short level)
{
	dev->brightness = level;
	dev->power = 1;
	backlight = dev->power && dev->brightness > 0 ? BACKLIGHT : 0;
	write_lcd(HD44780_DISPLAY_CONTROL | HD44780_DC_D, CMD);
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
		backlight = 0;
		write_lcd(HD44780_DISPLAY_CONTROL, CMD);
	}
}

static struct fd628_display *get_display_type(void)
{
	return &dev->dtb_active.display;
}

static unsigned char set_display_type(struct fd628_display *display)
{
	unsigned char ret = 0;
	if (display->controller == CONTROLLER_HD44780)
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
	size_t count = length;
	write_lcd(HD44780_HOME, CMD);
	while (count--) {
		*data = read_lcd(RS);
		data++;
	}
	return length;
}

static size_t write_data(const unsigned char *data, size_t length)
{
	write_lcd(HD44780_HOME, CMD);
	usleep_range(1600, 2000);
	if (length > 0) {
		if (length > 2)
			write_lcd(data[2], RS);
		if (length > 4)
			write_lcd(data[4], RS);
		if ((data[0] | dev->status_led_mask) & ledDots[LED_DOT_SEC])
			write_lcd(':', RS);
		else
			write_lcd(' ', RS);
		if (length > 6)
			write_lcd(data[6], RS);
		if (length > 8)
			write_lcd(data[8], RS);
	}

	return length;
}
