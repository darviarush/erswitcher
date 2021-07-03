.PHONY: clean install launch s

CLIBS = -lX11 -lxkbcommon -lXtst
CFLAGS = -Wall -Wextra -Werror -fmax-errors=1 -O4

erswitcher: erswitcher.c
	gcc $(CFLAGS) $(CLIBS) -o erswitcher erswitcher.c

clean:
	rm -fr erswitcher

install:
	mkdir -p ~/.local/bin
	mv -f erswitcher ~/.local/bin/erswitcher

launch:
	killall -9 erswitcher
	~/.local/bin/erswitcher &> /dev/null &

s: erswitcher
	./erswitcher

dist:
	autoscan
	mcedit configure.scan
	mv configure.scan configure.in
	autoconf
	./configure
