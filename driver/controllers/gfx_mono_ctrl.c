#include <linux/semaphore.h>
#include "../protocols/i2c.h"
#include "ssd1306.h"
#include "fonts/Grotesk16x32.h"
#include "fonts/Grotesk24x48.h"
#include "fonts/Retro8x16.h"
#include "fonts/icons16x16.h"
#include "fonts/icons32x32.h"

static unsigned char ssd1306_init(void);
static unsigned char sh1106_init(void);
static unsigned short ssd1306_get_brightness_levels_count(void);
static unsigned short ssd1306_get_brightness_level(void);
static unsigned char ssd1306_set_brightness_level(unsigned short level);
static unsigned char ssd1306_get_power(void);
static void ssd1306_set_power(unsigned char state);
static struct vfd_display *ssd1306_get_display_type(void);
static unsigned char ssd1306_set_display_type(struct vfd_display *display);
static void ssd1306_set_icon(const char *name, unsigned char state);
static size_t ssd1306_read_data(unsigned char *data, size_t length);
static size_t ssd1306_write_data(const unsigned char *data, size_t length);
static size_t ssd1306_write_display_data(const struct vfd_display_data *data);

static struct controller_interface ssd1306_interface = {
	.init = ssd1306_init,
	.get_brightness_levels_count = ssd1306_get_brightness_levels_count,
	.get_brightness_level = ssd1306_get_brightness_level,
	.set_brightness_level = ssd1306_set_brightness_level,
	.get_power = ssd1306_get_power,
	.set_power = ssd1306_set_power,
	.get_display_type = ssd1306_get_display_type,
	.set_display_type = ssd1306_set_display_type,
	.set_icon = ssd1306_set_icon,
	.read_data = ssd1306_read_data,
	.write_data = ssd1306_write_data,
	.write_display_data = ssd1306_write_display_data,
};

#define MAX_INDICATORS	4

enum display_modes {
	DISPLAY_MODE_128x32,
	DISPLAY_MODE_96x32,
	DISPLAY_MODE_80x32,
	DISPLAY_MODE_80x48,
	DISPLAY_MODE_64x48,
	DISPLAY_MODE_128x64,
	DISPLAY_MODE_96x64,
	DISPLAY_MODE_80x64,
	DISPLAY_MODE_64x64,
};

enum indicator_icons {
	INDICATOR_ICON_NONE = 1,
	INDICATOR_ICON_CHANNEL,
	INDICATOR_ICON_TEMP,
	INDICATOR_ICON_CALENDAR,
	INDICATOR_ICON_MEDIA,
	INDICATOR_ICON_TV,
	INDICATOR_ICON_ETH,
	INDICATOR_ICON_WIFI,
	INDICATOR_ICON_PLAY,
	INDICATOR_ICON_PAUSE,
	INDICATOR_ICON_USB,
	INDICATOR_ICON_SD,
	INDICATOR_ICON_BT,
	INDICATOR_ICON_APPS,
	INDICATOR_ICON_SETUP,
};

struct ssd1306_display {
	unsigned char columns				: 3;
	unsigned char banks				: 3;
	unsigned char offset				: 2;

	unsigned char address				: 7;
	unsigned char reserved1				: 1;

	unsigned char flags_secs			: 1;
	unsigned char flags_invert			: 1;
	unsigned char flags_transpose			: 1;
	unsigned char flags_rotate			: 1;
	unsigned char flags_ext_vcc			: 1;
	unsigned char flags_alt_com_conf		: 1;
	unsigned char flags_low_freq			: 1;
	unsigned char reserved2				: 1;

	unsigned char controller;
};

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

struct indicators {
	unsigned int usb	: 1;
	unsigned int sd		: 1;
	unsigned int play	: 1;
	unsigned int pause	: 1;
	unsigned int eth	: 1;
	unsigned int wifi	: 1;
	unsigned int bt		: 1;
	unsigned int apps	: 1;
	unsigned int setup	: 1;
	unsigned int reserved	: 23;
};

static unsigned char set_xy(unsigned char x, unsigned char y);
static void clear_ssd1306(void);
static void clear_sh1106(void);

static void init_font(struct font *font_struct, const unsigned char *font_bitmaps);
static void init_rect(struct rect *rect, const struct font *font, const char *str, unsigned char x, unsigned char y, unsigned char transposed);
static unsigned char print_icon(unsigned char ch);
static void print_indicator(unsigned char ch, unsigned char state, unsigned char index);
static void print_clock(const struct vfd_display_data *data, unsigned char print_seconds);
static void print_channel(const struct vfd_display_data *data);
static void print_playback_time(const struct vfd_display_data *data);
static void print_title(const struct vfd_display_data *data);
static void print_date(const struct vfd_display_data *data);
static void print_temperature(const struct vfd_display_data *data);

static struct vfd_dev *dev = NULL;
static struct protocol_interface *protocol = NULL;
static unsigned char columns = 128;
static unsigned char rows = 32;
static unsigned char banks = 32 / 8;
static unsigned char col_offset = 0;
static unsigned char show_colon = 1;
static unsigned char show_icons = 1;
static enum display_modes display_mode = DISPLAY_MODE_128x32;
static unsigned char icon_x_offset = 0;
static unsigned char indicators_on_screen[MAX_INDICATORS] = { 0 };
static unsigned char ram_buffer[1024] = { 0 };
static const unsigned char ram_buffer_blank[1024] = { 0 };
static struct vfd_display_data old_data;
static struct font font_text = { 0 };
static struct font font_icons = { 0 };
static struct font font_indicators = { 0 };
static struct font font_small_text = { 0 };
static struct indicators indicators = { 0 };
static struct ssd1306_display ssd1306_display;

