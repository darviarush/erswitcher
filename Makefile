.PHONY: start


start: erswitcher
	./erswitcher

# systray: systray.c
# 	gcc -lX11  -o $@ $^

erswitcher:	erswitcher.c
	gcc -lX11 -lxkbcommon -o erswitcher erswitcher.c
