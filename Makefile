.PHONY: clean install launch s kill format format-check appimage appimage-clean release dist help test test-unit test-functional

CLIBS = -lX11 -lxkbcommon -lXtst
CFLAGS = -Wall -Wextra -Werror -fmax-errors=1 -O4

# Тесты собираются без -Werror и без -O4: предупреждения компилятора не должны
# валить прогон, а libbacktrace нужны символы (-g)
CC = cc
TEST_CFLAGS = -std=gnu17 -g -Wall -Wextra
TEST_LIBS = check libbacktrace
TEST_LIBS_LOCAL = -lm -lX11 -lxkbcommon

# Приложение — src/app/app.c + main.c плюс чистые модули,
# X11-адаптеры инфраструктуры и presentation.

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
	src/use_cases/translate/translate.c \
	src/use_cases/sendkeys/sendkeys.c \
	src/use_cases/snippets/snippets.c \
	src/use_cases/commands/commands.c \
	src/use_cases/config_reload/config_reload.c
USE_CASE_OBJECTS = $(USE_CASE_SOURCES:.c=.o)

# Инфраструктурные адаптеры. В доменных тестах не участвуют.
INFRASTRUCTURE_SOURCES = \
	src/infrastructure/x11/keyboard.c \
	src/infrastructure/x11/clipboard.c \
	src/infrastructure/x11/events.c \
	src/infrastructure/x11/tray.c \
	src/infrastructure/system/scheduler_time.c \
	src/infrastructure/system/config_store_fs.c \
	src/infrastructure/system/command_runner_fork.c \
	src/infrastructure/system/desktop.c
INFRASTRUCTURE_OBJECTS = $(INFRASTRUCTURE_SOURCES:.c=.o)

# Слой представления: состояние иконки поверх ports/tray.
PRESENTATION_SOURCES = \
	src/presentation/tray_indicator.c
PRESENTATION_OBJECTS = $(PRESENTATION_SOURCES:.c=.o)

APP_OBJECTS = src/app/app.o main.o $(DOMAIN_OBJECTS) $(USE_CASE_OBJECTS) $(INFRASTRUCTURE_OBJECTS) $(PRESENTATION_OBJECTS)
TEST_DOMAIN = tests/unit/test_domain

# AppImage и GitHub Releases. VERSION берётся из README.md, но её можно
# переопределить: make appimage VERSION=7.3
VERSION ?= $(shell sed -n '/^# VERSION$$/{n;n;p;q;}' README.md | tr -d '[:space:]')
APPIMAGE_ARCH ?= $(shell uname -m)
APPIMAGE_NAME ?= erswitcher-$(VERSION)-$(APPIMAGE_ARCH).AppImage
APPIMAGE_STAGE ?= build/AppImage/AppDir
APPIMAGE_OUTDIR ?= dist
APPIMAGE_TOOL ?= appimagetool
GH ?= gh
RELEASE_TAG ?= v$(VERSION)
RELEASE_TITLE ?= erswitcher $(VERSION)

erswitcher: $(APP_OBJECTS)	## Компилирует erswitcher
		gcc $(CFLAGS) $(CLIBS) -o $@ $^

src/app/app.o: src/app/app.c $(wildcard src/app/*.h) $(wildcard src/domain/*/*.h) $(wildcard src/use_cases/*/*.h) $(wildcard src/ports/*.h) $(wildcard src/infrastructure/x11/*.h) $(wildcard src/infrastructure/system/*.h) $(wildcard src/presentation/*.h)
		gcc $(CFLAGS) -c -o $@ src/app/app.c

main.o: main.c
		gcc $(CFLAGS) -c -o $@ main.c

$(DOMAIN_OBJECTS): %.o: %.c $(wildcard src/domain/*/*.h)
		gcc $(CFLAGS) -c -o $@ $<

$(USE_CASE_OBJECTS): %.o: %.c $(wildcard src/domain/*/*.h) $(wildcard src/ports/*.h) $(wildcard src/use_cases/*/*.h)
		gcc $(CFLAGS) -c -o $@ $<

