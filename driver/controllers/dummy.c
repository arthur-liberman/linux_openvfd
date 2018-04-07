#include <linux/semaphore.h>
#include "../le_vfd_drv.h"
#include "dummy.h"

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

static struct controller_interface dummy_interface = {
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

struct controller_interface *init_dummy(struct fd628_dev *_dev)
{
	dev = _dev;
	return &dummy_interface;
}

static void init(void)
{
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
}

static struct fd628_display *get_display_type(void)
{
	return &dev->dtb_active.display;
}

static unsigned char set_display_type(struct fd628_display *display)
{
	dev->dtb_active.display = *display;
	return 1;
}

static void set_icon(const char *name, unsigned char state)
{
}

static size_t read_data(unsigned char *data, size_t length)
{
	return 0;
}

static size_t write_data(const unsigned char *_data, size_t length)
{
	return length;
}
