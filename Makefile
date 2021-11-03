.PHONY: clean install launch s

CLIBS = -lX11 -lxkbcommon -lXtst
CFLAGS = -Wall -Wextra -Werror -fmax-errors=1 -O4

erswitcher: erswitcher.c
		gcc $(CFLAGS) $(CLIBS) -o erswitcher erswitcher.c

keyboard: keyboard.c
		gcc $(CFLAGS) $(CLIBS) -o keyboard keyboard.c

clean:
		rm -fr erswitcher keyboard

install:
		mkdir -p ~/.local/bin
		mv -f erswitcher ~/.local/bin/erswitcher

launch: kill
		~/.local/bin/erswitcher &> /dev/null &

s: erswitcher
		./erswitcher

k: keyboard
		./keyboard > keyboard.csv

kill:
		killall -9 erswitcher || true

dist:
		autoscan
		mcedit configure.scan
		mv configure.scan configure.in
		autoconf
		./configure
