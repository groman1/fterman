alphabetic:
	cc src/main.c src/settings.c src/xmltools.c -O2 -o fterman -D ALPHABETIC -lncurses
lastmodified:
	cc src/main.c src/settings.c src/xmltools.c -O2 -o fterman -D LASTMODIFIED -lncurses
lastaccessed:
	cc src/main.c src/settings.c src/xmltools.c -O2 -o fterman -D LASTACCESSED -lncurses
size:
	cc src/main.c src/settings.c src/xmltools.c -O2 -o fterman -D SIZE -lncurses
debug:
	cc src/main.c src/settings.c src/xmltools.c -D SIZE -o fterman-g -lncurses -g
cleanmain:
	rm main
cleandebug:
	rm debug
install:
	install -d /etc/fterman
	install -m 666 example.conf /etc/fterman/fterman.conf
	mv fterman /usr/bin
uninstall:
	rm -r /etc/fterman
	rm /usr/bin/fterman
