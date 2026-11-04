#include "test.h"

TestSuite(clipboard, .init = er_fixture, .fini = er_fixture_teardown);

/*
 * Буфер обмена: порт и X11 Selection/INCR-адаптер
 *
 * Проверяется именно асинхронный контракт: get() не возвращает строку, а
 * вызывает callback; данные, которые callback видит, принадлежат порту
 */

typedef struct {
    int calls;
    int len;
    char data[256];
} er_clipboard_capture_t;

static void er_clipboard_capture(void* context, const char* data, int len,
                                 clipboard_format_t format) {
    er_clipboard_capture_t* capture = context;
    (void) format;

    capture->calls++;
    capture->len = len;
    snprintf(capture->data, sizeof capture->data, "%s", data ? data : "");
}

static void er_clipboard_setup(clipboard_port_t* port) {
    d = (Display*) 0x1;
    w = 42;
    er_clipboard.display = d;
    er_clipboard.window = w;
    utf8_atom = 2;
    clipboard_atom = 3;
    targets_atom = 4;
    incr_atom = 5;
    selection_chunk_size = 64;
    clipboard_x11_port(port, &er_clipboard);
}

Test(clipboard, er_clipboard_set_and_clear) {
    clipboard_port_t port;
    er_clipboard_setup(&port);

    port.set(&port, strdup("abc"), 3, CLIPBOARD_FORMAT_UTF8);

    ck_assert_ptr_nonnull(clipboard_s);
    ck_assert_str_eq(clipboard_s, "abc");
    ck_assert_int_eq(clipboard_len, 3);
    ck_assert_int_eq(clipboard_target, utf8_atom);
    ck_assert(mock_find("XSetSelectionOwner(") >= 0);

    port.clear(&port);
    ck_assert_ptr_null(clipboard_s);
    ck_assert_int_eq(clipboard_len, 0);
}
Test(clipboard, er_clipboard_get_own_selection) {
    clipboard_port_t port;
    er_clipboard_setup(&port);
    port.set(&port, strdup("тест"), 8, CLIPBOARD_FORMAT_UTF8);

    mock_selection_owner = w;
    er_clipboard_capture_t capture = {0};
    port.get(&port, CLIPBOARD_SELECTION_CLIPBOARD, CLIPBOARD_FORMAT_UTF8,
             er_clipboard_capture, &capture);

    ck_assert_int_eq(capture.calls, 1);
    ck_assert_int_eq(capture.len, 8);
    ck_assert_str_eq(capture.data, "тест");
}
Test(clipboard, er_clipboard_get_no_owner) {
    clipboard_port_t port;
    er_clipboard_setup(&port);

    mock_selection_owner = None;
    er_clipboard_capture_t capture = {0};
    port.get(&port, CLIPBOARD_SELECTION_CLIPBOARD, CLIPBOARD_FORMAT_UTF8,
             er_clipboard_capture, &capture);

    ck_assert_int_eq(capture.calls, 1);
    ck_assert_int_eq(capture.len, 0);
    ck_assert_str_eq(capture.data, "");
}
Test(clipboard, er_clipboard_get_foreign_is_async) {
    clipboard_port_t port;
    er_clipboard_setup(&port);

    mock_selection_owner = 99;

    er_clipboard_capture_t capture = {0};
    port.get(&port, CLIPBOARD_SELECTION_CLIPBOARD, CLIPBOARD_FORMAT_UTF8,
             er_clipboard_capture, &capture);

    ck_assert_int_eq(capture.calls, 0);
    ck_assert(mock_find("XConvertSelection(") >= 0);

    mock_property_type = utf8_atom;
    mock_property_data = "выделено";
    mock_property_after = 0;

    XEvent event;
    memset(&event, 0, sizeof event);
    event.xselection.type = SelectionNotify;
    event.xselection.display = d;
    event.xselection.requestor = 99;
    event.xselection.selection = clipboard_atom;
    event.xselection.property = 7;

    ck_assert_int_eq(clipboard_x11_handle_event(&er_clipboard, &event), 1);
    ck_assert_int_eq(capture.calls, 1);
    ck_assert_str_eq(capture.data, "выделено");
}
Test(clipboard, er_clipboard_get_incr) {
    clipboard_port_t port;
    er_clipboard_setup(&port);

    mock_selection_owner = 99;

    er_clipboard_capture_t capture = {0};
    port.get(&port, CLIPBOARD_SELECTION_CLIPBOARD, CLIPBOARD_FORMAT_UTF8,
             er_clipboard_capture, &capture);

    XEvent event;
    memset(&event, 0, sizeof event);
    event.xselection.type = SelectionNotify;
    event.xselection.display = d;
    event.xselection.requestor = 99;
    event.xselection.selection = clipboard_atom;
    event.xselection.property = 7;

    // Первый ответ — INCR: данных ещё нет
    mock_property_type = incr_atom;
    mock_property_data = "";
    mock_property_after = 0;
    ck_assert_int_eq(clipboard_x11_handle_event(&er_clipboard, &event), 1);
    ck_assert_int_eq(capture.calls, 0);

    // Второй ответ — последний кусок
    mock_property_type = utf8_atom;
    mock_property_data = "часть";
    mock_property_after = 0;
    ck_assert_int_eq(clipboard_x11_handle_event(&er_clipboard, &event), 1);
    ck_assert_int_eq(capture.calls, 1);
    ck_assert_str_eq(capture.data, "часть");
}
static mock_call_t* er_clipboard_find_call(const char* prefix) {
    for(int i = 0; i < mock_n; i++) {
        if(strncmp(mock_calls[i].str, prefix, strlen(prefix)) == 0) return &mock_calls[i];
    }
    return NULL;
}

