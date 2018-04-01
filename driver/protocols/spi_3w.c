#include <linux/delay.h>
#include <linux/gpio.h>
#include "spi_3w.h"

#define LOW	0
#define HIGH	1

static unsigned char read_data(unsigned char *data, int length);
static unsigned char read_byte(unsigned char *bdata);
static unsigned char write_data(unsigned char *data, int length);
static unsigned char write_byte(unsigned char bdata);

static struct protocol_interface spi_3w_interface = {
	.read_data = read_data,
	.read_byte = read_byte,
	.write_data = write_data,
	.write_byte = write_byte,
};

static unsigned long spi_delay = SPI_DELAY_100KHz;
static int pin_clk = 0;
static int pin_dat = 0;
static int pin_stb = 0;

struct protocol_interface *init_spi_3w(int clk, int dat, int stb, unsigned long _spi_delay)
{
	pin_clk = clk;
	pin_dat = dat;
	pin_stb = stb;
	spi_delay = _spi_delay;
	return &spi_3w_interface;
}

static void start_condition(void)
{
	gpio_direction_output(pin_stb, LOW);
	udelay(spi_delay);
}

static void stop_condition(void)
{
	gpio_direction_output(pin_clk, 1);
	udelay(spi_delay);
	gpio_direction_output(pin_stb, 1);
	gpio_direction_output(pin_dat, 1);
	gpio_direction_input(pin_dat);
	udelay(spi_delay);
}

static unsigned char write_raw_byte(unsigned char data)
{
	unsigned char i = 8;
	gpio_direction_output(pin_clk, LOW);
	while (i--) {
		if (data & 0x01)
			gpio_direction_output(pin_dat, HIGH);
		else
			gpio_direction_output(pin_dat, LOW);
		gpio_direction_output(pin_clk, HIGH);
		udelay(spi_delay);
		gpio_direction_output(pin_clk, LOW);
		udelay(spi_delay);
		data >>= 1;    
	}
	return 0;
}

static unsigned char read_raw_byte(unsigned char *data)
{
	unsigned char i = 8;
	*data = 0;
	gpio_direction_output(pin_dat, HIGH);
	gpio_direction_input(pin_dat);
	while (i--) {
		*data >>= 1;
		gpio_direction_output(pin_clk, HIGH);
		udelay(spi_delay);
		if (gpio_get_value(pin_dat))
			*data |= 0x80;
		gpio_direction_output(pin_clk, LOW);
		udelay(spi_delay);
	}
	return 0;
}

static unsigned char read_data(unsigned char *data, int length)
{
	unsigned char status = 0;
	start_condition();
	if (!status) {
		while (length--) {
			status |= read_raw_byte(data);
			data++;
		}
	}
	stop_condition();
	return status;
}

static unsigned char read_byte(unsigned char *bdata)
{
	return read_data(bdata, 1);
}

static unsigned char write_data(unsigned char *data, int length)
{
	unsigned char status = 0;
	start_condition();
	if (!status) {
		while (length--) {
			status |= write_raw_byte(*data);
			data++;
		}
	}
	stop_condition();
	return status;
}

static unsigned char write_byte(unsigned char bdata)
{
	return write_data(&bdata, 1);
}