static void (*clear)(void);

struct controller_interface *init_ssd1306(struct vfd_dev *_dev)
{
	dev = _dev;
	memcpy(&ssd1306_display, &dev->dtb_active.display, sizeof(ssd1306_display));
	columns = (ssd1306_display.columns + 1) * 16;
	banks = ssd1306_display.banks + 1;
	rows = banks * 8;
	col_offset = ssd1306_display.offset << 1;
	memset(&old_data, 0, sizeof(old_data));
	switch (ssd1306_display.controller) {
	case CONTROLLER_SH1106:
		clear = clear_sh1106;
		ssd1306_interface.init = sh1106_init;
		break;
	case CONTROLLER_SSD1306:
	default:
		clear = clear_ssd1306;
		ssd1306_interface.init = ssd1306_init;
		break;
	}
	return &ssd1306_interface;
}

static void write_oled_buf(unsigned char dc, const unsigned char *buf, unsigned int length)
{
	protocol->write_cmd_data(&dc, 1, buf, length);
}

static void write_oled_byte(unsigned char dc, unsigned char data)
{
	write_oled_buf(dc, &data, 1);
}

static void write_oled_command_buf(const unsigned char *buf, unsigned int length)
{
	write_oled_buf(0x00, buf, length);
}

static void write_oled_command(unsigned char cmd)
{
	write_oled_buf(0x00, &cmd, 1);
}

static void write_oled_data_buf(const unsigned char *buf, unsigned int length)
{
	write_oled_buf(0x40, buf, length);
}

static void write_oled_data(unsigned char data)
{
	write_oled_buf(0x40, &data, 1);
}

static void clear_ssd1306(void)
{
	unsigned char cmd_buf[] = { 0x21, col_offset, col_offset + columns - 1, 0x22, 0x00, banks - 1, 0xAE };
	write_oled_command_buf(cmd_buf, sizeof(cmd_buf));
	write_oled_data_buf(ram_buffer_blank, min((size_t)(columns * banks), sizeof(ram_buffer_blank)));
	write_oled_command(0xAF);
}

static void clear_sh1106(void)
{
	write_oled_command(0xAE);
	for (unsigned char i = 0; i < banks; i++) {
		set_xy(0, i);
		write_oled_data_buf(ram_buffer_blank, columns);
	}
	write_oled_command(0xAF);
}

static void set_power(unsigned char state)
{
	if (state)
		write_oled_command(0xAF); // Set display On
	else
		write_oled_command(0xAE); // Set display OFF
}

static void set_contrast(unsigned char value)
{
	unsigned char cmd_buf[] = { 0x81, ++value };
	write_oled_command_buf(cmd_buf, sizeof(cmd_buf));
}

static unsigned char set_xy(unsigned char x, unsigned char y)
{
	unsigned char ret = 0;
	x += col_offset;
	if (x < columns + col_offset || y < rows) {
		unsigned char cmd_buf[] = { 0xB0 | (y & 0xF), x & 0xF, 0x10 | (x >> 4) };
		write_oled_command_buf(cmd_buf, sizeof(cmd_buf));
		ret = 1;
	}

	return ret;
}

static void print_char(char ch, const struct font *font_struct, unsigned char x, unsigned char y)
{
	unsigned short offset = 0, i;
	if (x >= columns || y >= rows || ch < font_struct->font_offset || ch >= font_struct->font_offset + font_struct->font_char_count)
		return;

	ch -= font_struct->font_offset;
	offset = ch * font_struct->font_char_size;
	offset += 4;
	for (i = 0; i < font_struct->font_height; i++) {
		set_xy(x, y + i);
		write_oled_data_buf(&font_struct->font_bitmaps[offset], font_struct->font_width);
		offset += font_struct->font_width;
	}
}

extern void transpose8rS64(unsigned char* A, unsigned char* B);

#define SSD1306_PRINT_DEBUG 0

#if SSD1306_PRINT_DEBUG
unsigned char print_buffer_cnt = 30;
char txt_buf[20480] = { 0 };
static void print_buffer(unsigned char *buf, const struct rect *rect, unsigned char rotate)
{
	unsigned char height = rotate ? rect->height : rect->width / 8;
	unsigned char width = rotate ? rect->width : rect->height * 8;
	char *t = txt_buf;
	if (!print_buffer_cnt)
		return;
	print_buffer_cnt--;
	if (rotate)
		pr_dbg2("Transposed Buffer:\n");
	else
		pr_dbg2("Non-Transposed Buffer:\n");
	for (int j = 0; j < height; j++) {
		for (int k = 0; k < 8; k++) {
			t = txt_buf;
			for (int i = 0; i < width; i++) {
				if (i % 8 == 0)
					*t++ = ' ';
				*t++ = (buf[j * width + i] & (1 << k)) ? '#' : ' ';
			}
			*t++ = '\n';
			*t++ = '\0';
			printk(KERN_DEBUG "%s", txt_buf);
		}
		printk(KERN_DEBUG "\n");
	}

	printk(KERN_DEBUG "\n\n");
}
#else
static void print_buffer(unsigned char *buf, const struct rect *rect, unsigned char rotate) { }
#endif

