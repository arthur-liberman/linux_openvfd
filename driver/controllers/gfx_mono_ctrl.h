#ifndef __GFXMONOCTRLH__
#define __GFXMONOCTRLH__

#include "controller.h"

struct font {
	unsigned char font_height;
	unsigned char font_width;
	unsigned char font_char_size;
	unsigned char font_offset;
	unsigned char font_char_count;
	const unsigned char *font_bitmaps;
};

struct rect {
	unsigned char x1;
	unsigned char y1;
	unsigned char x2;
	unsigned char y2;
	unsigned char width;
	unsigned char height;
	unsigned char text_width;
	unsigned char text_height;
	unsigned char length;
	const struct font *font;
};

struct specific_gfx_mono_ctrl
{
	unsigned char (*init)(void);
	unsigned char (*set_display_type)(struct vfd_display *display);

	void (*clear)(void);
	void (*set_power)(unsigned char state);
	void (*set_contrast)(unsigned char value);
	unsigned char (*set_xy)(unsigned char x, unsigned char y);
	void (*print_string)(const unsigned char *buffer, const struct rect *rect);

	void (*write_ctrl_command_buf)(const unsigned char *buf, unsigned int length);
	void (*write_ctrl_command)(unsigned char cmd);
	void (*write_ctrl_data_buf)(const unsigned char *buf, unsigned int length);
	void (*write_ctrl_data)(unsigned char data);
};

struct controller_interface *init_gfx_mono_ctrl(struct vfd_dev *_dev, const struct specific_gfx_mono_ctrl *specific_gfx_mono_ctrl);

#endif
