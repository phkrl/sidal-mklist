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
	mkdir -p $(DESTDIR)$(MANPREFIX)/man8
	sed "s/VERSION/$(VERSION)/g" < sidal-mklist.8 > $(DESTDIR)$(MANPREFIX)/man8/sidal-mklist.8
	chmod 644 $(DESTDIR)$(MANPREFIX)/man8/sidal-mklist.8

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/sidal-mklist
	rm -f $(DESTDIR)$(MANPREFIX)/man8/sidal-mklist.8

.PHONY: all options clean install uninstall
