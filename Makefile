CROSS_COMPILE = arm-linux-gnueabihf-
AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
CC = $(CROSS_COMPILE)gcc
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm

STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump

export AS LD CC CPP AR NM
export STRIP OBJCOPY OBJDUMP

CFLAGS := -Wall  -O2 -g
CFLAGS += -I $(shell pwd)/include

LDFLAGS := -lm -lfreetype -lpthread    -lts  -ljpeg

export CFLAGS LDFLAGS

TOPDIR := $(shell pwd)
export TOPDIR

TARGET :=digitpic

obj-y += main.o
obj-y += display/
obj-y += debug/
obj-y += fonts/
obj-y += render/
obj-y += page/
obj-y += file/
obj-y += input/
obj-y += encoding/


all :
	make -C ./ -f $(TOPDIR)/Makefile.build
	$(CC)  -o $(TARGET) built-in.o $(LDFLAGS)

clean:
	rm -f $(shell find -name "*.o")
	rm -f $(TARGET)

distclean:
	rm -f $(shell find -name "*.o")
	rm -f $(shell find -name "*.d")
	rm -f $(TARGET)

copy:
	cp $(TARGET) /home/chenjiawen/workdir