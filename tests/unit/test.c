/*
 * Юнит-тесты En—Ru Switcher
 *
 * Сборка и запуск:
 *
 *     make test-unit
 *
 * Как это работает:
 *
 *  - Тесты линкуются с src/app/app.o и адаптерами; test.h даёт общие
 *    объявления (input, keyfn[], compose_map[], kbd_port…)
 *  - Иксы подменены: test.h подключает tests/unit/mock.c
 *  - Все наборы — отдельные .c, которые подключаются сюда же; общий
 *    main() один, поэтому символы не размножаются
 *
 * Типовой сценарий теста:
 *
 *     er_fixture_setup();      // песочница вместо ~, HOME, chdir
 *     er_reset_state();        // монолит в исходное состояние
 *     ...
 *     er_fixture_teardown();
 */

#include <check.h>
#include <stdio.h>

#include "test.h"

#include "test_load_config.c"
#include "test_commands.c"
#include "test_functions.c"
#include "test_compose.c"
#include "test_sendkeys.c"
#include "test_snippets.c"
#include "test_pure.c"
#include "test_keyboard.c"
#include "test_clipboard.c"
#include "test_use_cases.c"
#include "test_scheduler.c"
#include "test_config_reload.c"
#include "test_desktop.c"
#include "test_tray.c"

int main(void) {
    logger_init();

    int failed = 0;

    Suite* suites[] = {
        suite_load_config(),
        suite_commands(),
        suite_functions(),
        suite_compose(),
        suite_sendkeys(),
        suite_snippets(),
        suite_pure(),
        suite_keyboard(),
        suite_clipboard(),
        suite_use_cases(),
        suite_scheduler(),
        suite_config_reload(),
        suite_desktop(),
        suite_tray(),
        NULL};

    for(int i = 0; suites[i]; i++) {
        SRunner* runner = srunner_create(suites[i]);
        srunner_set_fork_status(runner, CK_NOFORK);
        srunner_run_all(runner, CK_NORMAL);
        int run = srunner_ntests_run(runner);
        int bad = srunner_ntests_failed(runner);
        failed += bad;
        printf("%s: пройдено %i, провалено %i\n", bad ? "ПРОВАЛ" : "ok", run - bad, bad);
        srunner_free(runner);
    }

    printf("\nИТОГ: провалено %i\n", failed);
    return failed ? 1 : 0;
}
