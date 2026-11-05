#ifndef __ERSWITCHER_TESTS_UNIT_TEST__H__
#define __ERSWITCHER_TESTS_UNIT_TEST__H__

/*
 * Общий заголовок юнит-тестов
 *
 * Тесты линкуются с модулями приложения (src/app/app.o и адаптерами),
 * а не подключают монолит через #include. Заголовок app.h даёт общие
 * глобалы и обёртки, mock.c — заглушки X11. Сброс состояния между
 * тестами — наша забота, для этого есть er_reset_state()
 *
 * Всё здесь static, а файл подключается из каждого test_*.c, поэтому
 * предупреждения «объявлено, но не используется» ожидаемы и заглушены
 */

#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#include "assert-compat.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Linux-коды клавиш (EV_SYN, KEY_PAUSE...) монолит использует как пространство
// имён при разборе комбинаций. Подключаем сами: в тестах мок не тащит за собой
// <linux/input-event-codes.h>, но общие заголовки X11 — тащат
#include <linux/input-event-codes.h>

#include "../../src/app/app.h"
#include "mock.c"

// Тестовые имена полей clipboard-адаптера: в приложении они не нужны,
// но тесты проверяют Selection/INCR напрямую
#define selection_chunk_size er_clipboard.selection_chunk_size
#define clipboard_s er_clipboard.send_data
#define clipboard_pos er_clipboard.send_pos
#define clipboard_len er_clipboard.send_len
#define clipboard_target er_clipboard.send_target
#define selection_retrive er_clipboard.receive_data
#define selection_retrive_length er_clipboard.receive_len
#define selection_retrive_type er_clipboard.receive_type
#define on_get_selection er_clipboard.pending_callback
#define incr_is er_clipboard.incr_is
#define incr_propid er_clipboard.incr_property
#define incr_window er_clipboard.incr_window
#define sel_data_atom er_clipboard.sel_data_atom
#define utf8_atom er_clipboard.utf8_atom
#define clipboard_atom er_clipboard.clipboard_atom
#define targets_atom er_clipboard.targets_atom
#define incr_atom er_clipboard.incr_atom

// Тестовое окружение: подменённый HOME, из которого load_config() читает
// .config/erswitcher.conf
typedef struct {
    char root[PATH_MAX];
    char home[PATH_MAX];
    char config[PATH_MAX];
    char original[PATH_MAX]; // HOME до теста, восстанавливается в teardown
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
    logger_init();

    // Монолит стартует с setlocale(LC_ALL, "ru_RU.UTF-8") — без него
    // atof() не понимает запятую в `delay=0,01` (см. app_run)
    setlocale(LC_ALL, "ru_RU.UTF-8");

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
    clipboard_x11_reset(&er_clipboard);

    config_free(&current_config);
    config_init(&current_config);
    keyfn_active = -1;
    load_config_requested = 0;

    input_buffer_init(&input);
    keys_pressed = 0;
    cursor_back = 0;

    er_kbd.delay_us = DELAY;
    loop_delay = DELAY;
    command_timeout = 13;
    command_interval = 0.1;

    app_reset_runtime();

    er_kbd.groups = 0;
    er_kbd.group_ru = -1;
    er_kbd.group_en = -1;
    er_kbd.maybe_group = -1;

    scheduler_system_reset(&er_scheduler);

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

    er_kbd.active_use = 0;
    er_kbd.active_len = 0;
    er_kbd.switch_key_set = 0;
    er_kbd.os_switch_active = 0;
}

/*
 * «Физическая» клавиатура для разбора конфига
 *
 * Комбинацию из конфига надо превратить в сканкод, а keysym_to_unikey()
 * ищет только среди клавиш настоящей раскладки. В тестах её нет, поэтому
 * кладём в мок те клавиши, что встречаются в конфиге по умолчанию
 * После загрузки конфига раскладку для конкретного теста можно переопределить:
 * er_load() её не трогает, дальше только поиск по таблице мока
 */
static void er_keyboard_config() {
    mock_keymap_clear();
    mock_keymap_set(70, 0, 0, XK_Pause);
    mock_keymap_set(71, 0, 0, XK_Break);
    mock_keymap_set(72, 0, 0, XK_KP_Begin);
    mock_keymap_set(73, 0, 0, XK_Scroll_Lock);
    mock_keymap_set(74, 0, 0, XK_F1);
    mock_keymap_set_all_groups(KEY_D, XK_d);
    mock_keymap_set_all_groups(KEY_CAPSLOCK, XK_Caps_Lock);
    mock_modifiers_fill();
    er_kbd.groups = 1;
    er_kbd.group_en = 0;
    er_kbd.maybe_group = 0;
}

// Начальное состояние перед каждым тестом: песочница и чистые глобалы монолита
static void er_fixture() {
    er_fixture_setup();
    er_reset_state();
    mock_keymap_clear();
    mock_keymap_set(75, 0, 0, 0xff13);
    mock_keymap_set_all_groups(KEY_CAPSLOCK, XK_Caps_Lock);
    er_kbd.groups = 1;
    er_kbd.maybe_group = 0;
}

// Загружает конфиг из текста (пишет в $HOME/.config/erswitcher.conf)
static void er_load(char const* text) {
    ck_assert_msg(er_write(er_env.config, text), "не записан %s", er_env.config);
    load_config();
}

// Минимальный [options]: без него проверки delay ничего не докажут
#define ER_MINIMAL_OPTIONS \
    "[options]\n"          \
    "delay=0,01\n"         \
    "loop_delay=0,02\n"

#endif /* __ERSWITCHER_TESTS_UNIT_TEST__H__ */
