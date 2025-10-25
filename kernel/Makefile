# ===========================================
# Makefile for SCULL module (Eduardo Naranjo)
# Compatible with modern kernels (5.x / 6.x)
# ===========================================

.PHONY: default clean install

ifneq ($(KERNELRELEASE),)
    # When invoked from the kernel build system
    obj-m := nxp_simtemp.o
else
    # Kernel build directory (auto-detected)
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    # Current working directory
    PWD := $(shell pwd)

# Default target: build the module
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

# Clean generated files
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

# Install module to the system (optional)
install:
	sudo cp scull.ko /lib/modules/$(shell uname -r)/
	sudo depmod -a
endif

