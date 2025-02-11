TARGET_MODULE:=rotenc

ifeq ($(KERNELRELEASE),)
	KERNEL_BUILDSYSTEM_DIR:=/lib/modules/$(shell uname -r)/build
	PWD:=$(shell pwd)

all : 
	$(MAKE) -C $(KERNEL_BUILDSYSTEM_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_BUILDSYSTEM_DIR) M=$(PWD) clean

load:
	insmod ./$(TARGET_MODULE).ko

unload:
	rmmod ./$(TARGET_MODULE).ko

else
	$(TARGET_MODULE)-objs := main.o
	obj-m := $(TARGET_MODULE).o

endif