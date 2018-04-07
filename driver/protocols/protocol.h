#ifndef __PROTOCOLS__
#define __PROTOCOLS__

struct protocol_interface {
	unsigned char (*read_cmd_data)(const unsigned char *cmd, unsigned int cmd_length, unsigned char *data, unsigned int data_length);
	unsigned char (*read_data)(unsigned char *data, unsigned int length);
	unsigned char (*read_byte)(unsigned char *bdata);
	unsigned char (*write_cmd_data)(const unsigned char *cmd, unsigned int cmd_length, const unsigned char *data, unsigned int data_length);
	unsigned char (*write_data)(const unsigned char *data, unsigned int length);
	unsigned char (*write_byte)(unsigned char bdata);
};

#endif
