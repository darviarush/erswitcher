.PHONY: ex

CLIBS = -lX11 -lxkbcommon -lXtst
CFLAGS = -Wall -Wextra -Werror -fmax-errors=1 -O4


ex: sym2int
		./sym2int > sym2int.csv

sym2int: sym2int.c
		gcc $(CFLAGS) $(CLIBS) -o sym2int sym2int.c
