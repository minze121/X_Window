
LINUX_NUM:=$(shell uname -r)
KDIR:=/home/devel/E5_config/kernel/linux-3.8.13

obj-m:=vts.o
vts-objs:=vts-drv.o

PWD:=$(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

