#/******************************************************************************
# *    Copyright (c) 2009-2012 by Hisi.
# *    All rights reserved.
#******************************************************************************/

################################################################################

PWD      := $(shell pwd)

CC       := $(CROSS_COMPILE)gcc
AR       := $(CROSS_COMPILE)ar
LD       := $(CROSS_COMPILE)ld
OBJCOPY  := $(CROSS_COMPILE)objcopy

################################################################################
BOOT        := ddr_training
TEXTBASE    := 0x04010C00
STACK_POINT := 0x04010C00
START       := start.o
COBJS       += ddr_dataeye_training.o ddrphy_training.o ddrphy_training_func.o
DEPS        := $(COBJS:.o=.d) $(START:.o=.d)
SSRC        := start.S ddrphy_training.c ddrphy_training_func.S ddr_dataeye_training.c

CFLAGS   := -Os -fno-strict-aliasing -fno-common -ffixed-r8 \
	-DTEXT_BASE=$(TEXTBASE) -DSTACK_POINT=$(STACK_POINT) \
	-fno-builtin -ffreestanding -I./ \
	-pipe -D__ARM__ -marm  -mabi=aapcs-linux  \
	-mno-thumb-interwork -march=armv7-a

.PHONY: $(BOOT).bin
all: $(BOOT).bin

$(BOOT).bin: $(BOOT).elf
	$(OBJCOPY) -O srec $(PWD)/$(BOOT).elf $(BOOT).srec
	$(OBJCOPY) --gap-fill=0xff -O binary $(PWD)/$(BOOT).elf $@

$(BOOT).elf: $(START) $(COBJS) boot.lds
	$(LD) -Bstatic -T boot.lds -Ttext $(TEXTBASE) $(START) \
		$(COBJS) -Map $(BOOT).map -o $@

.PHONY: clean
clean:
	@rm -rf *.o
	@rm -rf *.d
	@rm -rf ddr_training.bin
	@rm -rf ddr_training.elf
	@rm -rf ddr_training.srec
	@rm -rf ddr_training.map

%.o : %.S
	$(CC) -D__ASSEMBLY__ $(CFLAGS) -o $@ -c $*.S

%.o : %.c
	$(CC) $(CFLAGS) -Wall -Wstrict-prototypes -fno-stack-protector \
		-o $@ -c $*.c

ifneq ("$(MAKECMDGOALS)","clean")
sinclude $(DEPS)
endif

%.d : %.c
	set -e; $(CC) $(CFLAGS) -MM $< | sed 's,$*.o:,$*.o $*.d:,g' > $@

%.d : %.S
	set -e; $(CC) $(CFLAGS) -MM $< | sed 's,$*.o:,$*.o $*.d:,g' > $@

