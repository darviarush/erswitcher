.PHONY: clean install launch s kill format format-check check-deps cppcheck memcheck valgrind coverage coverage-build coverage-run coverage-text coverage-html coverage-clean fuzz fuzz-check fuzz-build fuzz-all fuzz-clean test-build test-sanitize appimage appimage-clean release dist help test test-unit test-functional

CLIBS = -lX11 -lxkbcommon -lXtst
CFLAGS = -Wall -Wextra -Werror -fmax-errors=1 -O4 -fanalyzer

# make s всегда запускает erswitcher с ERSWITCHER_DEBUG=1.
# Для ручного запуска: ERSWITCHER_DEBUG=1 ./erswitcher. На сборку это не влияет
ERSWITCHER_DEBUG ?= 0
export ERSWITCHER_DEBUG

# Тесты собираются с -Werror, AddressSanitizer и отладочными символами.
# Criterion даёт цветной вывод и стектрейсы падений
CC = cc
TEST_SANITIZE ?= -fsanitize=address
TEST_CFLAGS = -std=gnu17 -g -Wall -Wextra -Werror $(TEST_SANITIZE)
TEST_LDFLAGS = $(TEST_SANITIZE)
TEST_LIBS_LOCAL = -lm -lX11 -lxkbcommon
TEST_APP_SOURCES = src/app/app.c $(DOMAIN_SOURCES) $(USE_CASE_SOURCES) $(INFRASTRUCTURE_SOURCES) $(PRESENTATION_SOURCES)

# Приложение — src/app/app.c + main.c плюс чистые модули,
# X11-адаптеры инфраструктуры и presentation.

# Чистые доменные модули: без X11, файлов и процессов.
DOMAIN_SOURCES = \
	src/domain/text/text.c \
	src/domain/input-buffer/input-buffer.c \
	src/domain/case-invert/case-invert.c \
	src/domain/compose/compose.c \
	src/domain/command-template/command-template.c \
	src/domain/config/config.c
DOMAIN_OBJECTS = $(DOMAIN_SOURCES:.c=.o)

# Сценарии приложения: зависят от domain и ports, но не от X11.
USE_CASE_SOURCES = \
	src/use-cases/input/input.c \
	src/use-cases/translate/translate.c \
	src/use-cases/sendkeys/sendkeys.c \
	src/use-cases/snippets/snippets.c \
	src/use-cases/commands/commands.c \
	src/use-cases/config-reload/config-reload.c
USE_CASE_OBJECTS = $(USE_CASE_SOURCES:.c=.o)

# Инфраструктурные адаптеры. В доменных тестах не участвуют.
INFRASTRUCTURE_SOURCES = \
	src/infrastructure/x11/keyboard.c \
	src/infrastructure/x11/clipboard.c \
	src/infrastructure/x11/events.c \
	src/infrastructure/x11/tray.c \
	src/infrastructure/system/scheduler-time.c \
	src/infrastructure/system/logger.c \
	src/infrastructure/system/config-store-fs.c \
	src/infrastructure/system/command-runner-fork.c \
	src/infrastructure/system/desktop.c
INFRASTRUCTURE_OBJECTS = $(INFRASTRUCTURE_SOURCES:.c=.o)

# Слой представления: состояние иконки поверх ports/tray.
PRESENTATION_SOURCES = \
	src/presentation/tray-indicator.c
PRESENTATION_OBJECTS = $(PRESENTATION_SOURCES:.c=.o)

APP_OBJECTS = src/app/app.o main.o $(DOMAIN_OBJECTS) $(USE_CASE_OBJECTS) $(INFRASTRUCTURE_OBJECTS) $(PRESENTATION_OBJECTS)
TEST_DOMAIN = tests/unit/test-domain

# AppImage и GitHub Releases. VERSION берётся из README.md, но её можно
# переопределить: make appimage VERSION=7.3
VERSION ?= $(shell sed -n '/^# VERSION$$/{n;n;p;q;}' README.md | tr -d '[:space:]')
APPIMAGE_ARCH ?= $(shell uname -m)
APPIMAGE_NAME ?= erswitcher-$(VERSION)-$(APPIMAGE_ARCH).AppImage
APPIMAGE_STAGE ?= build/AppImage/AppDir
APPIMAGE_OUTDIR ?= dist
# appimagetool-bin из AUR устанавливает /usr/bin/appimagetool.
APPIMAGE_TOOL ?= appimagetool
GH ?= gh
RELEASE_TAG ?= v$(VERSION)
RELEASE_TITLE ?= erswitcher $(VERSION)

