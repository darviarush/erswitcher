.PHONY: clean install launch s kill astyle dist help test test-unit test-functional

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

test: test-unit test-functional	## Все тесты

# Один бинарник на все наборы: монолит подключается #include-ом из test.h
# (-DERSWITCHER_UNIT), поэтому main() только один
test-unit:	## Юнит-тесты на Check
		cd tests/unit && $(CC) $(TEST_CFLAGS) -rdynamic -o test test.c \
			$$(pkg-config --cflags --libs $(TEST_LIBS)) $(TEST_LIBS_LOCAL)
		cd tests/unit && ./test

# Окно конфигуратора требует X11. Без DISPLAY пробуем xvfb-run: в его
# виртуальном фреймбуфере Tk создаёт настоящее окно, а event generate
# доставляет в виджеты синтетические события. Если нет ни дисплея, ни
# xvfb-run, пробуем Tk напрямую (Aqua на macOS). Если и это не выйдет —
# это ОШИБКА, а не пропуск: зелёный прогон не должен скрывать, что
# функциональные тесты не выполнялись. Юнит-тесты от X11 не зависят
# и запускаются отдельно через `make test-unit`.
test-functional:	## Функциональные тесты окна конфигуратора (Tcl/Tk)
		@if ! command -v tclsh >/dev/null 2>&1; then \
			echo "FAIL: нет tclsh — функциональные тесты не выполнены" >&2; \
			exit 1; \
		elif [ -n "$$DISPLAY" ]; then \
			cd tests/functional && ./run.tcl; \
		elif command -v xvfb-run >/dev/null 2>&1; then \
			cd tests/functional && xvfb-run -a ./run.tcl; \
		else \
			cd tests/functional && ./run.tcl; \
		fi

help:	## Справка
		@grep -E -h '\s##\s' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'