unsigned char t_buf[sizeof(ram_buffer)] = { 0 };
static void transpose_buffer(unsigned char *buffer, const struct rect *rect)
{
	unsigned short i;
	if (!rect->width || !rect->height)
		return;

	print_buffer(buffer, rect, 0);
	for (i = 0; i < (rect->height * rect->width) / 8; i++) {
		int d = (rect->height - 1 - i % rect->height) * (rect->width / 8) + (i / rect->height);
		transpose8rS64(&buffer[i * 8], &t_buf[d * 8]);
	}
	memcpy(buffer, t_buf, rect->height * rect->width);
	print_buffer(buffer, rect, 1);
}

static void print_string(const char *str, const struct font *font_struct, unsigned char x, unsigned char y)
{
	unsigned char ch = 0;
	unsigned short soffset = 0, doffset = 0, i, j, k;
	struct rect rect;
	unsigned char rect_width = 0, text_width = 0;
	init_rect(&rect, font_struct, str, x, y, ssd1306_display.flags_transpose);
	if (rect.length == 0)
		return;

	if (ssd1306_display.flags_transpose) {
		rect_width = rect.height * 8;
		text_width = rect.text_height;
	} else {
		rect_width = rect.width;
		text_width = rect.text_width;
	}
	for (k = 0; k < rect.length; k += text_width) {
		for (i = 0; i < font_struct->font_height; i++) {
			for (j = 0; j < text_width; j++) {
				doffset = k + j;
				ch = doffset < rect.length ? str[doffset] : ' ';
				if (ch < font_struct->font_offset || ch >= font_struct->font_offset + font_struct->font_char_count)
					ch = ' ';
				ch -= font_struct->font_offset;
				soffset = ch * font_struct->font_char_size + i * font_struct->font_width;
				soffset += 4;
				memcpy(&ram_buffer[i * rect_width + (k * font_struct->font_height + j) * font_struct->font_width], &font_struct->font_bitmaps[soffset], font_struct->font_width);
			}
		}
	}

	rect.length = rect.text_height * rect.text_width;
	if (ssd1306_display.flags_transpose)
		transpose_buffer(ram_buffer, &rect);
	if (ssd1306_display.controller == CONTROLLER_SH1106) {
		for (i = 0; i < rect.height; i++) {
			set_xy(rect.x1, rect.y1 + i);
			write_oled_data_buf(ram_buffer + (i * rect.width), rect.width);
		}
	} else {
		unsigned char cmd_set_addr_range[] = { 0x21, rect.x1 + col_offset, rect.x2 + col_offset, 0x22, rect.y1, rect.y2 };
		unsigned char cmd_reset_addr_range[] = { 0x21, col_offset, col_offset + columns - 1, 0x22, 0x00, banks - 1 };
		write_oled_command_buf(cmd_set_addr_range, sizeof(cmd_set_addr_range));
		write_oled_data_buf(ram_buffer, rect.length * font_struct->font_char_size);
		write_oled_command_buf(cmd_reset_addr_range, sizeof(cmd_reset_addr_range));
	}
}

static void setup_fonts(void)
{
	init_font(&font_indicators, icons16x16);
	init_font(&font_small_text, Retro8x16);
	switch (banks) {
	case 6:
		init_font(&font_text, Grotesk16x32);
		init_font(&font_icons, icons16x16);
		if (columns >= 80) {
			display_mode = DISPLAY_MODE_80x48;
		} else {
			show_colon = 0;
			display_mode = DISPLAY_MODE_64x48;
		}
		break;
	case 8:
		if (columns >= 96) {
			init_font(&font_text, Grotesk24x48);
			init_font(&font_icons, icons16x16);
			if (columns >= 120) {
				display_mode = DISPLAY_MODE_128x64;
			} else {
				show_colon = 0;
				display_mode = DISPLAY_MODE_96x64;
			}
		} else {
			init_font(&font_text, Grotesk16x32);
			init_font(&font_icons, icons32x32);
			if (columns >= 80) {
				display_mode = DISPLAY_MODE_80x64;
			} else {
				show_colon = 0;
				display_mode = DISPLAY_MODE_64x64;
			}
		}
		break;
	case 4:
	default:
		init_font(&font_text, Grotesk16x32);
		if (columns >= 120) {
			display_mode = DISPLAY_MODE_128x32;
			init_font(&font_icons, icons32x32);
		} else if (columns >= 96) {
			display_mode = DISPLAY_MODE_96x32;
			init_font(&font_icons, icons16x16);
		} else if (columns >= 80) {
			display_mode = DISPLAY_MODE_80x32;
			show_icons = 0;
		} else {
			show_colon = 0;
			show_icons = 0;
		}
		break;
	}
}

