#ifndef __PROTOCOLS__
#define __PROTOCOLS__

struct protocol_interface {
	unsigned char (*read_data)(unsigned char *data, int length);
	unsigned char (*read_byte)(unsigned char *bdata);
	unsigned char (*write_data)(unsigned char *data, int length);
	unsigned char (*write_byte)(unsigned char bdata);
};

#endif
