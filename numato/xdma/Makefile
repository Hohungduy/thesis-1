SHELL = /bin/bash
ifneq ($(xvc_bar_num),)
	XVC_FLAGS += -D__XVC_BAR_NUM__=$(xvc_bar_num)
endif

ifneq ($(xvc_bar_offset),)
	XVC_FLAGS += -D__XVC_BAR_OFFSET__=$(xvc_bar_offset)
endif

$(warning XVC_FLAGS: $(XVC_FLAGS).)

topdir := $(shell cd $(src)/.. && pwd)

TARGET_MODULE:=xdma

EXTRA_CFLAGS := -I$(topdir)/include $(XVC_FLAGS)
# EXTRA_CFLAGS += -D__LIBXDMA_DEBUG__
#EXTRA_CFLAGS += -DINTERNAL_TESTING

ifneq ($(KERNELRELEASE),)
	$(TARGET_MODULE)-objs := libxdma.o xdma_cdev.o \
		cdev_ctrl.o cdev_events.o cdev_sgdma.o cdev_xvc.o \
		cdev_bypass.o xdma_mod.o xdma_thread.o xdma_crypto.o \
		xdma_region.o 
	huy_crypto-objs := cipher.o mycrypto.o
	obj-m := $(TARGET_MODULE).o example_use.o example_use_2.o \
		example_use_3.o example_use_4.o example_use_5.o \
		example_use_6.o example_use_7.o example_use_8.o \
	testcrypto.o huy_crypto.o 

else
	BUILDSYSTEM_DIR:=/lib/modules/$(shell uname -r)/build
	PWD:=$(shell pwd)
all :
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules
	mkdir -p ../bin
	cp -f *.ko ../bin

clean:
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) clean
	rm -r ../bin

install: all
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules_install

endif