static unsigned char sh1106_init(void)
{
	unsigned char cmd_buf[] = {
		0xAE, // [00] Set display OFF

		0xA0, // [01] Set Segment Re-Map

		0xDA, // [02] Set COM Hardware Configuration
		0x02, // [03] COM Hardware Configuration

		0xC0, // [04] Set Com Output Scan Direction

		0xA8, // [05] Set Multiplex Ratio
		0x3F, // [06] Multiplex Ratio for 128x64 (64-1)

		0xD5, // [07] Set Display Clock Divide Ratio / OSC Frequency
		0x80, // [08] Display Clock Divide Ratio / OSC Frequency

		0xDB, // [09] Set VCOMH Deselect Level
		0x35, // [10] VCOMH Deselect Level

		0x81, // [11] Set Contrast
		0x8F, // [12] Contrast

		0x30, // [13] Set Vpp

		0xAD, // [14] Set DC-DC
		0x8A, // [15] DC-DC ON/OFF

		0x40, // [16] Set Display Start Line

		0xA4, // [17] Set all pixels OFF
		0xA6, // [18] Set display not inverted
		0xAF, // [19] Set display On
	};

	protocol = init_i2c(ssd1306_display.address, I2C_MSB_FIRST, dev->clk_pin, dev->dat_pin, ssd1306_display.flags_low_freq ? I2C_DELAY_100KHz : I2C_DELAY_500KHz);
	if (!protocol)
		return 0;

	cmd_buf[1] |= ssd1306_display.flags_rotate ? 0x01 : 0x00;		// [01] Set Segment Re-Map
	cmd_buf[3] |= ssd1306_display.flags_alt_com_conf ? 0x10 : 0x00;		// [03] COM Hardware Configuration
	cmd_buf[4] |= ssd1306_display.flags_rotate ? 0x08 : 0x00;		// [04] Set Com Output Scan Direction
	cmd_buf[6] = max(min(rows-1, 63), 15);					// [06] Multiplex Ratio for 128 x rows (rows-1)
	cmd_buf[12] = (dev->brightness * 36) + 1;				// [12] Contrast
	cmd_buf[15] |= ssd1306_display.flags_ext_vcc ? 0x00 : 0x01;		// [15] DC-DC ON/OFF
	cmd_buf[18] |= ssd1306_display.flags_invert ? 0x01 : 0x00;		// [18] Set display not inverted
	write_oled_command_buf(cmd_buf, sizeof(cmd_buf));
	clear();

	setup_fonts();

	return 1;
}

static unsigned char ssd1306_init(void)
{
	unsigned char cmd_buf[] = {
		0xAE, // [00] Set display OFF

		0xD5, // [01] Set Display Clock Divide Ratio / OSC Frequency
		0x80, // [02] Display Clock Divide Ratio / OSC Frequency

		0xA8, // [03] Set Multiplex Ratio
		0x3F, // [04] Multiplex Ratio for 128x64 (64-1)

		0xD3, // [05] Set Display Offset
		0x00, // [06] Display Offset

		0x8D, // [07] Set Charge Pump
		0x14, // [08] Charge Pump (0x10 External, 0x14 Internal DC/DC)

		0xA0, // [09] Set Segment Re-Map
		0xC0, // [10] Set Com Output Scan Direction

		0xDA, // [11] Set COM Hardware Configuration
		0x02, // [12] COM Hardware Configuration

		0x81, // [13] Set Contrast
		0x8F, // [14] Contrast

		0xD9, // [15] Set Pre-Charge Period
		0xF1, // [16] Set Pre-Charge Period (0x22 External, 0xF1 Internal)

		0xDB, // [17] Set VCOMH Deselect Level
		0x30, // [18] VCOMH Deselect Level

		0x40, // [19] Set Display Start Line

		0x20, // [20] Set Memory Addressing Mode
		0x00, // [21] Set Horizontal Mode

		0x21, // [22] Set Column Address
		0x00, // [23] First column
		0x7F, // [24] Last column

		0x22, // [25] Set Page Address
		0x00, // [26] First page
		0x07, // [27] Last page

		0xA4, // [28] Set all pixels OFF
		0xA6, // [29] Set display not inverted
		0xAF, // [30] Set display On
	};

	protocol = init_i2c(ssd1306_display.address, I2C_MSB_FIRST, dev->clk_pin, dev->dat_pin, ssd1306_display.flags_low_freq ? I2C_DELAY_100KHz : I2C_DELAY_500KHz);
	if (!protocol)
		return 0;

	cmd_buf[4] = max(min(rows-1, 63), 15);					// [04] Multiplex Ratio for 128 x rows (rows-1)
	cmd_buf[8] = ssd1306_display.flags_ext_vcc ? 0x10 : 0x14;		// [08] Charge Pump (0x10 External, 0x14 Internal DC/DC)
	cmd_buf[9] |= ssd1306_display.flags_rotate ? 0x01 : 0x00;		// [09] Set Segment Re-Map
	cmd_buf[10] |= ssd1306_display.flags_rotate ? 0x08 : 0x00;		// [10] Set Com Output Scan Direction
	cmd_buf[12] |= ssd1306_display.flags_alt_com_conf ? 0x10 : 0x00;	// [12] COM Hardware Configuration
	cmd_buf[14] = (dev->brightness * 36) + 1;				// [14] Contrast
	cmd_buf[16] = ssd1306_display.flags_ext_vcc ? 0x22 : 0xF1;		// [16] Set Pre-Charge Period (0x22 External, 0xF1 Internal)
	cmd_buf[23] = col_offset;						// [23] First column
	cmd_buf[24] = col_offset + columns - 1;					// [24] Last column
	cmd_buf[27] = banks - 1;						// [27] Last page
	cmd_buf[29] |= ssd1306_display.flags_invert ? 0x01 : 0x00;		// [29] Set display not inverted
	write_oled_command_buf(cmd_buf, sizeof(cmd_buf));
	clear();

	setup_fonts();

	return 1;
}

static unsigned short ssd1306_get_brightness_levels_count(void)
{
	return 8;
}

static unsigned short ssd1306_get_brightness_level(void)
{
	return dev->brightness;
}

