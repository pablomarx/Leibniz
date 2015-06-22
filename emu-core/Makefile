CC = gcc
CFLAGS = -I. -g -std=c99 
LDFLAGS = -g
LD = $(CC)

all:	newton

newton:	arm.o copr14.o copr15.o disasm.o mmu.o newton.o opcodes.o linenoise.o runt.o main.o monitor.o hexdump.o lcd_sharp.o lcd_squirt.o
	$(LD) $(LDFLAGS) -o $@ $^

%.o:	%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

clean:
	rm -f *.o newton
