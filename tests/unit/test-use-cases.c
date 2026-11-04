#include "test.h"

/*
 * Сценарии input/translate с фейковым портом клавиатуры
 *
 * Здесь нет ни X11, ни мока X-функций: проверяется именно контракт
 * use-cases. Реальный X11-адаптер проверяется отдельно в test-keyboard.c
 */

typedef struct {
    keyboard_key_kind_t kind;
    wint_t symbols[KEYBOARD_SIZE];
    unikey_t state;
    int layout_en;
    int layout_ru;

    int clear_count;
    int recover_count;
    int restore_group;
    int backspace_count;

    unikey_t pressed[16];
    int pressed_count;
} er_fake_keyboard_t;

static keyboard_key_kind_t er_fake_kind(keyboard_port_t* port, unikey_t key) {
    (void) key;
    return ((er_fake_keyboard_t*) port->context)->kind;
}

static uint32_t er_fake_symbol(keyboard_port_t* port, unikey_t key) {
    er_fake_keyboard_t* fake = port->context;
    return fake->symbols[key.code];
}

static unikey_t er_fake_state(keyboard_port_t* port, int code) {
    er_fake_keyboard_t* fake = port->context;
    fake->state.code = code;
    return fake->state;
}

static int er_fake_layout_en(keyboard_port_t* port) {
    return ((er_fake_keyboard_t*) port->context)->layout_en;
}
static int er_fake_layout_ru(keyboard_port_t* port) {
    return ((er_fake_keyboard_t*) port->context)->layout_ru;
}
static void er_fake_restore_group(keyboard_port_t* port, int group) {
    ((er_fake_keyboard_t*) port->context)->restore_group = group;
}
static void er_fake_clear(keyboard_port_t* port) {
    ((er_fake_keyboard_t*) port->context)->clear_count++;
}
static void er_fake_recover(keyboard_port_t* port) {
    ((er_fake_keyboard_t*) port->context)->recover_count++;
}

static unikey_t er_fake_from_keysym(keyboard_port_t* port, uint32_t keysym) {
    (void) port;
    ck_assert_int_eq(keysym, KEYBOARD_SYMBOL_BACKSPACE);
    return (unikey_t){code : 14, mods : 0, group : 0};
}

static void er_fake_press_multi(keyboard_port_t* port, unikey_t key, int n) {
    (void) key;
    ((er_fake_keyboard_t*) port->context)->backspace_count += n;
}

static void er_fake_press(keyboard_port_t* port, unikey_t key) {
    er_fake_keyboard_t* fake = port->context;
    if(fake->pressed_count < (int) (sizeof(fake->pressed) / sizeof(fake->pressed[0]))) {
        fake->pressed[fake->pressed_count++] = key;
    }
}

static keyboard_port_t er_fake_port(er_fake_keyboard_t* fake) {
    keyboard_port_t port = {0};
    port.context = fake;
    port.kind = er_fake_kind;
    port.to_symbol = er_fake_symbol;
    port.state = er_fake_state;
    port.layout_en = er_fake_layout_en;
    port.layout_ru = er_fake_layout_ru;
    port.set_active_group = er_fake_restore_group;
    port.clear_active_mods = er_fake_clear;
    port.recover_active_mods = er_fake_recover;
    port.from_keysym = er_fake_from_keysym;
    port.press_key_multi = er_fake_press_multi;
    port.press_key = er_fake_press;
    return port;
}

START_TEST(er_input_use_case_prints) {
    er_fake_keyboard_t fake = {.kind = KEYBOARD_KEY_PRINTABLE};
    fake.symbols[30] = L'a';
    keyboard_port_t port = er_fake_port(&fake);
    input_buffer_t buffer;
    input_buffer_init(&buffer);
    input_use_case_t input;
    input_use_case_init(&input, &buffer, &port);

    input_use_case_handle_key(&input, (unikey_t){code : 30, mods : 0, group : 0});
    ck_assert_int_eq(buffer.pos, 1);
    ck_assert_int_eq(buffer.word[0].code, 30);
    ck_assert_int_eq(buffer.symbol[0], L'a');
}
END_TEST

START_TEST(er_input_use_case_backspace_and_control) {
    er_fake_keyboard_t fake = {.kind = KEYBOARD_KEY_BACKSPACE};
    keyboard_port_t port = er_fake_port(&fake);
    input_buffer_t buffer;
    input_buffer_init(&buffer);
    buffer.pos = 2;
    input_use_case_t input;
    input_use_case_init(&input, &buffer, &port);

    input_use_case_handle_key(&input, (unikey_t){code : 14, mods : 0, group : 0});
    ck_assert_int_eq(buffer.pos, 1);

    // Shift+Backspace не удаляет: старое поведение change_key/add_to_buffer
    input_use_case_handle_key(&input, (unikey_t){code : 14, mods : ERS_MOD_SHIFT, group : 0});
    ck_assert_int_eq(buffer.pos, 1);

    fake.kind = KEYBOARD_KEY_CONTROL;
    input_use_case_handle_key(&input, (unikey_t){code : 111, mods : 0, group : 0});
    ck_assert_int_eq(buffer.pos, 0);
}
END_TEST