static unsigned char ssd1306_set_brightness_level(unsigned short level)
{
	unsigned char tmp = dev->brightness = level & 0x7;
	dev->power = 1;
	set_contrast(tmp * 36); // ruonds to 0 - 252.
	return 1;
}

static unsigned char ssd1306_get_power(void)
{
	return dev->power;
}

static void ssd1306_set_power(unsigned char state)
{
	set_power(state);
	dev->power = state;
}

static struct vfd_display *ssd1306_get_display_type(void)
{
	return &dev->dtb_active.display;
}

static unsigned char ssd1306_set_display_type(struct vfd_display *display)
{
	unsigned char ret = 0;
	if (display->controller >= CONTROLLER_SSD1306 && display->controller <= CONTROLLER_SH1106) {
		dev->dtb_active.display = *display;
		ssd1306_init();
		ret = 1;
	}

	return ret;
}

static void ssd1306_set_icon(const char *name, unsigned char state)
{
	enum indicator_icons icon = INDICATOR_ICON_NONE;
	if (strncmp(name,"usb",3) == 0 && indicators.usb != state) {
		icon = INDICATOR_ICON_USB;
		indicators.usb = state;
	} else if (strncmp(name,"sd",2) == 0 && indicators.sd != state) {
		icon = INDICATOR_ICON_SD;
		indicators.sd = state;
	} else if (strncmp(name,"play",4) == 0 && indicators.play != state) {
		icon = INDICATOR_ICON_PLAY;
		indicators.play = state;
	} else if (strncmp(name,"pause",5) == 0 && indicators.pause != state) {
		icon = INDICATOR_ICON_PAUSE;
		indicators.pause = state;
	} else if (strncmp(name,"eth",3) == 0 && indicators.eth != state) {
		icon = INDICATOR_ICON_ETH;
		indicators.eth = state;
	} else if (strncmp(name,"wifi",4) == 0 && indicators.wifi != state) {
		icon = INDICATOR_ICON_WIFI;
		indicators.wifi = state;
	} else if (strncmp(name,"b-t",3) == 0 && indicators.bt != state) {
		icon = INDICATOR_ICON_BT;
		indicators.bt = state;
	} else if (strncmp(name,"apps",4) == 0 && indicators.apps != state) {
		icon = INDICATOR_ICON_APPS;
		indicators.apps = state;
	} else if (strncmp(name,"setup",5) == 0 && indicators.setup != state) {
		icon = INDICATOR_ICON_SETUP;
		indicators.setup = state;
	} else if (strncmp(name,"colon",5) == 0) {
		dev->status_led_mask = state ? (dev->status_led_mask | ledDots[LED_DOT_SEC]) : (dev->status_led_mask & ~ledDots[LED_DOT_SEC]);
	}

	switch (icon) {
	case INDICATOR_ICON_USB:
		if (!indicators.usb && indicators.sd)
			print_indicator(INDICATOR_ICON_SD, 1, 2);
		else
			print_indicator(INDICATOR_ICON_USB, indicators.usb, 2);
		break;
	case INDICATOR_ICON_SD:
		if (!indicators.usb)
			print_indicator(INDICATOR_ICON_SD, indicators.sd, 2);
		break;
	case INDICATOR_ICON_PLAY:
		if (!indicators.play && indicators.pause)
			print_indicator(INDICATOR_ICON_PAUSE, 1, 1);
		else
			print_indicator(INDICATOR_ICON_PLAY, indicators.play, 1);
		break;
	case INDICATOR_ICON_PAUSE:
		if (!indicators.pause && indicators.play)
			print_indicator(INDICATOR_ICON_PLAY, 1, 1);
		else
			print_indicator(INDICATOR_ICON_PAUSE, indicators.pause, 1);
		break;
	case INDICATOR_ICON_ETH:
		if (!indicators.eth && indicators.wifi)
			print_indicator(INDICATOR_ICON_WIFI, 1, 0);
		else
			print_indicator(INDICATOR_ICON_ETH, indicators.eth, 0);
		break;
	case INDICATOR_ICON_WIFI:
		if (!indicators.eth)
			print_indicator(INDICATOR_ICON_WIFI, indicators.wifi, 0);
		break;
	case INDICATOR_ICON_BT:
	case INDICATOR_ICON_APPS:
	case INDICATOR_ICON_SETUP:
		if (indicators.setup)
			print_indicator(INDICATOR_ICON_SETUP, indicators.setup, 3);
		else if (indicators.apps)
			print_indicator(INDICATOR_ICON_APPS, indicators.apps, 3);
		else
			print_indicator(INDICATOR_ICON_BT, indicators.bt, 3);
		break;
	default:
		break;
	}
}

static size_t ssd1306_read_data(unsigned char *data, size_t length)
{
	return 0;
}

static size_t ssd1306_write_data(const unsigned char *_data, size_t length)
{
	return length;
}

