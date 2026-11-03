.PHONY: clean install launch s kill astyle dist help test test-unit

CLIBS = -lX11 -lxkbcommon -lXtst
CFLAGS = -Wall -Wextra -Werror -fmax-errors=1 -O4

# Тесты собираются без -Werror и без -O4: предупреждения компилятора не должны
# валить прогон, а libbacktrace нужны символы (-g)
CC = cc
TEST_CFLAGS = -std=gnu17 -g -Wall -Wextra -DERSWITCHER_UNIT
TEST_LIBS = check libbacktrace
TEST_LIBS_LOCAL = -lm -lX11 -lxkbcommon

erswitcher: erswitcher.c main.c	## Компилирует erswitcher
		gcc $(CFLAGS) $(CLIBS) -o erswitcher erswitcher.c

clean:	## Удаляет исходники
		rm -fr erswitcher keyboard tests/unit/test

install:	## Устанавливает erswitcher в ~/
		mkdir -p ~/.local/bin
		mv -f erswitcher ~/.local/bin/erswitcher
		cp -a erswitcher-configurator.tcl ~/.local/bin/erswitcher-configurator.tcl
		if [ ! -e ~/.config/erswitcher.conf ]; then cp -a erswitcher.conf ~/.config/erswitcher.conf; fi
		rm -f ~/.config/autostart/erswitcher.desktop
		rm -f ~/.local/share/applications/erswitcher.desktop

diff:	## Сравнивает конфиги в ~/.config и дистрибутиве
		meld ~/.config/erswitcher.conf erswitcher.conf

install_conf:	## Копирует конфиг из дистрибутива в ~/.config
		cp -a erswitcher.conf ~/.config/erswitcher.conf

sync:	## Копирует конфиг из ~/.config в dist
		cp -a ~/.config/erswitcher.conf erswitcher.conf

launch: kill	## Перезапускает erswitcher из ~/.local/bin
		~/.local/bin/erswitcher &> /dev/null &

s: erswitcher	## Собирает и запускает ./erswitcher
		./erswitcher

kill:	## Убивает процесс erswitcher-а
		killall -9 erswitcher || true

astyle:	## Форматирует код
		astyle `find . -name "*.[ch]"`

dist:	## Собирает дистрибутив (экспериментально)
		autoscan
		mcedit configure.scan
		mv configure.scan configure.in
		autoconf
		./configure

test: test-unit	## Все тесты

# Один бинарник на все наборы: монолит подключается #include-ом из test.h
# (-DERSWITCHER_UNIT), поэтому main() только один
test-unit:	## Юнит-тесты на Check
		cd tests/unit && $(CC) $(TEST_CFLAGS) -rdynamic -o test test.c \
			$$(pkg-config --cflags --libs $(TEST_LIBS)) $(TEST_LIBS_LOCAL)
		cd tests/unit && ./test

help:	## Справка
		@grep -E -h '\s##\s' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'
