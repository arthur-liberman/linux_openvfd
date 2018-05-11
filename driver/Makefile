ifeq ($(KERNELRELEASE),)
		PWD = $(shell pwd)
	        KERNELDIR = ../../../
modules:
	        $(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules

modules_install:
	        $(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules_install
			                                  
clean:                    
	        rm -rf  *.o *.ko .tmp_versions *.mod.c modules.order  Module.symvers ssd253x-ts.* 
else                      
		obj-m := open_vfd.o
		open_vfd-objs += protocols/i2c.o
		open_vfd-objs += protocols/spi_3w.o
		open_vfd-objs += controllers/dummy.o
		open_vfd-objs += controllers/seg7_ctrl.o
		open_vfd-objs += controllers/fd628.o
		open_vfd-objs += controllers/fd650.o
		open_vfd-objs += controllers/hd44780.o
		open_vfd-objs += controllers/ssd1306.o
		open_vfd-objs += open_vfd_drv.o
endif
