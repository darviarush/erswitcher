.PHONY: clean install launch s

CLIBS = -lX11 -lxkbcommon -lXtst
CFLAGS = -Wall -Wextra -Werror -fmax-errors=1 -O4

erswitcher: erswitcher.c
	gcc $(CFLAGS) $(CLIBS) -o erswitcher erswitcher.c

clean:
	rm -fr erswitcher

install:
	mkdir -p ~/.local/bin
	cp erswitcher ~/.local/bin

launch:
	~/.local/bin/erswitcher &> /dev/null &

s: erswitcher
	./erswitcher
