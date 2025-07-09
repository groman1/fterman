alphabetic:
	cc src/main.c -O2 -o fterman -D ALPHABETIC -lncurses
lastmodified:
	cc src/main.c -O2 -o fterman -D LASTMODIFIED -lncurses
lastaccessed:
	cc src/main.c -O2 -o fterman -D LASTACCESSED -lncurses
size:
	cc src/main.c -O2 -o fterman -D SIZE -lncurses
debug:
	cc src/main.c -o fterman-g -lncurses -g
cleanmain:
	rm main
cleandebug:
	rm debug
install:
	mv fterman /usr/bin
