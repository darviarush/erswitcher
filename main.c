/**************************************************************
 * Приложение: erswitcher - переключатель клавиатурного ввода *
 * Автор: Ярослав О. Косьмина                                 *
 * Лицензия: GPLv3                                            *
 * Местонахождение: https://github.com/darviarush/erswitcher  *
 **************************************************************/

/*
 * Точка входа. Подключается из erswitcher.c (см. ERSWITCHER_APPLICATION_MAIN),
 * потому что main() в монолите объявлен раньше app_run():
 *
 *     int main(int ac, char** av);
 *     #ifdef ERSWITCHER_APPLICATION_MAIN
 *     #include "main.c"
 *     #endif
 *     void app_run(int ac, char** av) { ... }
 *
 * В юнит-тестах (ERSWITCHER_UNIT) этот файл не подключается.
 */

int main(int ac, char** av) {
    app_run(ac, av);
    return 0;
}
