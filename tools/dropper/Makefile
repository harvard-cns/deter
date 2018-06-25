CONFIG_MODULE_SIG=n

obj-m += dropper.o

CURRENT_PATH := $(shell pwd)
LINUX_KERNEL_PATH := /usr/src/linux-source-4.4.0
all:
	$(MAKE) -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules
clean:
	rm *.ko
	rm *.o
