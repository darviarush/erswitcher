.PHONY: start clean install s

OBJECTS = $(patsubst src/%.c,o/%.o,$(wildcard src/*.c))
CLIBS = -lX11 -lxkbcommon -lXtst
CFLAGS = -Wall -Wextra -Werror -fmax-errors=1 -O4

start: erswitcher
	./erswitcher

o/:
	mkdir o/

o/%.o: src/%.c src/%.h
	gcc $(CFLAGS) -c $< -o $@

erswitcher:	o/ $(OBJECTS)
	gcc $(CLIBS) -o erswitcher $(OBJECTS)

clean:
	rm -fr erswitcher o/

install:
	mkdir -p ~/.local/bin
	cp erswitcher ~/.local/bin
	if [ ! -e ~/.xinitrc ]; then cp /etc/X11/xinit/xinitrc ~/.xinitrc; fi
	grep "exec ~/.local/bin/erswitcher &" ~/.xinitrc > /dev/null || echo "exec ~/.local/bin/erswitcher &" >> ~/.xinitrc


ers: erswitcher.c
	gcc $(CFLAGS) $(CLIBS) -o ers erswitcher.c

s: ers
	./ers
