#include <linux/delay.h>
#include <linux/gpio.h>
#include "i2c.h"

#define pr_dbg2(args...) printk(KERN_DEBUG "OpenVFD: " args)
#define LOW	0
#define HIGH	1

static unsigned char i2c_read_cmd_data(const unsigned char *cmd, unsigned int cmd_length, unsigned char *data, unsigned int data_length);
static unsigned char i2c_read_data(unsigned char *data, unsigned int length);
static unsigned char i2c_read_byte(unsigned char *bdata);
static unsigned char i2c_write_cmd_data(const unsigned char *cmd, unsigned int cmd_length, const unsigned char *data, unsigned int data_length);
static unsigned char i2c_write_data(const unsigned char *data, unsigned int length);
static unsigned char i2c_write_byte(unsigned char bdata);

static struct protocol_interface i2c_interface = {
	.read_cmd_data = i2c_read_cmd_data,
	.read_data = i2c_read_data,
	.read_byte = i2c_read_byte,
	.write_cmd_data = i2c_write_cmd_data,
	.write_data = i2c_write_data,
	.write_byte = i2c_write_byte,
};

static void i2c_stop_condition(void);

static unsigned char i2c_address = 0;
static unsigned long i2c_delay = I2C_DELAY_100KHz;
static unsigned char lsb_first = 0;
static unsigned short clk_stretch_timeout = 0;
static int pin_scl = 0;
static int pin_sda = 0;

struct protocol_interface *init_i2c(unsigned char _address, unsigned char _lsb_first, struct vfd_pin _pin_scl, struct vfd_pin _pin_sda, unsigned long _i2c_delay)
{
	struct protocol_interface *i2c_ptr = NULL;
	if (_pin_scl.pin >= 0 && _pin_sda.pin >= 0) {
		i2c_address = _address;
		lsb_first = _lsb_first;
		pin_scl = _pin_scl.pin;
		pin_sda = _pin_sda.pin;
		i2c_delay = _i2c_delay;
		clk_stretch_timeout = 10 * _i2c_delay;
		i2c_stop_condition();
		i2c_ptr = &i2c_interface;
		pr_dbg2("I2C interface intialized\n");
	} else {
		pr_dbg2("I2C interface failed to intialize. Invalid SCL (%d) and/or SDA (%d) pins\n", _pin_scl.pin, _pin_sda.pin);
	}
	return i2c_ptr;
}

static void i2c_start_condition(void)
{
	gpio_direction_output(pin_sda, LOW);
	udelay(i2c_delay);
	gpio_direction_output(pin_scl, LOW);
	udelay(i2c_delay);
}

static void i2c_stop_condition(void)
{
	gpio_direction_input(pin_scl);
	udelay(i2c_delay);
	gpio_direction_input(pin_sda);
	udelay(i2c_delay);
	udelay(i2c_delay);
}

static inline unsigned char i2c_ack(void)
{
	unsigned char ret = 1, scl = 1;
	unsigned short timeout = clk_stretch_timeout;
	gpio_direction_output(pin_scl, LOW);
	gpio_direction_input(pin_sda);
	udelay(i2c_delay);
	gpio_direction_input(pin_scl);
	udelay(i2c_delay);
	do {
		scl = gpio_get_value(pin_scl) ? 1 : 0;
		udelay(1);
	} while (!scl && timeout--);
	ret = gpio_get_value(pin_sda) ? 1 : 0;
	gpio_direction_output(pin_scl, LOW);
	gpio_direction_output(pin_sda, LOW);
	udelay(i2c_delay);
	return ret;
}

static unsigned char i2c_write_raw_byte(unsigned char data)
{
	unsigned char i = 8;
	unsigned char mask = lsb_first ? 0x01 : 0x80;
	gpio_direction_output(pin_scl, LOW);
	while (i--) {
		if (data & mask)
			gpio_direction_input(pin_sda);
		else
			gpio_direction_output(pin_sda, LOW);
		udelay(i2c_delay);
		gpio_direction_input(pin_scl);
		udelay(i2c_delay);
		gpio_direction_output(pin_scl, LOW);
		if (lsb_first)
			data >>= 1;
		else
			data <<= 1;
	}
	return i2c_ack();
}

static unsigned char i2c_read_raw_byte(unsigned char *data)
{
	unsigned char i = 8;
	unsigned char mask = lsb_first ? 0x80 : 0x01;
	*data = 0;
	gpio_direction_input(pin_sda);
	while (i--) {
		if (lsb_first)
			*data >>= 1;
		else
			*data <<= 1;
		gpio_direction_input(pin_scl);
		udelay(i2c_delay);
		if (gpio_get_value(pin_sda))
			*data |= mask;
		gpio_direction_output(pin_scl, LOW);
		udelay(i2c_delay);
	}
	return i2c_ack();
}

static unsigned char i2c_write_address(unsigned char _address, unsigned char rw)
{
	_address <<= 1;
	_address |= rw ? 0x01 : 0x00;
	return i2c_write_raw_byte(_address);
}

static unsigned char i2c_read_cmd_data(const unsigned char *cmd, unsigned int cmd_length, unsigned char *data, unsigned int data_length)
{
	unsigned char status = 0;
	i2c_start_condition();
	if (i2c_address > 0)
		status = i2c_write_address(i2c_address, 1);
	if (cmd) {
		while (!status && cmd_length--)
			status |= i2c_write_raw_byte(*cmd++);
	}
	while (!status && data_length--)
		status |= i2c_read_raw_byte(data++);
	i2c_stop_condition();
	return status;
}

static unsigned char i2c_read_data(unsigned char *data, unsigned int length)
{
	return i2c_read_cmd_data(NULL, 0, data, length);
}

static unsigned char i2c_read_byte(unsigned char *bdata)
{
	return i2c_read_cmd_data(NULL, 0, bdata, 1);
}

static unsigned char i2c_write_cmd_data(const unsigned char *cmd, unsigned int cmd_length, const unsigned char *data, unsigned int data_length)
{
	unsigned char status = 0;
	i2c_start_condition();
	if (i2c_address > 0)
		status = i2c_write_address(i2c_address, 0);
	if (cmd) {
		while (!status && cmd_length--)
			status |= i2c_write_raw_byte(*cmd++);
	}
	while (!status && data_length--)
			status |= i2c_write_raw_byte(*data++);
	i2c_stop_condition();
	return status;
}

static unsigned char i2c_write_data(const unsigned char *data, unsigned int length)
{
	return i2c_write_cmd_data(NULL, 0, data, length);
}

static unsigned char i2c_write_byte(unsigned char bdata)
{
	return i2c_write_cmd_data(NULL, 0, &bdata, 1);
}
