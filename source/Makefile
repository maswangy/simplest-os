#Makefile:
#Copyright (C) 2009  david leels <davidontech@gmail.com>

#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation, either version 3 of the License, or
#(at your option) any later version.

#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.

#You should have received a copy of the GNU General Public License
#along with this program.  If not, see http://www.gnu.org/licenses/.
#

PRJ 		  = simplest-os
CROSS_COMPILE = arm-linux-
CC			  = $(CROSS_COMPILE)gcc
LD      	  = $(CROSS_COMPILE)ld
OBJCOPY 	  = $(CROSS_COMPILE)objcopy
OBJDUMP 	  = $(CROSS_COMPILE)objdump

CFLAGS= -O2 -g
ASFLAGS= -O2 -g
LDFLAGS=-T$(PRJ).lds 

OBJS=init.o lib_arm.o lowlevel_init.o start.o boot.o test.o abnormal.o mmu.o print.o interrupt.o mem.o driver.o ramdisk.o romfs.o fs.o exec.o syscall.o proc.o board.o serial.o led_on.o
.c.o:
	$(CC) $(CFLAGS) -c $<
.S.o:
	$(CC) $(ASFLAGS) -c $<

$(PRJ):$(OBJS)
	$(CC) -static -nostartfiles -nostdlib $(LDFLAGS) $? -o $@ -lgcc
	$(OBJDUMP) -D $(PRJ) > $(PRJ).dis
	$(OBJCOPY) -O binary $@ $(PRJ).bin

clean:
	rm *.o $(PRJ) $(PRJ).bin $(PRJ).dis -f
