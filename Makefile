OBJECTFILES := src/main.o src/settings.o src/xmltools.o src/rawtui.o
CFLAGS := -Wall -Wextra -Werror -Wno-error=unused-but-set-parameter
RM := rm -f
INSTALL := install

main: CFLAGS += -O2
main: fterman

fterman: $(OBJECTFILES)
	$(CC) $(LDFLAGS) $^ -o $@

clean: 
	$(RM) $(OBJECTFILES) fterman

src/%.o: src/%.c
	$(CC) -c $< ${CFLAGS} -o $@ 

debug: CFLAGS += -g
debug: fterman

sanitize: LDFLAGS := -fsanitize=address
sanitize: CFLAGS += -g
sanitize: fterman

install: install-config
	$(INSTALL) fterman /usr/bin/fterman

install-config:
	$(INSTALL) -d /etc/fterman
	$(INSTALL) -m 666 example.conf /etc/fterman/fterman.conf
