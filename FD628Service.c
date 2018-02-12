#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <dirent.h>
#include <stdint.h>

#include <time.h>
#include "driver/aml_fd628.h"

#define UNUSED(x)	(void*)(x)
#define DEV_NAME	"/dev/fd628_dev"

bool set_display_type(int new_display_type);
bool is_test_mode(int argc, char *argv[]);
int get_cmd_display_type(int argc, char *argv[]);
int get_cmd_chars_order(int argc, char *argv[], u_int8 chars[], const int sz);
bool print_usage(int argc, char *argv[]);

typedef struct _DotLedBitMap {
	uint8_t on;
	uint8_t bitmap;
}DotLedBitMap;

#define LED_MASK_VOID	0x00
static DotLedBitMap dotLeds[LED_DOT_MAX] = {
	{0, 0x01},
	{0, 0x02},
	{0, 0x04},
	{0, 0x08},
	{0, 0x10},
	{0, 0x20},
	{0, 0x40},
};

#define LEDCODES_LEN	(sizeof(LED_decode_tab1)/sizeof(LED_decode_tab1[0]))
static const led_bitmap *ledCodes = LED_decode_tab1;
static int display_type = DISPLAY_UNKNOWN;

int fd628_fd;

uint8_t char_to_mask(uint8_t ch)
{
	unsigned int index = 0;
	for (index = 0; index < LEDCODES_LEN; index++) {
		if (ledCodes[index].character == ch) {
			return ledCodes[index].bitmap;
		}
	}

	return LED_MASK_VOID;
}

void mdelay(int n)
{
	unsigned long msec=(n);
	while (msec--)
		usleep(1000);
}

void led_show_time_loop()
{
	unsigned short write_buffer[LED_DOT_MAX];
	int ret = -1;
	int hours;

	time_t now;
	struct tm *timenow;

	while(1) {
		// Get current time
		time(&now);
		timenow = localtime(&now);
		hours = timenow->tm_hour;

		write_buffer[1] = char_to_mask(hours / 10);
		write_buffer[2] = char_to_mask(hours % 10);
		write_buffer[3] = char_to_mask(timenow->tm_min / 10);
		write_buffer[4] = char_to_mask(timenow->tm_min % 10);

		// Toggle colon LED on/off every 500ms.
		dotLeds[LED_DOT_SEC].on = ~dotLeds[LED_DOT_SEC].on;
		write_buffer[0] = dotLeds[LED_DOT_SEC].on ? dotLeds[LED_DOT_SEC].bitmap : LED_MASK_VOID;
		ret = write(fd628_fd,write_buffer,sizeof(write_buffer[0])*5);
		mdelay(500);
	}
}

void led_test_codes()
{
	unsigned short write_buffer[7];
	int ret = -1;
	unsigned char val = ':';
	unsigned int i = 0;

	// Test chart, sequence of numbers.
	for(i = 0; i < LEDCODES_LEN; i++) {
		val = ~val;
		write_buffer[1] = char_to_mask(ledCodes[i].character);
		write_buffer[2] = char_to_mask(ledCodes[i].character);
		write_buffer[3] = char_to_mask(ledCodes[i].character);
		write_buffer[4] = char_to_mask(ledCodes[i].character);
		write_buffer[0] = val;

		ret = write(fd628_fd,write_buffer,sizeof(write_buffer[0])*5);
		mdelay(500);
	}

	// Test bit sequence.
	for(i = 0; i < 10; i++) {
		val = ~val;
		write_buffer[1] = char_to_mask(1);
		write_buffer[2] = char_to_mask(2);
		write_buffer[3] = char_to_mask(3);
		write_buffer[4] = char_to_mask(4);
		write_buffer[0] = val;

		ret = write(fd628_fd,write_buffer,sizeof(write_buffer[0])*5);
		mdelay(500);
	}

	// Test sequence 2
	write_buffer[0] = 0;
	for(i = 0; i < LED_DOT_MAX; i++){
		val = ~val;
		write_buffer[1] = char_to_mask(5);
		write_buffer[2] = char_to_mask(6);
		write_buffer[3] = char_to_mask(7);
		write_buffer[4] = char_to_mask(8);
		write_buffer[0] &= dotLeds[i%LED_DOT_MAX].bitmap;

		ret = write(fd628_fd,write_buffer,sizeof(write_buffer[0])*5);
		mdelay(500);
	}

	// Test sequence 3
	write_buffer[0] = 0;
	for(i = 0; i < LED_DOT_MAX; i++){
		val = ~val;
		write_buffer[1] = char_to_mask(6);
		write_buffer[2] = char_to_mask(7);
		write_buffer[3] = char_to_mask(8);
		write_buffer[4] = char_to_mask(9);
		write_buffer[0] |= dotLeds[i%LED_DOT_MAX].bitmap;

		ret = write(fd628_fd,write_buffer,sizeof(write_buffer[0])*5);
		mdelay(500);
	}
}

void led_test_loop(bool cycle_display_types)
{
	int current_type = DISPLAY_UNKNOWN;
	const pid_t pid = getpid();
	printf("Initializing...\n");
	if (!cycle_display_types)
		printf("Process ID = %d\n", pid);
	while (1) {
		int i;
		const int len = 7;
		unsigned short wb[7];
		const size_t sz = sizeof(wb[0])*len;

		if (cycle_display_types) {
			printf("Process ID = %d\n", pid);
			current_type = (++current_type) % DISPLAY_TYPE_MAX;
			printf("Set display type to %d\n", current_type);
			set_display_type(current_type);
			select_display_type();
		}

		// Light up all sections and cycle
		// through display brightness levels.
		memset(wb, 0xFF, sz);
		write(fd628_fd,wb,sz);
		for (i = FD628_Brightness_1; i <= FD628_Brightness_8; i++) {
			ioctl(fd628_fd, FD628_IOC_SBRIGHT, &i);
			mdelay(1000);
		}

		// Clear display for a second.
		memset(wb, 0x00, sz);
		write(fd628_fd,wb,sz);
		mdelay(1000);

		// Run original test codes.
		led_test_codes();

		// Cycle through fully lit characters.
		for (i = 0; i < len; i++) {
			memset(wb, 0x00, sz);
			wb[i] = 0xFF;
			write(fd628_fd,wb,sz);
			mdelay(1000);
		}

		// Cycle through bits in each character.
		for (i = 0; i < len; i++) {
			memset(wb, (1 << i), sz);
			write(fd628_fd,wb,sz);
			mdelay(1000);
		}
	}
}

