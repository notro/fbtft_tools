
obj-m := rpi_power_switch.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
ARCH := $(shell grep -Po '^Hardware\s*:\s*\K[[:alnum:]]{7}' /proc/cpuinfo)

ifeq ($(ARCH),BCM2709)
	PITYPE :=  CFLAGS=-DCONFIG_ARCH_BCM2709=y
endif


all:
	$(MAKE) -C $(KDIR) M=$(PWD) $(PITYPE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