START_TEST(er_input_use_case_from_space) {
    er_fake_keyboard_t fake = {0};
    fake.symbols[1] = 'a';
    fake.symbols[2] = ' ';
    fake.symbols[3] = 'b';
    keyboard_port_t port = er_fake_port(&fake);
    input_buffer_t buffer;
    input_buffer_init(&buffer);
    input_buffer_push(&buffer, (unikey_t){code : 1}, fake.symbols[1]);
    input_buffer_push(&buffer, (unikey_t){code : 2}, fake.symbols[2]);
    input_buffer_push(&buffer, (unikey_t){code : 3}, fake.symbols[3]);
    input_use_case_t input;
    input_use_case_init(&input, &buffer, &port);

    ck_assert_int_eq(input_use_case_from_space(&input), 2);
}
END_TEST

START_TEST(er_input_use_case_from_space_trailing_space) {
    er_fake_keyboard_t fake = {0};
    fake.symbols[1] = 'a';
    fake.symbols[2] = ' ';
    fake.symbols[3] = 'b';
    fake.symbols[4] = ' ';
    keyboard_port_t port = er_fake_port(&fake);
    input_buffer_t buffer;
    input_buffer_init(&buffer);
    input_buffer_push(&buffer, (unikey_t){code : 1}, fake.symbols[1]);
    input_buffer_push(&buffer, (unikey_t){code : 2}, fake.symbols[2]);
    input_buffer_push(&buffer, (unikey_t){code : 3}, fake.symbols[3]);
    input_buffer_push(&buffer, (unikey_t){code : 4}, fake.symbols[4]);
    input_use_case_t input;
    input_use_case_init(&input, &buffer, &port);

    // Хвостовой пробел не должен обнулять последнее слово
    ck_assert_int_eq(input_use_case_from_space(&input), 2);
}
END_TEST

START_TEST(er_translate_use_case_buffer) {
    er_fake_keyboard_t fake = {0};
    fake.layout_en = 0;
    fake.layout_ru = 1;
    fake.state.group = 0;
    keyboard_port_t port = er_fake_port(&fake);
    unikey_t word[2] = {
        {code : 30, mods : 0, group : 0},
        {code : 48, mods : 0, group : 1},
    };

    translate_use_case_buffer(&port, word, 2, 0, 1);

    ck_assert_int_eq(fake.clear_count, 1);
    ck_assert_int_eq(fake.recover_count, 1);
    ck_assert_int_eq(fake.restore_group, 1);
    ck_assert_int_eq(fake.backspace_count, 2);
    ck_assert_int_eq(fake.pressed_count, 2);
    ck_assert_int_eq(word[0].group, 1);
    ck_assert_int_eq(word[1].group, 0);
    ck_assert_int_eq(fake.pressed[0].group, 1);
    ck_assert_int_eq(fake.pressed[1].group, 0);
}
END_TEST

START_TEST(er_translate_use_case_invertcase) {
    er_fake_keyboard_t fake = {0};
    keyboard_port_t port = er_fake_port(&fake);
    unikey_t word[2] = {
        {code : 30, mods : 0, group : 0},
        {code : 48, mods : ERS_MOD_SHIFT, group : 0},
    };

    translate_use_case_invertcase(&port, word, 2, 0, 0);

    ck_assert_int_eq(fake.clear_count, 1);
    ck_assert_int_eq(fake.recover_count, 1);
    ck_assert_int_eq(fake.backspace_count, 0);
    ck_assert_int_eq(fake.pressed_count, 2);
    ck_assert_int_eq(word[0].mods, ERS_MOD_SHIFT);
    ck_assert_int_eq(word[1].mods, 0);
}
END_TEST

Suite* suite_use_cases() {
    Suite* s = suite_create("use cases");

    TCase* tc = tcase_create("input/translate");
    tcase_add_checked_fixture(tc, er_fixture, er_fixture_teardown);
    tcase_add_test(tc, er_input_use_case_prints);
    tcase_add_test(tc, er_input_use_case_backspace_and_control);
    tcase_add_test(tc, er_input_use_case_from_space);
    tcase_add_test(tc, er_input_use_case_from_space_trailing_space);
    tcase_add_test(tc, er_translate_use_case_buffer);
    tcase_add_test(tc, er_translate_use_case_invertcase);
    suite_add_tcase(s, tc);

    return s;
}
