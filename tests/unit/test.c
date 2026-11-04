/*
 * Юнит-тесты En—Ru Switcher
 *
 * Тесты линкуются с модулями приложения и подключаются сюда одним
 * translation unit, чтобы использовать общие статические фикстуры и
 * заглушки X11. Точку входа и запуск тестов предоставляет Criterion.
 */

#include "test.h"

#include "test-load-config.c"
#include "test-commands.c"
#include "test-functions.c"
#include "test-compose.c"
#include "test-sendkeys.c"
#include "test-snippets.c"
#include "test-pure.c"
#include "test-keyboard.c"
#include "test-clipboard.c"
#include "test-use-cases.c"
#include "test-scheduler.c"
#include "test-config-reload.c"
#include "test-desktop.c"
#include "test-tray.c"
