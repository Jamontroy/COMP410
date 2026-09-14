# Kbuild makefile -- runs INSIDE the QEMU guest, against kernel headers.
# On a machine with no Linux headers this cannot build; that is expected.
obj-m += procstat.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
