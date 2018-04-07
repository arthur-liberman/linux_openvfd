#ifndef __SSD1306H__
#define __SSD1306H__

#include "controller.h"

struct controller_interface *init_ssd1306(struct fd628_dev *_dev);

#endif
