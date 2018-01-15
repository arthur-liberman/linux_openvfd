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

#define  UNUSED(x)  (void*)(x)
#define   	DEV_NAME   "/dev/fd628_dev"

typedef struct _LedBitmap{
	uint8_t character;
	uint8_t mask;
}LedBitmap;

typedef struct _DotLedBitMap{
	uint8_t on;
	uint8_t mask;
}DotLedBitMap;

enum {
	LED_DOT_ALARM,
	LED_DOT_USB,
	LED_DOT_PLAY,
	LED_DOT_PAUSE,
	LED_DOT_SEC,
	LED_DOT_ETH,
	LED_DOT_WIFI,
	LED_DOT_MAX
};

#define LED_MASK_VOID	0x00
static DotLedBitMap ledDots[LED_DOT_MAX] = {
	{0, 0x01},
	{0, 0x02},
	{0, 0x04},
	{0, 0x08},
	{0, 0x10},
	{0, 0x20},
	{0, 0x40},
};

#define LEDCODES_LEN	(sizeof(ledCodes)/sizeof(ledCodes[0]))
#ifndef INVERTED_DISPLAY 
static const LedBitmap ledCodes[] =
{
	//{'0', 0x3F}, {'1', 0x30}, {'2', 0x5B}, {'3', 0x79},
	//{'4', 0x74}, {'5', 0x6d}, {'6', 0x6f}, {'7', 0x38},
	//{'8', 0x7F}, {'9', 0x7d},
    //{'!', 0X01},//
	//{'@', 0X02},//
	//{'#', 0X04},//
	//{'$', 0X08},//
	//{':', 0X10},//
	//{'^', 0X20},//
	//{'&', 0X40},//
	{0,   0x3F},
	{1,   0x30}, {2,   0x5B}, {3,   0x79}, {4,   0x74},
	{5,   0x6d}, {6,   0x6f}, {7,   0x38}, {8,   0x7F},
	{9,   0x7d},

    /* ? what's this ? */
    //{0xC5,0X00}, {0x3b,0x18}, {0xc4,0x08}, {0x3c,0x14},
	//{0xc3,0x04}, {0x3d,0x1c}, {0xc2,0x0c},
};
#else
static const LedBitmap ledCodes[] =
{
	//{'0', 0x3F}, {'1', 0x30}, {'2', 0x5B}, {'3', 0x79},
	//{'4', 0x74}, {'5', 0x6d}, {'6', 0x6f}, {'7', 0x38},
	//{'8', 0x7F}, {'9', 0x7d},
    //{'!', 0X01},//
	//{'@', 0X02},//
	//{'#', 0X04},//
	//{'$', 0X08},//
	//{':', 0X10},//
	//{'^', 0X20},//
	//{'&', 0X40},//
	{0,   0x3f},
	{1,   0x06}, {2,   0x5b}, {3,   0x4f}, {4,   0x66},
	{5,   0x6d}, {6,   0x7d}, {7,   0x07}, {8,   0x7f},
	{9,   0x6f},

    /* ? what's this ? */
    //{0xC5,0X00}, {0x3b,0x18}, {0xc4,0x08}, {0x3c,0x14},
	//{0xc3,0x04}, {0x3d,0x1c}, {0xc2,0x0c},
};
#endif

int fd628_fd;

