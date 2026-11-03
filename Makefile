.PHONY: clean install launch s kill astyle dist help test test-unit test-functional

CLIBS = -lX11 -lxkbcommon -lXtst
CFLAGS = -Wall -Wextra -Werror -fmax-errors=1 -O4

# Тесты собираются без -Werror и без -O4: предупреждения компилятора не должны
# валить прогон, а libbacktrace нужны символы (-g)
CC = cc
TEST_CFLAGS = -std=gnu17 -g -Wall -Wextra -DERSWITCHER_UNIT
TEST_LIBS = check libbacktrace
TEST_LIBS_LOCAL = -lm -lX11 -lxkbcommon

# Приложение пока — erswitcher.c (подключает main.c) плюс чистые модули
# и X11-адаптеры инфраструктуры.

# Чистые доменные модули: без X11, файлов и процессов.
DOMAIN_SOURCES = \
	src/domain/text/text.c \
	src/domain/input_buffer/input_buffer.c \
	src/domain/case_invert/case_invert.c \
	src/domain/compose/compose.c \
	src/domain/command_template/command_template.c \
	src/domain/config/config.c
DOMAIN_OBJECTS = $(DOMAIN_SOURCES:.c=.o)

# Сценарии приложения: зависят от domain и ports, но не от X11.
USE_CASE_SOURCES = \
	src/use_cases/input/input.c \
	src/use_cases/translate/translate.c
USE_CASE_OBJECTS = $(USE_CASE_SOURCES:.c=.o)

# Инфраструктурные адаптеры. В доменных тестах не участвуют.
INFRASTRUCTURE_SOURCES = \
	src/infrastructure/x11/keyboard.c \
	src/infrastructure/x11/clipboard.c \
	src/infrastructure/x11/events.c \
	src/infrastructure/system/scheduler_time.c
INFRASTRUCTURE_OBJECTS = $(INFRASTRUCTURE_SOURCES:.c=.o)

APP_OBJECTS = erswitcher.o $(DOMAIN_OBJECTS) $(USE_CASE_OBJECTS) $(INFRASTRUCTURE_OBJECTS)
TEST_DOMAIN = tests/unit/test_domain

erswitcher: $(APP_OBJECTS)	## Компилирует erswitcher
		gcc $(CFLAGS) $(CLIBS) -o $@ $^

erswitcher.o: erswitcher.c main.c $(wildcard src/domain/*/*.h) $(wildcard src/use_cases/*/*.h) $(wildcard src/ports/*.h) $(wildcard src/infrastructure/x11/*.h) $(wildcard src/infrastructure/system/*.h)
		gcc $(CFLAGS) -c -o $@ erswitcher.c

$(DOMAIN_OBJECTS): %.o: %.c $(wildcard src/domain/*/*.h)
		gcc $(CFLAGS) -c -o $@ $<

$(USE_CASE_OBJECTS): %.o: %.c $(wildcard src/domain/*/*.h) $(wildcard src/ports/*.h) $(wildcard src/use_cases/*/*.h)
		gcc $(CFLAGS) -c -o $@ $<

$(INFRASTRUCTURE_OBJECTS): %.o: %.c $(wildcard src/domain/*/*.h) $(wildcard src/ports/*.h) $(wildcard src/infrastructure/x11/*.h) $(wildcard src/infrastructure/system/*.h)
		gcc $(CFLAGS) -c -o $@ $<

clean:	## Удаляет исходники
		rm -fr erswitcher keyboard $(APP_OBJECTS) $(DOMAIN_OBJECTS) tests/unit/test $(TEST_DOMAIN)

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
test-unit: $(DOMAIN_OBJECTS) $(USE_CASE_OBJECTS) $(INFRASTRUCTURE_OBJECTS)	## Юнит-тесты на Check
		cd tests/unit && $(CC) $(TEST_CFLAGS) -rdynamic -o test test.c \
			$(addprefix ../../,$(DOMAIN_OBJECTS) $(USE_CASE_OBJECTS) $(INFRASTRUCTURE_OBJECTS)) \
			$$(pkg-config --cflags --libs $(TEST_LIBS)) $(TEST_LIBS_LOCAL)
		cd tests/unit && ./test
		$(CC) $(TEST_CFLAGS) -o $(TEST_DOMAIN) tests/unit/test_domain.c \
			$(DOMAIN_OBJECTS) $$(pkg-config --cflags --libs check)
		./$(TEST_DOMAIN)

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
