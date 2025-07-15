CC = psp-gcc
ELF = main
OBJS = main.o

PSPSDK = $(shell psp-config --pspsdk-path)
PSPDEV = $(shell psp-config --pspdev-path)
INCDIR = -I. -I$(PSPDEV)/psp/include -I$(PSPSDK)/include
LIBDIR = -L. -L$(PSPDEV)/psp/lib -L$(PSPSDK)/lib

CFLAGS = $(INCDIR) -g0 -O0 -Wall -D_PSP_FW_VERSION=660
LDFLAGS = $(LIBDIR) -lc -lpspctrl -lpspuser -lpspkernel -lpspdisplay -lpsprtc

all: EBOOT.PBP

EBOOT.PBP: $(ELF).elf
	psp-fixup-imports $(ELF).elf
	mksfo 'Trixlite' PARAM.SFO
	psp-strip $(ELF).elf -o $(ELF)_strip.elf
	pack-pbp EBOOT.PBP PARAM.SFO NULL \
	NULL NULL NULL \
	NULL $(ELF)_strip.elf NULL
	rm -f $(ELF)_strip.elf
    
$(ELF).elf: $(OBJS) dma.o
	$(CC) $(CFLAGS) -L. $^ $(LDFLAGS) -o $@

dma.o: dma.s
	$(CC) -I. $(CFLAGS) -c -x assembler-with-cpp $<

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(ELF).elf $(OBJS) PARAM.SFO EBOOT.PBP
