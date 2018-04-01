#ifndef __SPI_3WH__
#define __SPI_3WH__

#include "protocol.h"

#define SPI_DELAY_500KHz 1
#define SPI_DELAY_250KHz 2
#define SPI_DELAY_100KHz 5

struct protocol_interface *init_spi_3w(int clk, int dat, int stb, unsigned long _spi_delay);

#endif
