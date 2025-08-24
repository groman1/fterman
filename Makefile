main: alphabetic
size: 
	cc src/main.c src/settings.c src/xmltools.c src/rawtui.c -O2 -o fterman -D SIZE
alphabetic: 
	cc src/main.c src/settings.c src/xmltools.c src/rawtui.c -O2 -o fterman -D ALPHABETIC
lastmodified: 
	cc src/main.c src/settings.c src/xmltools.c src/rawtui.c -O2 -o fterman -D LASTMODIFIED
lastaccessed: 
	cc src/main.c src/settings.c src/xmltools.c src/rawtui.c -O2 -o fterman -D LASTACCESSED
debug: 
	cc src/main.c src/settings.c src/xmltools.c src/rawtui.c -D ALPHABETIC -o fterman-g -g
cleanmain: 
	rm main
cleandebug: 
	rm debug
install:
	install -d /etc/fterman
	install -m 666 example.conf /etc/fterman/fterman.conf
	mv fterman /usr/bin
update: 
	mv fterman /usr/bin
uninstall:
	rm -r /etc/fterman
	rm /usr/bin/fterman
