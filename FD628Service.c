#include <stdio.h>
#include <stdlib.h>
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

static uint8_t char_to_mask(uint8_t ch)
{
	unsigned int index = 0;
	for (index = 0; index < LEDCODES_LEN; index++) {
		if (ledCodes[index].character == ch) {
			return ledCodes[index].bitmap;
		}
	}

	return LED_MASK_VOID;
}

static void mdelay(int n)
{
	unsigned long msec=(n);
	while (msec--)
		usleep(1000);
}

static void led_show_time_loop()
{
	unsigned char write_buffer[LED_DOT_MAX];
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
	unsigned char write_buffer[8];
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

static void led_test_loop()
{
	int i;
	pid_t pid = getpid();
	printf("Initializing...\n");
	printf("Process ID = %d\n", pid);
	while (1) {
		const int len = 7;
		unsigned char wb[7];
		const size_t sz = sizeof(wb[0])*len;

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

static void *display_time_thread_handler(void *arg)
{
	UNUSED(arg);
	led_show_time_loop();
	pthread_exit(NULL);
}

static void *display_test_thread_handler(void *arg)
{
	UNUSED(arg);
	led_test_loop();
	pthread_exit(NULL);
}

void selectDisplayType()
{
	if (!ioctl(fd628_fd, FD628_IOC_DISPLAY_TYPE, &display_type)) {
		switch(display_type) {
			case DISPLAY_UNKNOWN:
			case DISPLAY_COMMON_CATHODE:
			default:
				ledCodes = LED_decode_tab1;
				break;
			case DISPLAY_COMMON_ANODE:
				ledCodes = LED_decode_tab2;
				break;
		}
	} else {
		display_type = DISPLAY_UNKNOWN;
		perror("Failed to read display type, using default.");
	}
}

int main(int argc, char *argv[])
{
	pthread_t disp_id,check_dev_id = 0;
	int ret;
	fd628_fd = open(DEV_NAME, O_RDWR);
	if (fd628_fd < 0) {
		perror("Open device fd628_fd\n");
		exit(1);
	}
	selectDisplayType();
	if (argc >= 2 && strstr(argv[1], "-t"))
		ret = pthread_create(&disp_id, NULL, display_test_thread_handler, NULL);
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
