#include "test.h"

TestSuite(functions, .init = er_fixture, .fini = er_fixture_teardown);

/*
 * [functions] — из одной части конфига в вызовы функций
 *
 * Проверяем, что после load_config() каждая строка `комбинация=функция`
 * попадает в keyfn[] с нужной клавишей, масками модификаторов и указателем
 * на функцию, а то, что распарсить нельзя, — пропускается без падения
 *
 * Мокается только X11 (mock.c): «Pause» проходит через XStringToKeysym,
 * поэтому проверяем и то, что вызов X11 вообще был
 */

static int er_find_action(config_action_t action) {
    for(int i = 0; i < keyfn_size; i++)
        if(keyfn[i].action == action) return i;
    return -1;
}

static int er_find_mods(unsigned code, int mods) {
    for(int i = 0; i < keyfn_size; i++) {
        if(keyfn[i].key.code == code && keyfn[i].key.mods == mods) return i;
    }
    return -1;
}

// @category конфиг по умолчанию

Test(functions, er_functions_default_config) {
    er_keyboard_config();
    er_load(
        "[functions]\n"
        "Pause=word translate\n"
        "Control+Pause=text translate\n"
        "Shift+Break=selected translate\n"
        "Alt+Shift+Break=word invertcase\n"
        "Alt+Control+Pause=text invertcase\n"
        "Alt+Pause=selected invertcase\n"
        "KP_Begin=compose\n"
        "Scroll_Lock=compose\n");

    ck_assert_int_eq(keyfn_size, 8);
    ck_assert(mock_has("XStringToKeysym(Pause"));

    int i = er_find_mods(STR_TO_KEY("Pause").code, ControlMask);
    ck_assert_msg(i >= 0, "Control+Pause не найден");
    ck_assert_int_eq(keyfn[i].action, CONFIG_ACTION_TEXT_TRANSLATE);
    ck_assert_ptr_null((void*) keyfn[i].arg);

    i = er_find_mods(STR_TO_KEY("Break").code, AltMask | ShiftMask);
    ck_assert_msg(i >= 0, "Alt+Shift+Break не найден");
    ck_assert_int_eq(keyfn[i].action, CONFIG_ACTION_WORD_INVERTCASE);

    i = er_find_action(CONFIG_ACTION_COMPOSE);
    ck_assert_msg(i >= 0, "compose не найден");
    ck_assert_int_eq(keyfn[i].key.code, STR_TO_KEY("KP_Begin").code);
}
Test(functions, er_functions_modifier_aliases) {
    er_keyboard_config();
    er_load(
        "[functions]\n"
        "Ctrl+Pause=word translate\n"
        "Option+Pause=word invertcase\n"
        "Win+Pause=selected translate\n"
        "Meta+Pause=selected invertcase\n"
        "Hyper+Pause=text translate\n"
        "Command+Pause=text invertcase\n");

    ck_assert_int_eq(keyfn_size, 6);

    struct {
        char const* name;
        int mods;
    } aliases[] = {
        {"Ctrl", ControlMask},
        {"Option", AltMask},
        {"Win", SuperMask},
        {"Command", SuperMask},
        {"Meta", MetaMask},
        {"Hyper", HyperMask},
    };

    for(size_t a = 0; a < sizeof aliases / sizeof aliases[0]; a++) {
        char line[64];
        snprintf(line, sizeof line, "%s+Pause", aliases[a].name);
        // В строке с плюсом STR_TO_KEY() не годится: XStringToKeysym("Ctrl+Pause")
        // не существует. Нужна сама клавиша, модификаторы уже проверены выше
        ck_assert_msg(er_find_mods(STR_TO_KEY("Pause").code, aliases[a].mods) >= 0, "не найден %s", line);
    }
}
Test(functions, er_functions_multiple_modifiers_order_is_free) {
    er_keyboard_config();
    er_load(
        "[functions]\n"
        "Alt+Control+Shift+Break=text translate\n"
        "Shift+Super+Pause=selected invertcase\n");

    ck_assert_int_eq(keyfn_size, 2);
    ck_assert(er_find_mods(STR_TO_KEY("Break").code, AltMask | ControlMask | ShiftMask) >= 0);
    ck_assert(er_find_mods(STR_TO_KEY("Pause").code, ShiftMask | SuperMask) >= 0);
}
// @category ошибки в конфиге

Test(functions, er_functions_unknown_function_is_skipped) {
    er_keyboard_config();
    er_load(
        "[functions]\n"
        "Pause=word translate\n"
        "Control+Pause=make coffee\n");

    ck_assert_int_eq(keyfn_size, 1);
    ck_assert_int_eq(keyfn[0].action, CONFIG_ACTION_WORD_TRANSLATE);
}
Test(functions, er_functions_unknown_key_is_skipped) {
    er_keyboard_config();
    er_load(
        "[functions]\n"
        "NoSuchKey=word translate\n"
        "Pause=word translate\n");

    ck_assert_int_eq(keyfn_size, 1);
    ck_assert_int_eq(keyfn[0].action, CONFIG_ACTION_WORD_TRANSLATE);
}
Test(functions, er_functions_only_modifiers_is_skipped) {
    er_keyboard_config();
    er_load(
        "[functions]\n"
        "Control+Alt=word translate\n"
        "Pause=word translate\n");

    ck_assert_int_eq(keyfn_size, 1);
}
Test(functions, er_functions_two_keys_is_skipped) {
    er_keyboard_config();
    er_load(
        "[functions]\n"
        "Break+Pause=word translate\n"
        "Pause=word translate\n");

    ck_assert_int_eq(keyfn_size, 1);
    ck_assert_int_eq(keyfn[0].action, CONFIG_ACTION_WORD_TRANSLATE);
}
Test(functions, er_functions_empty_section) {
    er_load("[functions]\n");
    ck_assert_int_eq(keyfn_size, 0);
}
Test(functions, er_functions_switch_language_option) {
    er_keyboard_config();
    er_load("[options]\nswitch_language=Caps_Lock\n");

    ck_assert_int_eq(er_kbd.switch_key_set, 1);
    ck_assert_int_eq(er_kbd.switch_key.code, KEY_CAPSLOCK);
}
Test(functions, er_functions_switch_language_os_option) {
    er_keyboard_config();
    er_load("[options]\nswitch_language=OSSwitchLanguage\n");

    ck_assert_int_eq(er_kbd.switch_key_set, 0);
}
Test(functions, er_functions_old_combinations_are_dropped_on_reload) {
    er_keyboard_config();
    er_load("[functions]\nPause=word translate\n");
    ck_assert_int_eq(keyfn_size, 1);

    er_load("[functions]\nControl+Pause=text translate\n");
    ck_assert_int_eq(keyfn_size, 1);
    ck_assert_int_eq(keyfn[0].action, CONFIG_ACTION_TEXT_TRANSLATE);
}