static size_t ssd1306_write_display_data(const struct vfd_display_data *data)
{
	size_t status = sizeof(*data);
	if (data->mode != old_data.mode) {
		icon_x_offset = 0;
		memset(&old_data, 0, sizeof(old_data));
		clear();
		switch (data->mode) {
		case DISPLAY_MODE_CLOCK:
			old_data.mode = DISPLAY_MODE_CLOCK;
			for (unsigned char i = 0; i < MAX_INDICATORS; i++)
				print_indicator(indicators_on_screen[i], 1, i);
			old_data.mode = 0;
			break;
		case DISPLAY_MODE_DATE:
			if (show_icons)
				icon_x_offset = print_icon(INDICATOR_ICON_CALENDAR);
			break;
		case DISPLAY_MODE_CHANNEL:
			if (show_icons)
				icon_x_offset = print_icon(INDICATOR_ICON_CHANNEL);
			break;
		case DISPLAY_MODE_PLAYBACK_TIME:
			if (show_icons)
				icon_x_offset = print_icon(INDICATOR_ICON_MEDIA);
			break;
		case DISPLAY_MODE_TITLE:
			break;
		case DISPLAY_MODE_TEMPERATURE:
			if (show_icons)
				icon_x_offset = print_icon(INDICATOR_ICON_TEMP);
			break;
		default:
			status = 0;
			break;
		}
	}

	switch (data->mode) {
	case DISPLAY_MODE_CLOCK:
		print_clock(data, 1);
		break;
	case DISPLAY_MODE_DATE:
		print_date(data);
		break;
	case DISPLAY_MODE_CHANNEL:
		print_channel(data);
		break;
	case DISPLAY_MODE_PLAYBACK_TIME:
		print_playback_time(data);
		break;
	case DISPLAY_MODE_TITLE:
		print_title(data);
		break;
	case DISPLAY_MODE_TEMPERATURE:
		print_temperature(data);
		break;
	default:
		status = 0;
		break;
	}

	old_data = *data;
	return status;
}

static unsigned char print_icon(unsigned char ch)
{
	char str[] = { ch, 0 };
	unsigned char offset_x = 0;
	unsigned char x, y;
	switch (display_mode) {
	case DISPLAY_MODE_128x32:
		y = (banks - font_icons.font_height) / 2;
		print_string(str, &font_icons, 0, y);
		offset_x = font_icons.font_width + font_text.font_width / 2;
		break;
	case DISPLAY_MODE_96x32	:
		y = (banks - font_icons.font_height) / 2;
		print_string(str, &font_icons, 0, y);
		offset_x = font_icons.font_width;
		break;
	case DISPLAY_MODE_80x32	:
		break;
	case DISPLAY_MODE_64x48	:
	case DISPLAY_MODE_64x64	:
	case DISPLAY_MODE_96x64	:
		print_string(str, &font_icons, 0, font_text.font_height);
		break;
	case DISPLAY_MODE_80x48	:
	case DISPLAY_MODE_128x64:
	case DISPLAY_MODE_80x64	:
		x = (columns - font_icons.font_width) / 2;
		print_string(str, &font_icons, x, font_text.font_height);
		break;
	}

	return offset_x;
}

static void print_indicator(unsigned char ch, unsigned char state, unsigned char index)
{
	char str[] = { state ? ch : INDICATOR_ICON_NONE, 0 };
	unsigned char x, y;
	if (index >= MAX_INDICATORS)
		return;

	indicators_on_screen[index] = str[0];
	if (old_data.mode == DISPLAY_MODE_CLOCK) {
		switch (display_mode) {
		case DISPLAY_MODE_128x32:
			x = (24 - font_indicators.font_width) / 2; // (128 - 80) / 2 = 24
			if (index >= 2)
				x += 104;
			y = font_indicators.font_height * (index % 2);
			print_string(str, &font_indicators, x, y);
			break;
		case DISPLAY_MODE_96x32	:
		case DISPLAY_MODE_80x32	:
			break;
		case DISPLAY_MODE_64x48	:
		case DISPLAY_MODE_64x64	:
		case DISPLAY_MODE_96x64	:
		case DISPLAY_MODE_80x48	:
		case DISPLAY_MODE_128x64:
		case DISPLAY_MODE_80x64	:
			x = columns / MAX_INDICATORS;
			x = (x - font_indicators.font_width) / 2 + index * x;
			print_string(str, &font_indicators, x, font_text.font_height);
			break;
		}
	}
}

static void print_clock(const struct vfd_display_data *data, unsigned char print_seconds)
{
	char buffer[10];
	unsigned char force_print = old_data.mode == DISPLAY_MODE_NONE;
	unsigned char offset = 0;
	unsigned char colon_on = data->colon_on || dev->status_led_mask & ledDots[LED_DOT_SEC] ? 1 : 0;
	print_seconds &= ssd1306_display.flags_secs & show_colon;
	if (ssd1306_display.flags_transpose) {
		if (force_print || data->time_date.minutes != old_data.time_date.minutes ||
			data->time_date.hours != old_data.time_date.hours) {
			offset = (columns - (font_text.font_height * 8 * 2 + show_colon * font_text.font_width)) / 2;
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.hours);
			print_string(buffer, &font_text, offset, 0);
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
			print_string(buffer, &font_text, offset + show_colon * font_text.font_width + (font_text.font_height * 8), 0);
		}
		if (colon_on != old_data.colon_on && show_colon) {
			unsigned char offset = (columns - font_text.font_width) / 2;
			print_char(colon_on ? ':' : ' ', &font_text, offset, banks - font_text.font_height);
		}
	} else if (!force_print) {
		const int len = print_seconds ? 8 : 5;
		offset = (columns - (font_text.font_width * len)) / 2;
		if (data->time_date.hours != old_data.time_date.hours) {
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.hours);
			print_string(buffer, &font_text, offset, 0);
		}
		offset += 2 * font_text.font_width;
		if (show_colon) {
			if (colon_on != old_data.colon_on)
				print_char(colon_on ? ':' : ' ', &font_text, offset, 0);
			offset += font_text.font_width;
		}
		if (data->time_date.minutes != old_data.time_date.minutes) {
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
			print_string(buffer, &font_text, offset, 0);
		}
		offset += 2 * font_text.font_width;
		if (print_seconds) {
			if (colon_on != old_data.colon_on)
				print_char(colon_on ? ':' : ' ', &font_text, offset, 0);
			offset += font_text.font_width;
			if (data->time_date.seconds != old_data.time_date.seconds) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.seconds);
				print_string(buffer, &font_text, offset + (6 * font_text.font_width), 0);
			}
		}
	} else if (print_seconds) {
		int len = scnprintf(buffer, sizeof(buffer), "%02d%c%02d%c%02d", data->time_date.hours, colon_on ? ':' : ' ', data->time_date.minutes,
			colon_on ? ':' : ' ', data->time_date.seconds);
		offset = (columns - (font_text.font_width * len)) / 2;
		print_string(buffer, &font_text, offset, 0);
	} else {
		int len;
		if (show_colon)
			len = scnprintf(buffer, sizeof(buffer), "%02d%c%02d", data->time_date.hours, colon_on ? ':' : ' ', data->time_date.minutes);
		else
			len = scnprintf(buffer, sizeof(buffer), "%02d%02d", data->time_date.hours, data->time_date.minutes);
		offset = (columns - (font_text.font_width * len)) / 2;
		print_string(buffer, &font_text, offset, 0);
	}
}

