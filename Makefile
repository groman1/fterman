main: 
	cc src/main.c src/settings.c src/xmltools.c src/rawtui.c -O2 -o fterman
debug: 
	cc src/main.c src/settings.c src/xmltools.c src/rawtui.c -o fterman-g -g
sanitize:
	cc src/main.c src/settings.c src/xmltools.c src/rawtui.c -fsanitize=address -o fterman-g -g
install:
	install -d /etc/fterman
	install -m 666 example.conf /etc/fterman/fterman.conf
	mv fterman /usr/bin
install-config:
	install -d /etc/fterman
	install -m 666 example.conf /etc/fterman/fterman.conf
update: 
	mv fterman /usr/bin
uninstall:
	rm -r /etc/fterman
	rm /usr/bin/fterman
