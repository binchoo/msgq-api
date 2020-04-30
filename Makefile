obj-m := cdev_ku_ipc.o
dev-name := ku_ipc
run := app.c

KERNELDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

node:
	mknod /dev/$(dev-name) c $(shell awk '$$2=="$(dev-name)" {print $$1}' /proc/devices) 0

test: $(run:.c=) 
	./$(run:.c=) 

$(run:.c=): $(run)
	gcc -o $(run:.c=) $(run)