erswitcher: $(APP_OBJECTS)	## Компилирует erswitcher
		gcc $(CFLAGS) $(CLIBS) -o $@ $^

src/app/app.o: src/app/app.c $(wildcard src/app/*.h) $(wildcard src/domain/*/*.h) $(wildcard src/use-cases/*/*.h) $(wildcard src/ports/*.h) $(wildcard src/infrastructure/x11/*.h) $(wildcard src/infrastructure/system/*.h) $(wildcard src/presentation/*.h)
		gcc $(CFLAGS) -c -o $@ src/app/app.c

main.o: main.c
		gcc $(CFLAGS) -c -o $@ main.c

$(DOMAIN_OBJECTS): %.o: %.c $(wildcard src/domain/*/*.h)
		gcc $(CFLAGS) -c -o $@ $<

$(USE_CASE_OBJECTS): %.o: %.c $(wildcard src/domain/*/*.h) $(wildcard src/ports/*.h) $(wildcard src/use-cases/*/*.h)
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

s: erswitcher	## Собирает и запускает ./erswitcher с отладочным выводом
		ERSWITCHER_DEBUG=1 ./erswitcher

kill:	## Убивает процесс erswitcher-а
		killall -9 erswitcher || true

CLANG_FORMAT ?= clang-format

format:	## Форматирует C/H через clang-format
		@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "FAIL: не найден $(CLANG_FORMAT); см. doc/formatting.md" >&2; exit 1; }
		find main.c src tests -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 -r $(CLANG_FORMAT) -i

format-check:	## Проверяет форматирование C/H без изменений
		@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "FAIL: не найден $(CLANG_FORMAT); см. doc/formatting.md" >&2; exit 1; }
		find main.c src tests -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 -r $(CLANG_FORMAT) --dry-run --Werror

check-deps:	## Проверяет зависимости; C=Сборка/Тесты/Разработка/Упаковка
		@CATEGORY="$(C)" ./tools/check-deps.sh

CPPCHECK ?= cppcheck
CPPCHECK_FLAGS ?= --enable=warning,performance,portability --std=c17 --language=c \
	--inline-suppr --suppress=missingIncludeSystem \
	--suppress='unknownMacro:tests/unit/*' \
	--suppress='returnImplicitInt:tests/unit/*' --error-exitcode=1
CPPCHECK_SOURCES = main.c $(shell find src -name '*.c' -print | sort) \
	tests/unit/test.c tests/unit/test-domain.c $(wildcard tests/fuzz/*.c)

cppcheck:	## Статический анализ cppcheck
		@command -v $(CPPCHECK) >/dev/null 2>&1 || { echo "FAIL: не найден $(CPPCHECK)" >&2; exit 1; }
		$(CPPCHECK) $(CPPCHECK_FLAGS) -I src -I tests/unit $(CPPCHECK_SOURCES)

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

test: test-unit test-functional	## Все тесты

# Criterion предоставляет main; test.c подключает все наборы в один
# translation unit, чтобы использовать общие фикстуры и моки X11
test-build: $(TEST_APP_SOURCES) $(wildcard tests/unit/*.c) $(wildcard tests/unit/*.h) $(wildcard src/*/*.h) $(wildcard src/*/*/*.h)
		$(CC) $(TEST_CFLAGS) $(TEST_LDFLAGS) -rdynamic -o tests/unit/test \
			tests/unit/test.c $(TEST_APP_SOURCES) \
			$$(pkg-config --cflags --libs criterion) $(TEST_LIBS_LOCAL)
		$(CC) $(TEST_CFLAGS) $(TEST_LDFLAGS) -o $(TEST_DOMAIN) \
			tests/unit/test-domain.c $(DOMAIN_SOURCES) \
			$$(pkg-config --cflags --libs criterion)

test-unit: test-build	## Юнит-тесты на Criterion
		tests/unit/test --color=always
		$(TEST_DOMAIN) --color=always

