obj-m += kfetch_mod_314551128.o

ARCH ?= riscv

CROSS_COMPILE ?= riscv64-linux-gnu-

KDIR ?= $(PWD)/linux-v6.8-devkit

all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean
