OBJECTFILES := src/main.o src/settings.o src/xmltools.o src/rawtui.o
CFLAGS := -Wall -Wextra -Werror -Wno-unused-but-set-parameter
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

install:
	$(INSTALL) fterman ${PREFIX}/bin/fterman
