#ifndef __ERSWITCHER_TESTS_UNIT_TEST__H__
#define __ERSWITCHER_TESTS_UNIT_TEST__H__

/*
 * Общий заголовок юнит-тестов.
 *
 * Монолит подключается целиком, чтобы тесты видели и static-функции,
 * и глобалы (pos, word[], keyfn[], compose_map[], delay...). Побочный
 * эффект: сброс состояния между тестами — наша забота, для этого есть
 * er_reset_state().
 *
 * Всё здесь static, а файл подключается из каждого test_*.c, поэтому
 * предупреждения «объявлено, но не используется» ожидаемы и заглушены.
 */

#ifndef ERSWITCHER_UNIT
#define ERSWITCHER_UNIT
#endif

#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Linux-коды клавиш (EV_SYN, KEY_PAUSE...) монолит использует как пространство
// имён при разборе комбинаций. Подключаем сами: в тестах мок не тащит за собой
// <linux/input-event-codes.h>, но общие заголовки X11 — тащат
#include <linux/input-event-codes.h>

#include "../../erswitcher.c"

// Тестовое окружение: подменённый HOME, из которого load_config() читает
// .config/erswitcher.conf
typedef struct {
    char root[PATH_MAX];
    char home[PATH_MAX];
    char config[PATH_MAX];
    char original[PATH_MAX];    // HOME до теста, восстанавливается в teardown
} er_test_env_t;

static er_test_env_t er_env;

// Путь $HOME/<name>
static char* er_path(char* out, size_t out_size, char const* name) {
    snprintf(out, out_size, "%s/%s", er_env.home, name);
    return out;
}

// Пишет текст в файл, создавая недостающий каталог (на один уровень)
static int er_write(char const* path, char const* text) {
    char dir[PATH_MAX];
    snprintf(dir, sizeof dir, "%s", path);
    char* slash = strrchr(dir, '/');
    if(slash) {
        *slash = '\0';
        if(*dir) mkdir(dir, 0755);
    }

    FILE* f = fopen(path, "wb");
    if(!f) return 0;
    fputs(text, f);
    fclose(f);
    return 1;
}

// Читает файл целиком в buf
static char* er_read(char const* path, char* buf, size_t size) {
    buf[0] = '\0';
    FILE* f = fopen(path, "rb");
    if(!f) return buf;
    size_t n = fread(buf, 1, size - 1, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

// Делает копию строки, чтобы тест мог мокать её, не задевая настоящую
static void er_fixture_setup() {
    char tmpl[] = "/tmp/erswitcher-unit-XXXXXX";
    if(!mkdtemp(tmpl)) {
        fprintf(stderr, "mkdtemp: %s\n", strerror(errno));
        exit(1);
    }
    snprintf(er_env.root, sizeof er_env.root, "%s", tmpl);
    snprintf(er_env.home, sizeof er_env.home, "%s/home", tmpl);
    snprintf(er_env.original, sizeof er_env.original, "%s", getenv("HOME") ? getenv("HOME") : "");
    er_path(er_env.config, sizeof er_env.config, ".config/erswitcher.conf");

    mkdir(er_env.home, 0700);
    setenv("HOME", er_env.home, 1);

    if(chdir(er_env.home)) {
        fprintf(stderr, "chdir(%s): %s\n", er_env.home, strerror(errno));
        exit(1);
    }
}

static void er_fixture_teardown() {
    chdir("/");
    setenv("HOME", er_env.original, 1);

    char cmd[PATH_MAX + 32];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", er_env.root);
    if(system(cmd)) fprintf(stderr, "не удалил песочницу %s\n", er_env.root);
}

// Сбрасывает всё, что load_config() и тестируемый код оставили в монолите
static void er_reset_state() {
    mock_reset();
    mock_keymap_clear();

    for(keyfn_t* k = keyfn, *n = keyfn + keyfn_size; k < n; k++) if(k->arg) free(k->arg);
    free(keyfn);
    keyfn = NULL;
    keyfn_size = keyfn_max_size = 0;
    keyfn_active = -1;

    for(int i = 0; i < compose_map_size; i++) free(compose_map[i].to);
    free(compose_map);
    compose_map = NULL;
    compose_map_size = compose_map_max_size = 0;

    pos = 0;
    keys_pressed = 0;
    memset(word, 0, sizeof word);
    cursor_back = 0;

    delay = DELAY;
    loop_delay = DELAY;
    command_timeout = 13;
    command_interval = 0.1;

    groups = 0;
    group_ru = -1;
    group_en = -1;
    maybe_group = -1;

    curcommand = NULL;
    memset(timers, 0, sizeof timers);
    timers_size = 0;

    d = NULL;
    w = None;
    utf8_atom = None;
    clipboard_atom = None;
    targets_atom = None;
    incr_atom = None;
    sel_data_atom = None;
    selection_chunk_size = 0;
    insert_from_clipboard_data = NULL;
    insert_from_clipboard_save_data = NULL;
    insert_from_clipboard_save_len = 0;
    insert_from_clipboard_save_target = None;
    on_get_selection = NULL;
    on_copy_selection = NULL;

    active_use = 0;
    active_len = 0;
}

// Начальное состояние перед каждым тестом: песочница и чистые глобалы монолита
static void er_fixture() {
    er_fixture_setup();
    er_reset_state();
    mock_keymap_clear();
    mock_keymap_set(75,0,0,0xff13);
    groups = 1;
    maybe_group = 0;
}

// Загружает конфиг из текста (пишет в $HOME/.config/erswitcher.conf)
static void er_load(char const* text) {
    ck_assert_msg(er_write(er_env.config, text), "не записан %s", er_env.config);
    load_config();
}

// Минимальный [options]: без него проверки delay ничего не докажут
#define ER_MINIMAL_OPTIONS \
    "[options]\n" \
    "delay=0,01\n" \
    "loop_delay=0,02\n"

#endif /* __ERSWITCHER_TESTS_UNIT_TEST__H__ */
