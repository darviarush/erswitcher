CLIBS = -lX11 -lxkbcommon -lXtst
CFLAGS = -Wall -Wextra -Werror -fmax-errors=1 -O4


keyboard: keyboard.c
		gcc $(CFLAGS) $(CLIBS) -o keyboard keyboard.c

k: keyboard
		./keyboard > keyboard.csv
