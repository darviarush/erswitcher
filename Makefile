
systray: systray.c
	gcc -lX11 -o $@ $^


erswitcher:	erswitcher.c
	gcc -lX11 -o erswitcher erswitcher.c