$(INFRASTRUCTURE_OBJECTS): %.o: %.c $(wildcard src/domain/*/*.h) $(wildcard src/ports/*.h) $(wildcard src/infrastructure/x11/*.h) $(wildcard src/infrastructure/system/*.h)
		gcc $(CFLAGS) -c -o $@ $<

$(PRESENTATION_OBJECTS): %.o: %.c $(wildcard src/ports/*.h) $(wildcard src/presentation/*.h)
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

CLANG_FORMAT ?= clang-format

format:	## Форматирует C/H через clang-format
		@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "FAIL: не найден $(CLANG_FORMAT); см. doc/formatting.md" >&2; exit 1; }
		find main.c src tests -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 -r $(CLANG_FORMAT) -i

format-check:	## Проверяет форматирование C/H без изменений
		@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "FAIL: не найден $(CLANG_FORMAT); см. doc/formatting.md" >&2; exit 1; }
		find main.c src tests -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 -r $(CLANG_FORMAT) --dry-run --Werror

appimage: erswitcher	## Собирает AppImage в dist/
		@test -n "$(VERSION)" || { echo "FAIL: VERSION пуст; задайте make appimage VERSION=..." >&2; exit 1; }
		@command -v $(APPIMAGE_TOOL) >/dev/null 2>&1 || { echo "FAIL: не найден $(APPIMAGE_TOOL); см. doc/packaging/appimage.md" >&2; exit 1; }
		rm -rf $(APPIMAGE_STAGE)
		install -d $(APPIMAGE_STAGE)/usr/bin
		install -d $(APPIMAGE_STAGE)/usr/share/applications
		install -d $(APPIMAGE_STAGE)/usr/share/icons/hicolor/256x256/apps
		install -d $(APPIMAGE_STAGE)/usr/share/icons/hicolor/scalable/apps
		install -m 0755 erswitcher $(APPIMAGE_STAGE)/usr/bin/erswitcher
		install -m 0755 erswitcher-configurator.tcl $(APPIMAGE_STAGE)/usr/bin/erswitcher-configurator.tcl
		install -m 0755 packaging/appimage/AppRun $(APPIMAGE_STAGE)/AppRun
		install -m 0644 packaging/appimage/erswitcher.desktop $(APPIMAGE_STAGE)/erswitcher.desktop
		install -m 0644 packaging/appimage/erswitcher.desktop $(APPIMAGE_STAGE)/usr/share/applications/erswitcher.desktop
		install -m 0644 packaging/appimage/erswitcher.png $(APPIMAGE_STAGE)/erswitcher.png
		ln -sf erswitcher.png $(APPIMAGE_STAGE)/.DirIcon
		install -m 0644 packaging/appimage/erswitcher.png $(APPIMAGE_STAGE)/usr/share/icons/hicolor/256x256/apps/erswitcher.png
		install -m 0644 packaging/appimage/erswitcher.svg $(APPIMAGE_STAGE)/usr/share/icons/hicolor/scalable/apps/erswitcher.svg
		install -d $(APPIMAGE_OUTDIR)
		rm -f $(APPIMAGE_OUTDIR)/$(APPIMAGE_NAME)
		APPIMAGE_EXTRACT_AND_RUN=1 ARCH=$(APPIMAGE_ARCH) $(APPIMAGE_TOOL) --no-appstream $(APPIMAGE_STAGE) $(APPIMAGE_OUTDIR)/$(APPIMAGE_NAME)

appimage-clean:	## Удаляет staging-каталог AppImage
		rm -rf build/AppImage

release: appimage	## Собирает AppImage и публикует его в GitHub Releases
		@command -v $(GH) >/dev/null 2>&1 || { echo "FAIL: не найден $(GH); установите GitHub CLI" >&2; exit 1; }
		@$(GH) auth status >/dev/null 2>&1 || { echo "FAIL: gh не авторизован; выполните gh auth login" >&2; exit 1; }
		@if $(GH) release view "$(RELEASE_TAG)" >/dev/null 2>&1; then \
			echo "Обновляю релиз $(RELEASE_TAG)"; \
			$(GH) release upload "$(RELEASE_TAG)" "$(APPIMAGE_OUTDIR)/$(APPIMAGE_NAME)" --clobber; \
		else \
			echo "Создаю релиз $(RELEASE_TAG)"; \
			$(GH) release create "$(RELEASE_TAG)" "$(APPIMAGE_OUTDIR)/$(APPIMAGE_NAME)" --title "$(RELEASE_TITLE)" --generate-notes; \
		fi

dist:	## Собирает дистрибутив (экспериментально)
		autoscan
		mcedit configure.scan
		mv configure.scan configure.in
		autoconf
		./configure

test: test-unit test-functional	## Все тесты

# Один бинарник на все наборы: test.c содержит общий main(), а app.c
# линкуется как обычный объектный файл.
test-unit: src/app/app.o $(DOMAIN_OBJECTS) $(USE_CASE_OBJECTS) $(INFRASTRUCTURE_OBJECTS) $(PRESENTATION_OBJECTS)	## Юнит-тесты на Check
		cd tests/unit && $(CC) $(TEST_CFLAGS) -rdynamic -o test test.c \
			../../src/app/app.o $(addprefix ../../,$(DOMAIN_OBJECTS) $(USE_CASE_OBJECTS) $(INFRASTRUCTURE_OBJECTS) $(PRESENTATION_OBJECTS)) \
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
