PROGNAME := fterman

CFLAGS := -Wall -Werror -Wextra -Wno-unused-but-set-parameter
LDFLAGS := 

RM := rm -f
INSTALL := install

OBJECTFILES := $(patsubst src/%.c, src/%.o, $(wildcard src/*.c))

main: CFLAGS += -O2
main: LDFLAGS += -O2
main: $(PROGNAME)

sanitizer: LDFLAGS := -fsanitize=address
sanitizer: debug

debug: CFLAGS += -g
debug: LDFLAGS += -g
debug: $(PROGNAME)

$(PROGNAME):
$(PROGNAME): info build

info:
	@$(info Building with:)
	@$(info CFLAGS := $(CFLAGS))
	@$(info LDFLAGS := $(LDFLAGS))

build: $(OBJECTFILES)
build: link

link: $(OBJECTFILES)
	@$(info LD $(PROGNAME))
	@$(CC) $^ $(LDFLAGS) -o $(PROGNAME)

src/%.o: src/%.c
	@$(info CC $<)
	@$(CC) -c $< $(CFLAGS) -o $@

clean:
	@$(info RM$(OBJECTFILES))
	@$(RM) $(OBJECTFILES)
	@$(info RM $(PROGNAME))
	@$(RM) $(PROGNAME)
	

install:
	@$(info INSTALL $(PROGNAME))
	@$(INSTALL) $(PROGNAME) /bin/
