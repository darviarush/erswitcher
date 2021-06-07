.PHONY: start clean

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

# systray: systray.c
# 	gcc -lX11  -o $@ $^

clean:
	rm -fr erswitcher o/