static void print_channel(const struct vfd_display_data *data)
{
	char buffer[10];
	scnprintf(buffer, sizeof(buffer), "%*d", 4, data->channel_data.channel % 10000);
	print_string(buffer, &font_text, font_icons.font_width + (font_text.font_width / 2), 0);
}

static void print_playback_time(const struct vfd_display_data *data)
{
	char buffer[20];
	unsigned char offset = icon_x_offset ? icon_x_offset : (columns - (show_colon * font_text.font_width + font_text.font_width * 4)) / 2;
	unsigned char force_print = old_data.mode == DISPLAY_MODE_NONE || data->time_date.hours != old_data.time_date.hours;
	if (ssd1306_display.flags_transpose) {
		if (data->time_date.hours > 0) {
			if (force_print || data->time_date.minutes != old_data.time_date.minutes ||
				data->time_date.hours != old_data.time_date.hours) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.hours);
				print_string(buffer, &font_text, offset, 0);
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
				print_string(buffer, &font_text, offset + show_colon * font_text.font_width + (font_text.font_height * 8), 0);
			}
		} else {
			if (force_print || data->time_date.seconds != old_data.time_date.seconds ||
				data->time_date.minutes != old_data.time_date.minutes) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
				print_string(buffer, &font_text, offset, 0);
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.seconds);
				print_string(buffer, &font_text, offset + show_colon * font_text.font_width + (font_text.font_height * 8), 0);
			}
		}
		if (show_colon)
			print_char(data->colon_on ? ':' : ' ', &font_text, offset + (font_text.font_height * 8), banks - font_text.font_height);
	} else if (!force_print) {
		if (data->colon_on != old_data.colon_on && show_colon) {
			print_char(data->colon_on ? ':' : ' ', &font_text, offset + (2 * font_text.font_width), 0);
		}
		if (data->time_date.hours > 0) {
			if (data->time_date.hours != old_data.time_date.hours) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.hours);
				print_string(buffer, &font_text, offset, 0);
			}
			if (data->time_date.minutes != old_data.time_date.minutes) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
				print_string(buffer, &font_text, offset + (show_colon * font_text.font_width + 2 * font_text.font_width), 0);
			}
		} else {
			if (data->time_date.minutes != old_data.time_date.minutes) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
				print_string(buffer, &font_text, offset, 0);
			}
			if (data->time_date.seconds != old_data.time_date.seconds) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.seconds);
				print_string(buffer, &font_text, offset + (show_colon * font_text.font_width + 2 * font_text.font_width), 0);
			}
		}
	} else {
		unsigned char pos0, pos1;
		if (data->time_date.hours > 0) {
			pos0 = data->time_date.hours;
			pos1 = data->time_date.minutes;
		} else {
			pos0 = data->time_date.minutes;
			pos1 = data->time_date.seconds;
		}
		if (show_colon)
			scnprintf(buffer, sizeof(buffer), "%02d%c%02d", pos0, data->colon_on ? ':' : ' ', pos1);
		else
			scnprintf(buffer, sizeof(buffer), "%02d%02d", pos0, pos1);
		print_string(buffer, &font_text, offset, 0);
	}
	if (banks >= 8 && !ssd1306_display.flags_transpose && strcmp(data->string_main, old_data.string_main)) {
		struct rect rect;
		offset = show_icons * font_icons.font_width;
		init_rect(&rect, &font_small_text, data->string_main, offset, font_text.font_height, 0);
		if (rect.length > 0) {
			if (show_icons) {
				print_icon(INDICATOR_ICON_NONE);
				buffer[0] = INDICATOR_ICON_MEDIA;
				buffer[1] = '\0';
				print_string(buffer, &font_icons, 0, font_text.font_height);
			}
			if (rect.length < strlen(data->string_main)) {
				scnprintf(buffer, sizeof(buffer), "%s", data->string_main);
				buffer[rect.length - 1] = 0x7F; // 0x7F = position of ellipsis.
				buffer[rect.length] = '\0';
				print_string(buffer, &font_small_text, offset, font_text.font_height);
			} else {
				print_string(data->string_main, &font_small_text, offset, font_text.font_height);
			}
		}
	}
}

