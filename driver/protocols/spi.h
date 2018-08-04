#ifndef __SPIH__
#define __SPIH__

#include "protocol.h"
#include "../openvfd_drv.h"

#define SPI_DELAY_500KHz	1
#define SPI_DELAY_250KHz	2
#define SPI_DELAY_100KHz	5
#define SPI_DELAY_20KHz	25

struct protocol_interface *init_spi_3w(struct vfd_pin clk, struct vfd_pin dat, struct vfd_pin stb, unsigned long _spi_delay);

#endif
