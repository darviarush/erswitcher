.PHONY: start


start: erswitcher
	./erswitcher

# systray: systray.c
# 	gcc -lX11  -o $@ $^

erswitcher:	src/erswitcher.c src/keyboard.c src/keyboard.h
	gcc -lX11 -lxkbcommon -o erswitcher src/keyboard.c src/erswitcher.c