static void print_title(const struct vfd_display_data *data)
{
}

static void print_date(const struct vfd_display_data *data)
{
	char buffer[10];
	unsigned char force_print = old_data.mode == DISPLAY_MODE_NONE || data->time_date.day != old_data.time_date.day || data->time_date.month != old_data.time_date.month;
	unsigned char offset = icon_x_offset ? icon_x_offset : (columns - (show_colon * font_text.font_width + font_text.font_width * 4)) / 2;
	if (force_print) {
		if (ssd1306_display.flags_transpose) {
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_secondary._reserved ? data->time_date.month + 1 : data->time_date.day);
			print_string(buffer, &font_text, offset, 0);
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_secondary._reserved ? data->time_date.day : data->time_date.month + 1);
			print_string(buffer, &font_text, offset + show_colon * font_text.font_width + (font_text.font_height * 8), 0);
			if (show_colon)
				print_char('|', &font_text, offset + font_text.font_height * 8, banks - font_text.font_height);
		} else {
			unsigned char day, month;
			if (data->time_secondary._reserved) {
				day = data->time_date.month + 1;
				month = data->time_date.day;
			} else {
				day = data->time_date.day;
				month = data->time_date.month + 1;
			}
			if (show_colon)
				scnprintf(buffer, sizeof(buffer), "%02d/%02d", day, month);
			else
				scnprintf(buffer, sizeof(buffer), "%02d%02d", day, month);
			print_string(buffer, &font_text, offset, 0);
		}
	}
}

static void print_temperature(const struct vfd_display_data *data)
{
	char buffer[10];
	if (data->temperature != old_data.temperature) {
		if (ssd1306_display.flags_transpose) {
			unsigned char offset = icon_x_offset ? icon_x_offset : (columns - (2 * 8 * font_text.font_height)) / 2;
			scnprintf(buffer, sizeof(buffer), "%02d", data->temperature % 100);
			print_string(buffer, &font_text, offset, 0);
			scnprintf(buffer, sizeof(buffer), "%cC", 0x7F); // 0x7F = position of degree.
			print_string(buffer, &font_text, offset + 8 * font_text.font_height, 0);
		} else {
			size_t len = scnprintf(buffer, sizeof(buffer), "%d%cC", data->temperature % 1000, 0x7F); // 0x7F = position of degree.
			unsigned char offset = icon_x_offset ? icon_x_offset : (columns - (len * font_text.font_width)) / 2;
			print_string(buffer, &font_text, offset, 0);
		}
	}
}

static void init_font(struct font *font_struct, const unsigned char *font_bitmaps)
{
	font_struct->font_width = font_bitmaps[0];
	font_struct->font_height = font_bitmaps[1] / 8;
	font_struct->font_offset = font_bitmaps[2];
	font_struct->font_char_size = font_struct->font_height * font_struct->font_width;
	font_struct->font_char_count = font_bitmaps[3];
	font_struct->font_bitmaps = font_bitmaps;
}

static void init_rect(struct rect *rect, const struct font *font, const char *str, unsigned char x, unsigned char y, unsigned char transposed)
{
	unsigned char c_width = 0, c_height = 0;
	memset(rect, 0, sizeof(*rect));
	if (x < columns && y < banks) {
		rect->font = font;
		if (!transposed) {
			rect->x1 = x;
			rect->y1 = y;
			rect->width = columns - x;
			rect->height = banks - y;
			c_width = rect->width / font->font_width;
			c_height = rect->height / font->font_height;
			rect->length = (unsigned char)min(strlen(str), (size_t)(c_width * c_height));
			rect->text_width = min(c_width, rect->length);
			for (rect->text_height = 0; rect->text_height < c_height; rect->text_height++)
				if (rect->text_width * rect->text_height >= rect->length)
					break;
			rect->width = rect->text_width * font->font_width;
			rect->height = rect->text_height * font->font_height;
			rect->x2 = x + rect->width - 1;
			rect->y2 = y + rect->height - 1;
		} else {
			const unsigned char font_height = font->font_height * 8;
			const unsigned char font_width = font->font_width / 8;
			rect->width = columns - x;
			rect->height = banks - y;
			c_width = rect->width / font_height;
			c_height = rect->height / font_width;
			rect->length = (unsigned char)min(strlen(str), (size_t)(c_width * c_height));
			rect->text_height = min(rect->length, c_height);
			for (rect->text_width = 0; rect->text_width < c_width; rect->text_width++)
				if (rect->text_width * rect->text_height >= rect->length)
					break;
			rect->width = rect->text_width * font_height;
			rect->height = rect->text_height * font_width;
			rect->x1 = x;
			rect->y2 = banks - 1 - y;
			rect->x2 = rect->x1 + rect->width - 1;
			rect->y1 = rect->y2 - rect->height + 1;
		}
	}

#if SSD1306_PRINT_DEBUG
	pr_dbg2("str = %s, c_width = %d, c_height = %d, length = %d, xy1 = (%d,%d), xy2 = (%d,%d), size = (%d,%d), text size = (%d,%d)\n",
		str, c_width, c_height, rect->length, rect->x1, rect->y1, rect->x2, rect->y2, rect->width, rect->height, rect->text_width, rect->text_height);
#endif
}