ASAN_OPTIONS ?= detect_leaks=1:abort_on_error=1:halt_on_error=1

memcheck: test-build	## Полный прогон тестов с ASan
		ASAN_OPTIONS="$(ASAN_OPTIONS)" tests/unit/test --color=always
		ASAN_OPTIONS="$(ASAN_OPTIONS)" $(TEST_DOMAIN) --color=always

SANITIZE ?= -fsanitize=address,undefined -fno-sanitize-recover=all
UBSAN_OPTIONS ?= halt_on_error=1:print_stacktrace=1

test-sanitize:	## Юнит-тесты с ASan + UBSan
		ASAN_OPTIONS="$(ASAN_OPTIONS)" UBSAN_OPTIONS="$(UBSAN_OPTIONS)" \
			$(MAKE) test-unit TEST_SANITIZE='$(SANITIZE)'

VALGRIND ?= valgrind
VALGRIND_FILTER ?= commands/*
VALGRIND_FLAGS ?= --leak-check=full --show-leak-kinds=definite,indirect \
	--errors-for-leak-kinds=definite,indirect --track-origins=yes --error-exitcode=1

valgrind:	## Valgrind для выбранных тестов: VALGRIND_FILTER=...
		@command -v $(VALGRIND) >/dev/null 2>&1 || { echo "FAIL: не найден $(VALGRIND)" >&2; exit 1; }
		$(MAKE) test-build TEST_SANITIZE=
		cd tests/unit && CRITERION_JOBS=1 $(VALGRIND) $(VALGRIND_FLAGS) \
			--trace-children=yes --trace-children-skip=/bin/sh,/usr/bin/rm,/bin/rm \
			./test --filter '$(VALGRIND_FILTER)' --color=never

# Фаззинг: libFuzzer из Clang, отдельные бинарники и корпус
FUZZ_CC ?= clang
FUZZ_DIR ?= build/fuzz
FUZZ_CORPUS ?= $(FUZZ_DIR)/corpus
FUZZ_ARTIFACTS ?= $(FUZZ_DIR)/artifacts
FUZZ_SECONDS ?= 60
FUZZ_TARGETS ?= text config command input compose
FUZZ_TARGET ?= text
FUZZ_ARGS ?=
FUZZ_CFLAGS ?= -std=gnu17 -g -O1 -fno-omit-frame-pointer -Wall -Wextra -Werror \
	-fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all
FUZZ_BINARIES = $(addprefix $(FUZZ_DIR)/fuzz-,$(FUZZ_TARGETS))

# Внутренняя проверка для fuzz-build; в make help не показывается
fuzz-check:
		@command -v $(FUZZ_CC) >/dev/null 2>&1 || { echo "FAIL: не найден $(FUZZ_CC); установите clang" >&2; exit 1; }

$(FUZZ_DIR)/fuzz-text: tests/fuzz/fuzz-text.c src/domain/text/text.c
		@mkdir -p $(FUZZ_DIR)
		$(FUZZ_CC) $(FUZZ_CFLAGS) -o $@ $^

$(FUZZ_DIR)/fuzz-config: tests/fuzz/fuzz-config.c src/use-cases/config-reload/config-reload.c src/domain/config/config.c src/domain/text/text.c
		@mkdir -p $(FUZZ_DIR)
		$(FUZZ_CC) $(FUZZ_CFLAGS) -o $@ $^

$(FUZZ_DIR)/fuzz-command: tests/fuzz/fuzz-command.c src/domain/command-template/command-template.c
		@mkdir -p $(FUZZ_DIR)
		$(FUZZ_CC) $(FUZZ_CFLAGS) -o $@ $^

$(FUZZ_DIR)/fuzz-input: tests/fuzz/fuzz-input.c src/domain/input-buffer/input-buffer.c
		@mkdir -p $(FUZZ_DIR)
		$(FUZZ_CC) $(FUZZ_CFLAGS) -o $@ $^

$(FUZZ_DIR)/fuzz-compose: tests/fuzz/fuzz-compose.c src/domain/compose/compose.c src/domain/config/config.c src/domain/text/text.c
		@mkdir -p $(FUZZ_DIR)
		$(FUZZ_CC) $(FUZZ_CFLAGS) -o $@ $^

fuzz-build: fuzz-check $(FUZZ_BINARIES)	## Собирает libFuzzer-бинарники

fuzz: fuzz-build	## Один фаззер: FUZZ_TARGET=text FUZZ_SECONDS=60
		@test -n "$(FUZZ_TARGET)" || { echo "FAIL: задайте FUZZ_TARGET=text|config|command|input|compose" >&2; exit 1; }
		@test -x $(FUZZ_DIR)/fuzz-$(FUZZ_TARGET) || { echo "FAIL: нет $(FUZZ_DIR)/fuzz-$(FUZZ_TARGET)" >&2; exit 1; }
		@mkdir -p $(FUZZ_CORPUS)/$(FUZZ_TARGET) $(FUZZ_ARTIFACTS)
		@if [ -d tests/fuzz/corpus/$(FUZZ_TARGET) ]; then cp -R tests/fuzz/corpus/$(FUZZ_TARGET)/. $(FUZZ_CORPUS)/$(FUZZ_TARGET)/; fi
		@if [ -d tests/fuzz/regressions/$(FUZZ_TARGET) ]; then cp -R tests/fuzz/regressions/$(FUZZ_TARGET)/. $(FUZZ_CORPUS)/$(FUZZ_TARGET)/; fi
		$(FUZZ_DIR)/fuzz-$(FUZZ_TARGET) -max_total_time=$(FUZZ_SECONDS) \
			-artifact_prefix=$(FUZZ_ARTIFACTS)/ $(FUZZ_ARGS) $(FUZZ_CORPUS)/$(FUZZ_TARGET)

fuzz-all: fuzz-build	## Все фаззеры: FUZZ_SECONDS=60
		@set -e; for t in $(FUZZ_TARGETS); do \
			mkdir -p $(FUZZ_CORPUS)/$$t $(FUZZ_ARTIFACTS); \
			if [ -d tests/fuzz/corpus/$$t ]; then cp -R tests/fuzz/corpus/$$t/. $(FUZZ_CORPUS)/$$t/; fi; \
			if [ -d tests/fuzz/regressions/$$t ]; then cp -R tests/fuzz/regressions/$$t/. $(FUZZ_CORPUS)/$$t/; fi; \
			$(FUZZ_DIR)/fuzz-$$t -max_total_time=$(FUZZ_SECONDS) \
				-artifact_prefix=$(FUZZ_ARTIFACTS)/ $(FUZZ_ARGS) $(FUZZ_CORPUS)/$$t; \
		done

fuzz-clean:	## Удаляет бинарники и корпус фаззера
		rm -rf $(FUZZ_DIR)

# Покрытие: отдельные объекты с --coverage, чтобы не смешивать с ASan-сборкой
COVERAGE_DIR ?= build/coverage
COVERAGE_CFLAGS = -std=gnu17 -g -O0 -Wall -Wextra -Werror --coverage
COVERAGE_LDFLAGS = --coverage
COVERAGE_APP_OBJECTS = $(patsubst %.c,$(COVERAGE_DIR)/%.o,$(TEST_APP_SOURCES))
COVERAGE_TEST_OBJECT = $(COVERAGE_DIR)/tests/unit/test.o
COVERAGE_DOMAIN_TEST_OBJECT = $(COVERAGE_DIR)/tests/unit/test-domain.o
COVERAGE_DOMAIN_OBJECTS = $(patsubst %.c,$(COVERAGE_DIR)/%.o,$(DOMAIN_SOURCES))
COVERAGE_OBJECTS = $(COVERAGE_APP_OBJECTS) $(COVERAGE_DOMAIN_OBJECTS) \
	$(COVERAGE_TEST_OBJECT) $(COVERAGE_DOMAIN_TEST_OBJECT)
GCOVR ?= gcovr
COVERAGE_FILTER ?= src/.*
COVERAGE_HTML_DIR ?= build/coverage-html
COVERAGE_HTML ?= $(COVERAGE_HTML_DIR)/index.html

$(COVERAGE_DIR)/%.o: %.c $(wildcard tests/unit/*.h) $(wildcard src/*/*.h) $(wildcard src/*/*/*.h)
		@mkdir -p $(dir $@)
		$(CC) $(COVERAGE_CFLAGS) -c -o $@ $<

