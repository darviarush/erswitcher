#include "test.h"

/*
 * Трей: X11-адаптер раскладывает native events в tray_event_t, а
 * presentation/tray-indicator реагирует на клик и размер, не зная X11
 */

typedef struct {
    int dock_calls;
    int send_calls;
    int event_calls;
    void* dock_display;
    unsigned long dock_window;
    tray_event_t next_event;
    int width;
    int height;
} fake_tray_t;

static int fake_tray_dock(tray_port_t* port, void* display,
                          unsigned long window) {
    fake_tray_t* state = (fake_tray_t*) port->context;
    state->dock_calls++;
    state->dock_display = display;
    state->dock_window = window;
    return 1;
}

static int fake_tray_send(tray_port_t* port, long message,
                          long data1, long data2, long data3) {
    (void) message;
    (void) data1;
    (void) data2;
    (void) data3;
    fake_tray_t* state = (fake_tray_t*) port->context;
    state->send_calls++;
    return 1;
}

static tray_event_t fake_tray_event(tray_port_t* port, const void* event) {
    (void) event;
    fake_tray_t* state = (fake_tray_t*) port->context;
    state->event_calls++;
    return state->next_event;
}

static void fake_tray_size(tray_port_t* port, int* width, int* height) {
    fake_tray_t* state = (fake_tray_t*) port->context;
    if(width) *width = state->width;
    if(height) *height = state->height;
}

static void fake_tray_init(tray_port_t* port, fake_tray_t* state) {
    memset(state, 0, sizeof(*state));
    memset(port, 0, sizeof(*port));
    port->context = state;
    port->dock = fake_tray_dock;
    port->send = fake_tray_send;
    port->event = fake_tray_event;
    port->size = fake_tray_size;
}

typedef struct {
    int clicks;
} tray_click_context_t;

static void tray_click(void* context) {
    ((tray_click_context_t*) context)->clicks++;
}

START_TEST(er_tray_indicator_attaches_and_clicks) {
    fake_tray_t state;
    tray_port_t port;
    tray_indicator_t indicator;
    tray_click_context_t context = {0};

    fake_tray_init(&port, &state);
    state.width = 24;
    state.height = 24;

    tray_indicator_init(&indicator, &port, tray_click, &context);
    ck_assert_int_eq(tray_indicator_attach(&indicator, (void*) 0x1234, 77), 1);
    ck_assert_int_eq(state.dock_calls, 1);
    ck_assert_ptr_eq(state.dock_display, (void*) 0x1234);
    ck_assert_int_eq(state.dock_window, 77);

    int width = 0;
    int height = 0;
    tray_indicator_size(&indicator, &width, &height);
    ck_assert_int_eq(width, 24);
    ck_assert_int_eq(height, 24);

    state.next_event = TRAY_EVENT_CLICK;
    ck_assert_int_eq(tray_indicator_handle_event(&indicator, NULL), 1);
    ck_assert_int_eq(context.clicks, 1);

    state.next_event = TRAY_EVENT_NONE;
    ck_assert_int_eq(tray_indicator_handle_event(&indicator, NULL), 0);
    ck_assert_int_eq(context.clicks, 1);
}
END_TEST

START_TEST(er_tray_x11_maps_events) {
    tray_x11_t state;
    tray_port_t port;
    XEvent event;

    tray_x11_port(&port, &state);

    ck_assert_int_eq(port.dock(&port, (void*) 0x1, 77), 1);
    ck_assert_ptr_eq((void*) state.display, (void*) 0x1);
    ck_assert_int_eq(state.window, 77);

    memset(&event, 0, sizeof event);
    event.type = ButtonPress;
    ck_assert_int_eq(port.event(&port, &event), TRAY_EVENT_CLICK);

    event.type = Expose;
    ck_assert_int_eq(port.event(&port, &event), TRAY_EVENT_EXPOSED);

    event.type = ConfigureNotify;
    event.xconfigure.width = 10;
    event.xconfigure.height = 20;
    ck_assert_int_eq(port.event(&port, &event), TRAY_EVENT_RESIZED);
    int width = 0;
    int height = 0;
    port.size(&port, &width, &height);
    ck_assert_int_eq(width, 10);
    ck_assert_int_eq(height, 20);

    event.type = ClientMessage;
    ck_assert_int_eq(port.event(&port, &event), TRAY_EVENT_DOCKED);
    port.size(&port, &width, &height);
    ck_assert_int_eq(width, 1);
    ck_assert_int_eq(height, 1);

    event.type = SelectionNotify;
    ck_assert_int_eq(port.event(&port, &event), TRAY_EVENT_NONE);
}
END_TEST

static TCase* tcase_tray() {
    TCase* tc = tcase_create("tray");
    tcase_add_test(tc, er_tray_indicator_attaches_and_clicks);
    tcase_add_test(tc, er_tray_x11_maps_events);
    return tc;
}

Suite* suite_tray() {
    Suite* s = suite_create("tray");
    suite_add_tcase(s, tcase_tray());
    return s;
}
