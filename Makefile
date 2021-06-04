.PHONY: start clean

OBJECTS = $(patsubst src/%.c,o/%.o,$(wildcard src/*.c))
CFLAGS = -lX11 -lxkbcommon -lXtst

start: erswitcher
	./erswitcher

o/:
	mkdir o/

o/%.o: src/%.c src/%.h
	gcc -c $< -o $@

erswitcher:	o/ $(OBJECTS)
	gcc $(CFLAGS) -o erswitcher $(OBJECTS)

# systray: systray.c
# 	gcc -lX11  -o $@ $^

clean:
	rm -fr erswitcher o/
