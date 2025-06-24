CC = psp-gcc
ELF = main
OBJS = main.o
PSPSDK = $(shell psp-config --pspsdk-path)
CFLAGS = -I. -I$(PSPSDK)/include -g0 -O0 -Wall -D_PSP_FW_VERSION=660
LDFLAGS = -lc -lpspctrl -lpspuser -lpspkernel -lpspdisplay -lpsprtc

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
	$(CC) $(CFLAGS) -L. -L$(PSPSDK)/lib $^ $(LDFLAGS) -o $@

dma.o: dma.s
	$(CC) -I. -I$(PSPSDK)/include -c -x assembler-with-cpp $<
    
%o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(ELF).elf $(OBJS) PARAM.SFO EBOOT.PBP