void *display_time_thread_handler(void *arg)
{
	UNUSED(arg);
	led_show_time_loop();
	pthread_exit(NULL);
}

void *display_test_thread_handler(void *arg)
{
	bool cycle_display_types = *(bool*)arg;
	led_test_loop(cycle_display_types);
	pthread_exit(NULL);
}

void select_display_type()
{
	if (!ioctl(fd628_fd, FD628_IOC_GDISPLAY_TYPE, &display_type)) {
		switch(display_type) {
			case DISPLAY_UNKNOWN:
			case DISPLAY_COMMON_ANODE_FD628:
			case DISPLAY_COMMON_CATHODE_5D_FD620:
			case DISPLAY_COMMON_CATHODE_4D_FD620:
			case DISPLAY_COMMON_CATHODE_5D_TM1618:
			case DISPLAY_COMMON_CATHODE_4D_TM1618:
			default:
				ledCodes = LED_decode_tab2;
				break;
			case DISPLAY_COMMON_CATHODE_FD628:
				ledCodes = LED_decode_tab1;
				break;
		}
	} else {
		display_type = DISPLAY_UNKNOWN;
		perror("Failed to read display type, using default.");
	}
}

bool set_display_type(int new_display_type)
{
	long ret = ioctl(fd628_fd, FD628_IOC_SDISPLAY_TYPE, &new_display_type);
	if (ret) {
		printf("Failed setting a new display type.\n");
		if (ret == ERANGE)
			printf("Unsupported display type. (out of range)\n");
	}

	return ret == 0;
}

int main(int argc, char *argv[])
{
	u_int8 char_indexes[7];
	int ret, char_order_count;
	bool test_mode = false;
	bool cycle_display_types = true;
	pthread_t disp_id, check_dev_id = 0;

	if (print_usage(argc, argv))
		return 0;
	fd628_fd = open(DEV_NAME, O_RDWR);
	if (fd628_fd < 0) {
		perror("Open device fd628_fd\n");
		exit(1);
	}

	char_order_count = get_cmd_chars_order(argc, argv, char_indexes, (int)sizeof(char_indexes));
	if (char_order_count)
		if (ioctl(fd628_fd, FD628_IOC_SCHARS_ORDER, char_indexes))
			printf("Error setting new character order.\n");

	display_type = get_cmd_display_type(argc, argv);
	if (display_type >= 0)
		cycle_display_types = !set_display_type(display_type);
	select_display_type();

	test_mode = is_test_mode(argc, argv);
	if (test_mode)
		ret = pthread_create(&disp_id, NULL, display_test_thread_handler, &cycle_display_types);
	else
		ret = pthread_create(&disp_id, NULL, display_time_thread_handler, NULL);
	if(ret != 0) {
		perror("Create disp_id thread error\n");
		return ret;
	}
	pthread_join(disp_id, NULL);
	pthread_join(check_dev_id, NULL);
	close(fd628_fd);
	return 0;
}

bool is_test_mode(int argc, char *argv[])
{
	bool ret = false;
	int i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-t")) {
			ret = true;
			break;
		}
	}

	return ret;
}

int get_cmd_display_type(int argc, char *argv[])
{
	int ret = -1, i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-dt")) {
			if (argc >= (i + 2)) {
				long temp = -1;
				char *end;
				temp = strtol(argv[i+1], &end, 10);
				if (end == argv[i+1] || *end != '\0' || errno == ERANGE)
					printf("Error parsing display type index.\n");
				else
					ret = (int)temp;
			} else {
				printf("Error parsing display type index, missing argument.\n");
			}
			break;
		}
	}

	return ret;
}

int get_cmd_chars_order(int argc, char *argv[], u_int8 chars[], const int sz)
{
	int ret = 0, i, j;
	for (i = 0; i < sz; i++)
		chars[i] = i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-co")) {
			for (i++, j = 0; i < argc && j < sz; i++) {
				long temp = -1;
				char *end;
				temp = strtol(argv[i], &end, 10);
				if (end == argv[i] || *end != '\0' || errno == ERANGE)
					break;
				else if (temp >= 0 && temp < sz)
					chars[j++] = temp;
			}

			ret = j;
			break;
		}
	}

	if (ret) {
		printf("Got %d char indexes.\n", ret);
		for (i = 0; i < ret; i++) {
			printf("index[%d] = %d\n", i, chars[i]);
		}
	}

	return ret;
}

bool print_usage(int argc, char *argv[])
{
	bool ret = false;
	int i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			ret = true;
			printf("\nUsage: FD628Service [-t] [-dt INDEX] [-h]\n\n");
			printf("\t-t\t\tRun FD628Service in display test mode.\n");
			printf("\t-dt N\t\tSpecifies which display type to use.\n");
			printf("\t-co N...\t< D HH:MM > Order of display chars.\n\t\t\tValid values are 0 - 6.\n\t\t\t(D=dots, represented by a single char)\n");
			printf("\t-h\t\tThis text.\n\n");
		}
	}

	return ret;
}