$(COVERAGE_DIR)/test: $(COVERAGE_TEST_OBJECT) $(COVERAGE_APP_OBJECTS)
		$(CC) $(COVERAGE_LDFLAGS) -rdynamic -o $@ $^ \
			$$(pkg-config --cflags --libs criterion) $(TEST_LIBS_LOCAL)

$(COVERAGE_DIR)/test-domain: $(COVERAGE_DOMAIN_TEST_OBJECT) $(COVERAGE_DOMAIN_OBJECTS)
		$(CC) $(COVERAGE_LDFLAGS) -o $@ $^ \
			$$(pkg-config --cflags --libs criterion)

coverage-build: $(COVERAGE_DIR)/test $(COVERAGE_DIR)/test-domain	## Собирает тесты с покрытием

coverage-run: coverage-build	## Запускает тесты для покрытия
		find $(COVERAGE_DIR) -name '*.gcda' -delete
		$(COVERAGE_DIR)/test --color=never
		$(COVERAGE_DIR)/test-domain --color=never

coverage-text: coverage-run	## Цветная сводка покрытия исходников
		@command -v $(GCOVR) >/dev/null 2>&1 || { echo "FAIL: не найден $(GCOVR); установите gcovr" >&2; exit 1; }
		GCOVR="$(GCOVR)" tools/coverage-summary.sh '$(COVERAGE_FILTER)'

