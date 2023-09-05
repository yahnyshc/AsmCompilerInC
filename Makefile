all: emulator

emulator: emulator.c emulator.h main.c
	gcc -g emulator.c main.c -o emulator

emulator-week24.zip: emulator.c Makefile emulator.h main.c ip_csm.asm eth_mult.asm simple_add.asm simple_loop.asm
	zip scc150_template_week24.zip emulator.c emulator.h Makefile main.c *.asm		

clean:
	rm -r *.o emulator *.dSYM
