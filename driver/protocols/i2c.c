#include <linux/delay.h>
#include <linux/gpio.h>
#include "i2c.h"

#define pr_dbg2(args...) printk(KERN_DEBUG "FD628: " args)
#define LOW	0
#define HIGH	1

static unsigned char read_cmd_data(const unsigned char *cmd, unsigned int cmd_length, unsigned char *data, unsigned int data_length);
static unsigned char read_data(unsigned char *data, unsigned int length);
static unsigned char read_byte(unsigned char *bdata);
static unsigned char write_cmd_data(const unsigned char *cmd, unsigned int cmd_length, const unsigned char *data, unsigned int data_length);
static unsigned char write_data(const unsigned char *data, unsigned int length);
static unsigned char write_byte(unsigned char bdata);

static struct protocol_interface i2c_interface = {
	.read_cmd_data = read_cmd_data,
	.read_data = read_data,
	.read_byte = read_byte,
	.write_cmd_data = write_cmd_data,
	.write_data = write_data,
	.write_byte = write_byte,
};

static void stop_condition(void);

static unsigned char i2c_address = 0;
static unsigned long i2c_delay = I2C_DELAY_100KHz;
static int pin_scl = 0;
static int pin_sda = 0;

struct protocol_interface *init_i2c(unsigned char _address, int _pin_scl, int _pin_sda, unsigned long _i2c_delay)
{
	i2c_address = _address;
	pin_scl = _pin_scl;
	pin_sda = _pin_sda;
	i2c_delay = _i2c_delay;
	stop_condition();
	pr_dbg2("I2C interface intialized\n");
	return &i2c_interface;
}

static void start_condition(void)
{
	gpio_direction_output(pin_sda, LOW);
	udelay(i2c_delay);
	gpio_direction_output(pin_scl, LOW);
	udelay(i2c_delay);
}

static void stop_condition(void)
{
	gpio_direction_input(pin_scl);
	udelay(i2c_delay);
	gpio_direction_input(pin_sda);
	udelay(i2c_delay);
}

static unsigned char ack(void)
{
	unsigned char ret = 1;
	gpio_direction_output(pin_scl, LOW);
	gpio_direction_input(pin_sda);
	udelay(i2c_delay);
	gpio_direction_input(pin_scl);
	udelay(i2c_delay);
	ret = gpio_get_value(pin_sda) ? 1 : 0;
	gpio_direction_output(pin_scl, LOW);
	udelay(i2c_delay);
	return ret;
}

static unsigned char write_raw_byte(unsigned char data)
{
	unsigned char i = 8;
	gpio_direction_output(pin_scl, LOW);
	while (i--) {
		if (data & 0x80)
			gpio_direction_input(pin_sda);
		else
			gpio_direction_output(pin_sda, LOW);
		gpio_direction_input(pin_scl);
		udelay(i2c_delay);
		gpio_direction_output(pin_scl, LOW);
		udelay(i2c_delay);
		data <<= 1;    
	}
	return ack();
}

static unsigned char read_raw_byte(unsigned char *data)
{
	unsigned char i = 8;
	*data = 0;
	gpio_direction_input(pin_sda);
	while (i--) {
		*data <<= 1;
		gpio_direction_input(pin_scl);
		udelay(i2c_delay);
		if (gpio_get_value(pin_sda))
			*data |= 0x01;
		gpio_direction_output(pin_scl, LOW);
		udelay(i2c_delay);
	}
	return ack();
}

static unsigned char write_address(unsigned char _address, unsigned char rw)
{
	_address <<= 1;
	_address |= rw ? 0x01 : 0x00;
	return write_raw_byte(_address);
}

static unsigned char read_cmd_data(const unsigned char *cmd, unsigned int cmd_length, unsigned char *data, unsigned int data_length)
{
	unsigned char status = 0;
	start_condition();
	if (i2c_address > 0)
		status = write_address(i2c_address, 1);
	if (!status && cmd) {
		while (cmd_length--) {
			status |= write_raw_byte(*cmd);
			cmd++;
		}
	}
	if (!status) {
		while (data_length--) {
			status |= read_raw_byte(data);
			data++;
		}
	}
	stop_condition();
	return status;
}

static unsigned char read_data(unsigned char *data, unsigned int length)
{
	return read_cmd_data(NULL, 0, data, length);
}

static unsigned char read_byte(unsigned char *bdata)
{
	return read_cmd_data(NULL, 0, bdata, 1);
}

static unsigned char write_cmd_data(const unsigned char *cmd, unsigned int cmd_length, const unsigned char *data, unsigned int data_length)
{
	unsigned char status = 0;
	start_condition();
	if (i2c_address > 0)
		status = write_address(i2c_address, 0);
	if (!status && cmd) {
		while (cmd_length--) {
			status |= write_raw_byte(*cmd);
			cmd++;
		}
	}
	if (!status) {
		while (data_length--) {
			status |= write_raw_byte(*data);
			data++;
		}
	}
	stop_condition();
	return status;
}

static unsigned char write_data(const unsigned char *data, unsigned int length)
{
	return write_cmd_data(NULL, 0, data, length);
}

static unsigned char write_byte(unsigned char bdata)
{
	return write_cmd_data(NULL, 0, &bdata, 1);
}