coverage-html: coverage-run	## Покрытие исходников в build/coverage-html/index.html
		@command -v $(GCOVR) >/dev/null 2>&1 || { echo "FAIL: не найден $(GCOVR); установите gcovr" >&2; exit 1; }
		@mkdir -p $(COVERAGE_HTML_DIR)
		$(GCOVR) -r . --filter '$(COVERAGE_FILTER)' --html --html-details -o $(COVERAGE_HTML)

coverage: coverage-run	## Сводка исходников и build/coverage-html/index.html
		@command -v $(GCOVR) >/dev/null 2>&1 || { echo "FAIL: не найден $(GCOVR); установите gcovr" >&2; exit 1; }
		GCOVR="$(GCOVR)" tools/coverage-summary.sh '$(COVERAGE_FILTER)'
		@mkdir -p $(COVERAGE_HTML_DIR)
		$(GCOVR) -r . --filter '$(COVERAGE_FILTER)' --html --html-details -o $(COVERAGE_HTML)

coverage-clean:	## Удаляет артефакты покрытия
		rm -rf $(COVERAGE_DIR) $(COVERAGE_HTML_DIR) coverage.html coverage.*.html coverage.css coverage.js

# По умолчанию функциональные тесты идут через xvfb-run: окно не
# появляется на рабочем столе и не перехватывает ввод пользователя. Если
# xvfb-run не установлен, используется текущий DISPLAY; на macOS Tk
# работает напрямую. FUNCTIONAL_HEADLESS=0 запускает тесты на текущем
# дисплее даже при наличии xvfb-run. Если выполнить тесты нельзя — это
# ОШИБКА, а не пропуск: зелёный прогон не должен скрывать, что
# функциональные тесты не выполнялись.
FUNCTIONAL_HEADLESS ?= 1

test-functional:	## Функциональные тесты окна конфигуратора (Tcl/Tk)
		@if ! command -v tclsh >/dev/null 2>&1; then \
			echo "FAIL: нет tclsh — функциональные тесты не выполнены" >&2; \
			exit 1; \
		elif [ "$(FUNCTIONAL_HEADLESS)" = 1 ] && command -v xvfb-run >/dev/null 2>&1; then \
			cd tests/functional && xvfb-run -a ./run.tcl; \
		elif [ -n "$$DISPLAY" ]; then \
			if [ "$(FUNCTIONAL_HEADLESS)" = 1 ] && ! command -v xvfb-run >/dev/null 2>&1; then \
				echo "WARN: xvfb-run не найден; тесты используют DISPLAY=$$DISPLAY" >&2; \
			fi; \
			cd tests/functional && ./run.tcl; \
		elif command -v xvfb-run >/dev/null 2>&1; then \
			cd tests/functional && xvfb-run -a ./run.tcl; \
		else \
			cd tests/functional && ./run.tcl; \
		fi

help:	## Справка
		@grep -E -h '\s##\s' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'
