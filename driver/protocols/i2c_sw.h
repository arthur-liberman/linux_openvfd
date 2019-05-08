#ifndef __I2CH__
#define __I2CH__

#include "protocol.h"

#define I2C_DELAY_500KHz	1
#define I2C_DELAY_250KHz	2
#define I2C_DELAY_100KHz	5
#define I2C_DELAY_20KHz	25

struct protocol_interface *init_i2c(unsigned short _address, unsigned char _lsb_first, struct vfd_pin pin_scl, struct vfd_pin pin_sda, unsigned long i2c_delay);

#endif
