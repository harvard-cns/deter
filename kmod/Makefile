CONFIG_MODULE_SIG=n

obj-m += deter_recorder.o deter_replayer.o
deter_recorder-objs := recorder.o record_ctrl.o record_ops.o proc_expose.o record_user_share.o mem_util.o logger.o record_shmem.o
deter_replayer-objs := replayer.o replay_ctrl.o replay_ops.o proc_expose.o mem_util.o logger.o

CURRENT_PATH := $(shell pwd)
LINUX_KERNEL_PATH := /usr/src/deter_kernel_4.4.98/
all:
	$(MAKE) -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules
clean:
	rm *.ko
	rm *.o

