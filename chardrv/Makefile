# Makefile  makefile of our first driver
#  
# if KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq (${KERNELRELEASE},)
	obj-m += ofd.o
	obj-m += chardev.o
	obj-m += chardev-1.o
# Otherwise we were called directly from the command line.
# Invoke the kernel build system.
 else
	KERNEL_SOURCE := /usr/src/kernels/2.6.32-431.5.1.el6.x86_64
	PWD := $(shell pwd)
default:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} modules
#              
clean:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} clean
endif
