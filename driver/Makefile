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
endif
