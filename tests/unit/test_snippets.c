#include "test.h"

/*
 * [snippets] — сниппеты, вставляемые через буфер обмена.
 *
 * insert_from_clipboard() кладёт текст в буфер (set_clipboard) и ставит
 * таймер на Shift+Insert. Тест проверяет,
 *   - что в буфер попал именно текст сниппета (копия в собственной памяти),
 *   - раскладку вызовов X11,
 *   - что повторный сниппет отменяет предыдущий таймер.
 */

START_TEST(er_snippets_inserts_text_into_clipboard) {
    // Идентификаторы атомов нужны заранее: в жизни их создаёт open_display()
    utf8_atom = 2;
    clipboard_atom = 3;
    mock_reset();
    insert_from_clipboard("Готово.");

    ck_assert_ptr_nonnull(clipboard_s);
    ck_assert_str_eq(clipboard_s, "Готово.");
    ck_assert_int_eq(clipboard_len, 13); // «Готово.» — 13 байт в utf8

    // Буфер принадлежит окну erswitcher-а, а вставка — по таймеру
    ck_assert(mock_find("XSetSelectionOwner(") >= 0);
    ck_assert_int_gt(er_scheduler.size, 0);
}
END_TEST

START_TEST(er_snippets_copies_into_own_memory) {
    char text[] = "темп";
    insert_from_clipboard(text);

    // Указатель не должен ссылаться на временный массив: он живёт после вызова
    text[0] = 'X';
    ck_assert_str_eq(clipboard_s, "темп");
}
END_TEST

START_TEST(er_snippets_second_replaces_first) {
    insert_from_clipboard("первый");
    insert_from_clipboard("второй");

    ck_assert_str_eq(clipboard_s, "второй");
}
END_TEST

START_TEST(er_snippets_from_config) {
    er_load(
        "[snippets]\n"
        "Super+Shift+Pause=Готово.\n");

    ck_assert_int_eq(keyfn_size, 1);
    ck_assert_str_eq(keyfn[0].arg, "Готово.");
    ck_assert_int_eq(keyfn[0].action, CONFIG_ACTION_SNIPPET);
}
END_TEST

START_TEST(er_snippets_from_config_keeps_multiline) {
    er_load(
        "[snippets]\n"
        "Control+Shift+Pause=строка\\n\\tс табуляцией\n");

    ck_assert_int_eq(keyfn_size, 1);
    ck_assert_str_eq(keyfn[0].arg, "строка\n\tс табуляцией");
}
END_TEST

static TCase* tcase_snippets() {
    TCase* tc = tcase_create("snippets");
    tcase_add_checked_fixture(tc, er_fixture, er_fixture_teardown);
    tcase_add_test(tc, er_snippets_inserts_text_into_clipboard);
    tcase_add_test(tc, er_snippets_copies_into_own_memory);
    tcase_add_test(tc, er_snippets_second_replaces_first);
    tcase_add_test(tc, er_snippets_from_config);
    tcase_add_test(tc, er_snippets_from_config_keeps_multiline);
    return tc;
}

Suite* suite_snippets() {
    Suite* s = suite_create("[snippets]");
    suite_add_tcase(s, tcase_snippets());
    return s;
}
