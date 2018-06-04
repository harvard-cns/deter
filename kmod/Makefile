CONFIG_MODULE_SIG=n

obj-m += derand.o
derand-objs := main.o derand_ctrl.o derand_ops.o proc_expose.o derand_user_share.o

CURRENT_PATH := $(shell pwd)
LINUX_KERNEL_PATH := /usr/src/linux-source-4.4.0
all:
	$(MAKE) -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules
clean:
	rm *.ko
	rm *.o

