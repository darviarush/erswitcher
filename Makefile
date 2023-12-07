.PHONY: clean install launch s k

CLIBS = -lX11 -lxkbcommon -lXtst
CFLAGS = -Wall -Wextra -Werror -fmax-errors=1 -O4

erswitcher: erswitcher.c
		gcc $(CFLAGS) $(CLIBS) -o erswitcher erswitcher.c

clean:
		rm -fr erswitcher keyboard

install:
		mkdir -p ~/.local/bin
		mv -f erswitcher ~/.local/bin/erswitcher
		cp -a erswitcher-configurator.tcl ~/.local/bin/erswitcher-configurator.tcl
		if [ ! -e ~/.config/erswitcher.conf ]; then cp -a erswitcher.conf ~/.config/erswitcher.conf; fi

diff:
		meld ~/.config/erswitcher.conf erswitcher.conf

install_conf:
		cp -a erswitcher.conf ~/.config/erswitcher.conf

sync:
		cp -a ~/.config/erswitcher.conf erswitcher.conf

launch: kill
		~/.local/bin/erswitcher &> /dev/null &

s: erswitcher
		./erswitcher

kill:
		killall -9 erswitcher || true

astyle:
		astyle `find . -name "*.[ch]"`

dist:
		autoscan
		mcedit configure.scan
		mv configure.scan configure.in
		autoconf
		./configure
