CC = gcc
CFLAGS = -I. -g -std=c99 -DHOST_WORDS_BIGENDIAN=1 
LDFLAGS = -g
LD = $(CC)

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
SDLLIBS = -F/Library/Frameworks -framework SDL -lSDLMain -framework Cocoa 
CFLAGS = -F/Library/Frameworks 
else
SDLLIBS = -lSDL
endif


OBJS := arm.o \
		copr14.o \
		copr15.o \
		disasm.o \
		mmu.o \
		newton.o \
		opcodes.o \
		memory.o \
		fpa.o \
		linenoise.o \
		runt.o \
		pcmcia.o \
		hexdump.o \
		lcd_sharp.o \
		lcd_squirt.o \
		double_cpdo.o \
		extended_cpdo.o \
		fpa11.o \
		fpa11_cpdo.o \
		fpa11_cpdt.o \
		fpa11_cprt.o \
		fpopcode.o \
		single_cpdo.o \
		softfloat.o \

all:	newton

newton:	$(OBJS) main.o monitor.o
	$(LD) $(LDFLAGS) -o $@ $^

sdlnewton:	$(OBJS) sdlnewton.o 
	$(LD) $(LDFLAGS) $(SDLLIBS) -o $@ $^

%.o:	%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

clean:
	rm -f *.o newton
