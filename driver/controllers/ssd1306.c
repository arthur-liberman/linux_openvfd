#include <linux/semaphore.h>
#include "../le_vfd_drv.h"
#include "ssd1306.h"

static unsigned char ssd1306_init(void);
static unsigned short ssd1306_get_brightness_levels_count(void);
static unsigned short ssd1306_get_brightness_level(void);
static unsigned char ssd1306_set_brightness_level(unsigned short level);
static unsigned char ssd1306_get_power(void);
static void ssd1306_set_power(unsigned char state);
static struct fd628_display *ssd1306_get_display_type(void);
static unsigned char ssd1306_set_display_type(struct fd628_display *display);
static void ssd1306_set_icon(const char *name, unsigned char state);
static size_t ssd1306_read_data(unsigned char *data, size_t length);
static size_t ssd1306_write_data(const unsigned char *data, size_t length);
static size_t ssd1306_write_display_data(const struct fd628_display_data *data);

static struct controller_interface ssd1306_interface = {
	.init = ssd1306_init,
	.get_brightness_levels_count = ssd1306_get_brightness_levels_count,
	.get_brightness_level = ssd1306_get_brightness_level,
	.set_brightness_level = ssd1306_set_brightness_level,
	.get_power = ssd1306_get_power,
	.set_power = ssd1306_set_power,
	.get_display_type = ssd1306_get_display_type,
	.set_display_type = ssd1306_set_display_type,
	.set_icon = ssd1306_set_icon,
	.read_data = ssd1306_read_data,
	.write_data = ssd1306_write_data,
	.write_display_data = ssd1306_write_display_data,
};

static struct fd628_dev *dev = NULL;

struct controller_interface *init_ssd1306(struct fd628_dev *_dev)
{
	dev = _dev;
	return &ssd1306_interface;
}

static unsigned char ssd1306_init(void)
{
	return 1;
}

static unsigned short ssd1306_get_brightness_levels_count(void)
{
	return 8;
}

static unsigned short ssd1306_get_brightness_level(void)
{
	return dev->brightness;
}

static unsigned char ssd1306_set_brightness_level(unsigned short level)
{
	dev->brightness = level & 0x7;
	dev->power = 1;
	return 1;
}

static unsigned char ssd1306_get_power(void)
{
	return dev->power;
}

static void ssd1306_set_power(unsigned char state)
{
	dev->power = state;
}

static struct fd628_display *ssd1306_get_display_type(void)
{
	return &dev->dtb_active.display;
}

static unsigned char ssd1306_set_display_type(struct fd628_display *display)
{
	unsigned char ret = 0;
	if (display->controller == CONTROLLER_SSD1306)
	{
		dev->dtb_active.display = *display;
		ssd1306_init();
		ret = 1;
	}

	return ret;
}

static void ssd1306_set_icon(const char *name, unsigned char state)
{
}

static size_t ssd1306_read_data(unsigned char *data, size_t length)
{
	return 0;
}

static size_t ssd1306_write_data(const unsigned char *_data, size_t length)
{
	return length;
}

static size_t ssd1306_write_display_data(const struct fd628_display_data *data)
{
	return sizeof(*data);
}