static uint8_t char_to_mask(uint8_t ch){
	unsigned int index = 0;
	for(index = 0; index < LEDCODES_LEN; index++){
		if(ledCodes[index].character == ch){
			return ledCodes[index].mask;
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

static void led_show_time_loop(){
	unsigned char write_buffer[LED_DOT_MAX];
	int ret = -1;
	int hours;

	time_t now;
	struct tm *timenow;
	int counter = 0;
	int i = 0;
	int dotsVal = 0;

	while(1){
		dotsVal = 0;
	   	counter++;

	   	// 更新时间
	   	time(&now);
	   	timenow  =  localtime(&now);
		hours = timenow->tm_hour;
		

#ifndef INVERTED_DISPLAY 
	   	write_buffer[1] = char_to_mask(hours/10);
       	write_buffer[2] = char_to_mask(hours %10);
	   	write_buffer[3] = char_to_mask(timenow->tm_min /10);
       	write_buffer[4] = char_to_mask(timenow->tm_min %10);
#else
	   	write_buffer[4] = char_to_mask(hours /10);
       	write_buffer[3] = char_to_mask(hours %10);
	   	write_buffer[2] = char_to_mask(timenow->tm_min /10);
       	write_buffer[1] = char_to_mask(timenow->tm_min %10);
#endif

	   	// 冒号灯500ms闪一次
	   	ledDots[LED_DOT_SEC].on = ~ledDots[LED_DOT_SEC].on;
#if 0
		//FIXME:测试代码
		i = counter % LED_DOT_MAX;
		if(i != LED_DOT_SEC){
				ledDots[i].on = ~ledDots[i].on;
		}

		// 更新灯
		for(i = 0; i < LED_DOT_MAX; i++){
		   if(ledDots[i].on){
			   dotsVal |= ledDots[i].mask;
		   }else{
			   dotsVal &= (~ledDots[i].mask);
		   }
		}
		write_buffer[0] = dotsVal;
#else
		write_buffer[0] = ledDots[LED_DOT_SEC].on ? ledDots[LED_DOT_SEC].mask : LED_MASK_VOID;
#endif
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

	// 测试码表
	for(i = 0; i < LEDCODES_LEN; i++){
		val = ~val;
		write_buffer[1] = char_to_mask(ledCodes[i].character);
        write_buffer[2] = char_to_mask(ledCodes[i].character);
 	   	write_buffer[3] = char_to_mask(ledCodes[i].character);
        write_buffer[4] = char_to_mask(ledCodes[i].character);
		write_buffer[0] = val;

		ret = write(fd628_fd,write_buffer,sizeof(write_buffer[0])*5);
		mdelay(500);
	}

	// 测试位序
	for(i = 0; i < 10; i++){
		val = ~val;
		write_buffer[1] = char_to_mask(1);
        write_buffer[2] = char_to_mask(2);
 	   	write_buffer[3] = char_to_mask(3);
        write_buffer[4] = char_to_mask(4);
		write_buffer[0] = val;

		ret = write(fd628_fd,write_buffer,sizeof(write_buffer[0])*5);
		mdelay(500);
	}


	// 测试位序2
	write_buffer[0] = 0;
	for(i = 0; i < LED_DOT_MAX; i++){
		val = ~val;
		write_buffer[1] = char_to_mask(5);
        write_buffer[2] = char_to_mask(6);
 	   	write_buffer[3] = char_to_mask(7);
        write_buffer[4] = char_to_mask(8);
		write_buffer[0] &= ledDots[i%LED_DOT_MAX].mask;

		ret = write(fd628_fd,write_buffer,sizeof(write_buffer[0])*5);
		mdelay(500);
	}

	// 测试位序3
	write_buffer[0] = 0;
	for(i = 0; i < LED_DOT_MAX; i++){
		val = ~val;
		write_buffer[1] = char_to_mask(6);
        write_buffer[2] = char_to_mask(7);
 	   	write_buffer[3] = char_to_mask(8);
        write_buffer[4] = char_to_mask(9);
		write_buffer[0] |= ledDots[i%LED_DOT_MAX].mask;

		ret = write(fd628_fd,write_buffer,sizeof(write_buffer[0])*5);
		mdelay(500);
	}
}

static void *display_thread_handler(void *arg)
{
	//led_test_codes();
	UNUSED(arg);

    led_show_time_loop();

	pthread_exit(NULL);
}



int main(void)
{
	//	pthread_t check_usb_id,check_sd_id;
	pthread_t disp_id,check_dev_id = 0;
	int ret;
    fd628_fd = open(DEV_NAME, O_RDWR);
    if (fd628_fd < 0) {
		perror("open device fd628_fd \r\n");
		exit(1);
	}
	ret = pthread_create(&disp_id, NULL, display_thread_handler, NULL);
	if(ret != 0)
	{
		printf("Create disp_id thread error\n");
		return ret;
	}
	pthread_join(disp_id, NULL);
	pthread_join(check_dev_id, NULL);
	close(fd628_fd);
	return 0;
}