Test(clipboard, er_clipboard_selection_request_sends_data) {
    clipboard_port_t port;
    er_clipboard_setup(&port);
    port.set(&port, strdup("abc"), 3, CLIPBOARD_FORMAT_UTF8);

    mock_reset();
    XEvent event;
    memset(&event, 0, sizeof event);
    event.xselectionrequest.type = SelectionRequest;
    event.xselectionrequest.display = d;
    event.xselectionrequest.requestor = 99;
    event.xselectionrequest.selection = clipboard_atom;
    event.xselectionrequest.target = utf8_atom;
    event.xselectionrequest.property = 7;

    ck_assert_int_eq(clipboard_x11_handle_event(&er_clipboard, &event), 1);

    mock_call_t* change = er_clipboard_find_call("XChangeProperty(");
    ck_assert_ptr_nonnull(change);
    ck_assert_int_eq(change->ints[0], 7); // property
    ck_assert_int_eq(change->ints[1], 8); // format
    ck_assert_int_eq(change->ints[3], 3); // nelements
    ck_assert(mock_find("XSendEvent(") >= 0);
}
Test(clipboard, er_clipboard_selection_request_incr) {
    clipboard_port_t port;
    er_clipboard_setup(&port);
    selection_chunk_size = 2;
    port.set(&port, strdup("abcdef"), 6, CLIPBOARD_FORMAT_UTF8);

    mock_reset();
    XEvent event;
    memset(&event, 0, sizeof event);
    event.xselectionrequest.type = SelectionRequest;
    event.xselectionrequest.display = d;
    event.xselectionrequest.requestor = 99;
    event.xselectionrequest.selection = clipboard_atom;
    event.xselectionrequest.target = utf8_atom;
    event.xselectionrequest.property = 7;

    ck_assert_int_eq(clipboard_x11_handle_event(&er_clipboard, &event), 1);

    mock_call_t* change = er_clipboard_find_call("XChangeProperty(");
    ck_assert_ptr_nonnull(change);
    ck_assert_int_eq(change->ints[3], 0); // INCR: пока без данных
    ck_assert(mock_find("XSelectInput(") >= 0);

    // После PropertyDelete отправляется следующий кусок ровно на chunk
    mock_reset();
    memset(&event, 0, sizeof event);
    event.xproperty.type = PropertyNotify;
    event.xproperty.display = d;
    event.xproperty.window = 99;
    event.xproperty.atom = 7;
    event.xproperty.state = PropertyDelete;

    ck_assert_int_eq(clipboard_x11_handle_event(&er_clipboard, &event), 1);
    change = er_clipboard_find_call("XChangeProperty(");
    ck_assert_ptr_nonnull(change);
    ck_assert_int_eq(change->ints[3], 2);
}
Test(clipboard, er_clipboard_selection_clear) {
    clipboard_port_t port;
    er_clipboard_setup(&port);
    port.set(&port, strdup("abc"), 3, CLIPBOARD_FORMAT_UTF8);

    XEvent event;
    memset(&event, 0, sizeof event);
    event.type = SelectionClear;

    ck_assert_int_eq(clipboard_x11_handle_event(&er_clipboard, &event), 1);
    ck_assert_ptr_null(clipboard_s);
}
