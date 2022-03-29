.PHONY: s

CLIBS = -lX11 -lxkbcommon -lXtst
CFLAGS = -Wall -Wextra -Werror -fmax-errors=1 -O4

input: input.c
		gcc $(CFLAGS) $(CLIBS) -o input input.c

s: input
	./input
