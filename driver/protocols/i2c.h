#ifndef __I2CH__
#define __I2CH__

#include "protocol.h"

#define I2C_DELAY_100KHz 5
#define I2C_DELAY_400KHz 2

struct protocol_interface *init_i2c(unsigned char address, int pin_scl, int pin_sda, unsigned long i2c_delay);

#endif
