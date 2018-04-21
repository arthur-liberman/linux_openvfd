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
		obj-m := aml_fd628.o
		aml_fd628-objs += protocols/i2c.o
		aml_fd628-objs += protocols/spi_3w.o
		aml_fd628-objs += controllers/dummy.o
		aml_fd628-objs += controllers/seg7_ctrl.o
		aml_fd628-objs += controllers/fd628.o
		aml_fd628-objs += controllers/fd650.o
		aml_fd628-objs += controllers/hd44780.o
		aml_fd628-objs += controllers/ssd1306.o
		aml_fd628-objs += le_vfd_drv.o
endif
