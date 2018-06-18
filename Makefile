include config.mk

SRC = utils.c sidal-mklist.c
OBJ = ${SRC:.c=.o}

all: options sidal-mklist

options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

sidal-mklist: ${OBJ}
	${CC} -o $@ sidal-mklist.o utils.o

clean:
	rm -f sidal-mklist

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f sidal-mklist $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/sidal-mklist

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/sidal-mklist

.PHONY: all options clean install uninstall
