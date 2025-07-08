main:
	cc src/main.c -O2 -o fterman -lncurses
debug:
	cc src/main.c -o fterman-g -lncurses -g
cleanmain:
	rm main
cleandebug:
	rm debug
install:
	mv fterman /usr/bin
