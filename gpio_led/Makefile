obj-m := gpio_led.o  
KERNELDIR := /home/yxie/android/SRC/180510_H9380V30_STD_Rv5856/kernel_imx
PWD :=$(shell pwd)                                                   
ARCH=arm  
CROSS_COMPILE= /home/yxie/android/SRC/180510_H9380V30_STD_Rv5856/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi-  
CC=$(CROSS_COMPILE)gcc  
LD=$(CROSS_COMPILE)ld  
CFLAGS_MODULE=-fno-pic  
modules:     
	make -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules    
clean:    
	rm -rf *.o* *.ko* *.mod* *.order *.symvers .*.ko* .*.mod* .*.o*
