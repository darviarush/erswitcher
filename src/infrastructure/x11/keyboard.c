#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wctype.h>
#include <xkbcommon/xkbcommon.h>

#include "keyboard.h"
#include "../../ports/logger.h"

#define BIT(VECTOR, BIT_IDX) (((char*) (VECTOR))[BIT_IDX / 8] & (1 << (BIT_IDX % 8)))

// delay_us по умолчанию совпадает с DELAY монолита (0,01 с)
keyboard_x11_t er_kbd = {.delay_us = 10000};

static keyboard_x11_t* state_of(keyboard_port_t* port) {
    return (keyboard_x11_t*) port->context;
}

static void append_sym(char* out, size_t size, const char* text) {
    size_t len = strlen(out);
    if(len < size - 1) snprintf(out + len, size - len, "%s", text);
}

static void format_sym_impl(char* out, size_t size, int mods, KeySym ks) {
    int begin = 0;
    out[0] = '\0';

    for(int i = 0; i < 32; i++) {
        unsigned int mask = (unsigned int) mods & (1u << i);
        if(!mask) continue;
        if(begin)
            append_sym(out, size, "+");
        else
            begin = 1;
        switch(mask) {
        case ShiftMask:
            append_sym(out, size, "⇧Shift");
            break;
        case LockMask:
            append_sym(out, size, "⇪🄰CapsLock");
            break;
        case ControlMask:
            append_sym(out, size, "⌃⎈Ctrl");
            break;
        case Mod1Mask:
            append_sym(out, size, "⌥⎇Alt");
            break;
        case Mod2Mask:
            append_sym(out, size, "⇭NumLock");
            break;
        case Mod3Mask:
            append_sym(out, size, "❖Meta");
            break;
        case Mod4Mask:
            append_sym(out, size, "⌘⊞❖Super");
            break;
        case Mod5Mask:
            append_sym(out, size, "✦Hyper");
            break;
        default: {
            char buf[32];
            snprintf(buf, sizeof buf, "Mod%i", i);
            append_sym(out, size, buf);
        }
        }
    }

    if(begin) append_sym(out, size, "+");
    switch(ks) {
    case XK_Escape: append_sym(out, size, "⎋"); break;
    case XK_BackSpace: append_sym(out, size, "⌫←"); break;
    case XK_Delete: append_sym(out, size, "⌦"); break;
    case XK_Return: append_sym(out, size, "↵↩⏎⌤⌅⎆"); break;
    case XK_Tab: append_sym(out, size, "↹"); break;
    case XK_Home: append_sym(out, size, "⇱"); break;
    case XK_End: append_sym(out, size, "⇲"); break;
    case XK_Menu: append_sym(out, size, "≣"); break;
    case XK_Pause: append_sym(out, size, "⎉"); break;
    case XK_Print: append_sym(out, size, "⎙"); break;
    case XK_Multi_key: append_sym(out, size, "⎄"); break;
    }

    const char* name = XKeysymToString(ks);
    if(name) append_sym(out, size, name);
}

int keyboard_x11_format_sym(char* out, size_t size, int mods, uint32_t keysym) {
    if(!out || !size) return 0;
    format_sym_impl(out, size, mods, (KeySym) keysym);
    return 1;
}

void keyboard_x11_print_sym(int mods, uint32_t keysym) {
    char sym[256];
    keyboard_x11_format_sym(sym, sizeof sym, mods, keysym);
    DEBUG_LOG("%s", sym);
}

static void init_impl(keyboard_x11_t* s) {
    XkbDescRec* kb = XkbAllocKeyboard();
    if(!kb) return;

    kb->dpy = s->display;
    if(XkbGetNames(s->display, XkbGroupNamesMask, kb) != Success) return;

    Atom* gs = kb->names->groups;
    for(s->groups = 0; gs[s->groups] != 0;) {
        char* kb_name = XGetAtomName(s->display, gs[s->groups]);
        if(kb_name) {
            if(strncmp(kb_name, "Russian", strlen("Russian")) == 0) s->group_ru = s->groups;
            if(strncmp(kb_name, "English", strlen("English")) == 0) s->group_en = s->groups;
            XFree(kb_name);
        }
        s->groups++;
    }

    XkbFreeNames(kb, XkbGroupNamesMask, 0);
}

static unikey_t state_impl(keyboard_x11_t* s, int code) {
    XkbStateRec state;
    XkbGetState(s->display, XkbUseCoreKbd, &state);
    return (unikey_t){.code = code, .mods = state.mods, .group = state.group};
}

static KeySym to_keysym_impl(keyboard_x11_t* s, unikey_t key) {
    KeySym ks = XkbKeycodeToKeysym(s->display, key.code, key.group, key.mods & ShiftMask ? 1 : 0);
    if(ks == NoSymbol) ks = XkbKeycodeToKeysym(s->display, key.code, s->group_en, key.mods & ShiftMask ? 1 : 0);
    if(key.mods & LockMask) {
        KeySym lower, upper;
        XConvertCase(ks, &lower, &upper);
        if(key.mods & ShiftMask)
            ks = lower;
        else
            ks = upper;
    }
    return ks;
}

static int keysym_in(KeySym symbol, const KeySym* symbols) {
    while(*symbols)
        if(symbol == *symbols++) return 1;
    return 0;
}

static keyboard_key_kind_t kind_impl(keyboard_x11_t* s, unikey_t key) {
    KeySym ks = to_keysym_impl(s, key);
    if(ks == XK_BackSpace) return KEYBOARD_KEY_BACKSPACE;

    // Клавиши, которые сбрасывают начало ввода, как в старом add_to_buffer()
    const KeySym is_control[] = {
        XK_Home, XK_Left, XK_Up, XK_Right, XK_Down,
        XK_Prior, XK_Page_Up, XK_Next, XK_Page_Down,
        XK_End, XK_Begin, XK_Tab, XK_Return, 0};
    if(keysym_in(ks, is_control)) return KEYBOARD_KEY_CONTROL;

    wint_t cs = xkb_keysym_to_utf32(ks);
    const KeySym is_print[] = {XK_space, XK_underscore, 0};
    if(iswprint(cs) || keysym_in(ks, is_print)) return KEYBOARD_KEY_PRINTABLE;

    return KEYBOARD_KEY_OTHER;
}

static unikey_t from_keysym_impl(keyboard_x11_t* s, KeySym ks) {
    if(s->maybe_group >= 0) {
        for(int code = 0; code < KEYBOARD_SIZE; ++code)
            for(int shift = 0; shift < 2; ++shift) {
                KeySym search_ks = XkbKeycodeToKeysym(s->display, code, s->maybe_group, shift);
                if(ks == search_ks) return (unikey_t){.code = code, .mods = shift ? ShiftMask : 0, .group = s->maybe_group};
            }
    }

    for(int group = 0; group < s->groups; ++group)
        for(int code = 0; code < KEYBOARD_SIZE; ++code)
            for(int shift = 0; shift < 2; ++shift) {
                KeySym search_ks = XkbKeycodeToKeysym(s->display, code, group, shift);
                if(ks == search_ks) {
                    s->maybe_group = group;
                    return (unikey_t){.code = code, .mods = shift ? ShiftMask : 0, .group = group};
                }
            }

    return (unikey_t){.code = 0, .mods = 0, .group = 0};
}

static void press_code(keyboard_x11_t* s, int code, int is_press) {
    if(ERSWITCHER_DEBUG) {
        unikey_t key = state_impl(s, code);
        char sym[256];
        keyboard_x11_format_sym(sym, sizeof sym, key.mods, to_keysym_impl(s, key));
        DEBUG_LOG("   %s: %i %s\n", is_press ? "press" : "release", code, sym);
    }

    XTestFakeKeyEvent(s->display, code, is_press ? 1 : 0, CurrentTime);
    XSync(s->display, False);
}

// устанавливает модификаторы. Локи пропускает
static void send_mods(keyboard_x11_t* s, int mods, int is_press) {
    mods &= ~(LockMask | Mod2Mask);

    // Для обычных букв модификаторов нет. Не делаем лишний X-запрос
    // XGetModifierMapping на каждое нажатие: при переводе длинного текста
    // это заметно замедляет ввод в графических приложениях
    if(!mods) return;

    XModifierKeymap* modifiers = XGetModifierMapping(s->display);

    for(int mod_index = ShiftMapIndex; mod_index <= Mod5MapIndex; mod_index++) {
        if(mods & (1 << mod_index)) {
            for(int mod_key = 0; mod_key < modifiers->max_keypermod; mod_key++) {
                int code = modifiers->modifiermap[mod_index * modifiers->max_keypermod + mod_key];
                if(code) {
                    press_code(s, code, is_press);
                    break;
                }
            }
        }
    }

    XFreeModifiermap(modifiers);
}

static void press_mods(keyboard_x11_t* s, int code, int mods, int is_press) {
    if(!is_press) press_code(s, code, 0);
    send_mods(s, mods, is_press);
    if(is_press) press_code(s, code, 1);
}

static void set_locks(keyboard_x11_t* s, int mods) {
    int loks[2] = {LockMask, Mod2Mask};
    int loks_sym[2] = {XK_Caps_Lock, XK_Num_Lock};

    for(int i = 0; i < 2; i++) {
        if(mods & loks[i]) {
            unikey_t k = from_keysym_impl(s, loks_sym[i]);
            press_mods(s, k.code, k.mods, 1);
            press_mods(s, k.code, k.mods, 0);
            usleep(s->delay_us / 2);
        }
    }
}

static void set_active_group_impl(keyboard_x11_t* s, int group) {
    s->active_state.group = group;
}

static void set_switch_key_impl(keyboard_x11_t* s, const unikey_t* key) {
    memset(&s->switch_key, 0, sizeof(s->switch_key));
    s->switch_key_set = 0;
    s->os_switch_active = 0;

    if(key && key->code) {
        s->switch_key = *key;
        s->switch_key_set = 1;
    }
}

static void set_group_impl(keyboard_x11_t* s, int group) {
    if(s->switch_key_set) return;
    if(state_impl(s, 0).group == group) return;

    XkbLockGroup(s->display, XkbUseCoreKbd, group);
    XkbStateRec state;
    XkbGetState(s->display, XkbUseCoreKbd, &state);
    DEBUG_LOG("set_group: %i\n", group);
    usleep(s->delay_us);
}

static void send_key_impl(keyboard_x11_t* s, unikey_t key, int is_press, int set_layout) {
    if(key.code == 0) {
        DEBUG_LOG("send_key: No Key!\n");
        return;
    }

    KeySym ks = to_keysym_impl(s, key);
    const char* name = XKeysymToString(ks);
    DEBUG_LOG("send_key: %s %s\n", name ? name : "(null)",
              is_press == 2 ? "" : (is_press ? "PRESS" : "RELEASE"));

    int xdelay = s->delay_us / 2;
    if(xdelay <= 0) xdelay = 1;

    if(set_layout) set_group_impl(s, key.group);
    set_locks(s, key.mods);
    press_mods(s, key.code, key.mods, is_press);

    if(is_press == 2) {
        usleep(xdelay);
        press_mods(s, key.code, key.mods, 0);
        set_locks(s, key.mods);
    }

    usleep(xdelay);
}

static void switch_language_impl(keyboard_x11_t* s, int group) {
    if(s->switch_key_set) {
        // Сначала включаем режим, чтобы сам switch_key не вызвал XkbLockGroup
        s->os_switch_active = 1;
        send_key_impl(s, s->switch_key, KEYBOARD_PRESS_AND_RELEASE, 0);
        return;
    }

    s->os_switch_active = 0;
    set_active_group_impl(s, group);
}

static void press_key_impl(keyboard_x11_t* s, unikey_t key) {
    send_key_impl(s, key, KEYBOARD_PRESS_AND_RELEASE, 1);
}

static void press_key_multi_impl(keyboard_x11_t* s, unikey_t key, int n) {
    for(int i = 0; i < n; i++)
        press_key_impl(s, key);
}

static void clear_active_mods_impl(keyboard_x11_t* s) {
    if(s->active_use) {
        fprintf(stderr, "ABORT: Вложенный clear_active_mods\n");
        exit(1);
    }
    s->active_use++;

    DEBUG_LOG("===== clear_active_mods START =====\n");

    init_impl(s);
    s->active_state = state_impl(s, 0);

    XModifierKeymap* modifiers = XGetModifierMapping(s->display);
    s->active_len = 0;

    for(int mod_index = ShiftMapIndex; mod_index <= Mod5MapIndex; mod_index++) {
        for(int mod_key = 0; mod_key < modifiers->max_keypermod; mod_key++) {
            int code = modifiers->modifiermap[mod_index * modifiers->max_keypermod + mod_key];
            if(code && BIT(s->keys, code)) s->active_codes[s->active_len++] = code;
        }
    }

    XFreeModifiermap(modifiers);

    for(int i = 0; i < s->active_len; i++)
        press_code(s, s->active_codes[i], 0);

    set_locks(s, s->active_state.mods);

    DEBUG_LOG("===== clear_active_mods END =====\n");
}

static void recover_active_mods_impl(keyboard_x11_t* s) {
    DEBUG_LOG("===== recover_active_mods START =====\n");

    unikey_t state = state_impl(s, 0);
    send_mods(s, state.mods, 0);

    char active_keys[KEYBOARD_BUFFER_SIZE];
    XQueryKeymap(s->display, active_keys);
    for(int i = 0; i < s->active_len; i++) {
        int code = s->active_codes[i];
        if(BIT(s->keys, code) == BIT(active_keys, code)) {
            press_code(s, code, 1);
        }
    }

    set_locks(s, s->active_state.mods);
    if(!s->os_switch_active) set_group_impl(s, s->active_state.group);
    s->os_switch_active = 0;

    DEBUG_LOG("===== recover_active_mods END =====\n");

    // Парный к active_use++ в clear_active_mods_impl(). Без этого второй
    // вызов clear_active_mods() в том же процессе считает, что вложенный
    s->active_use--;
}

static void cb_init(keyboard_port_t* port) {
    init_impl(state_of(port));
}
static unikey_t cb_state(keyboard_port_t* port, int code) {
    return state_impl(state_of(port), code);
}
static uint32_t cb_to_keysym(keyboard_port_t* port, unikey_t key) {
    return (uint32_t) to_keysym_impl(state_of(port), key);
}
static unikey_t cb_from_keysym(keyboard_port_t* port, uint32_t keysym) {
    return from_keysym_impl(state_of(port), (KeySym) keysym);
}
static uint32_t cb_to_symbol(keyboard_port_t* port, unikey_t key) {
    return xkb_keysym_to_utf32(to_keysym_impl(state_of(port), key));
}
static unikey_t cb_from_symbol(keyboard_port_t* port, uint32_t symbol) {
    return from_keysym_impl(state_of(port), xkb_utf32_to_keysym(symbol));
}
static keyboard_key_kind_t cb_kind(keyboard_port_t* port, unikey_t key) {
    return kind_impl(state_of(port), key);
}

static int cb_layout_count(keyboard_port_t* port) {
    return state_of(port)->groups;
}
static int cb_layout_ru(keyboard_port_t* port) {
    return state_of(port)->group_ru;
}
static int cb_layout_en(keyboard_port_t* port) {
    return state_of(port)->group_en;
}
static int cb_layout_hint(keyboard_port_t* port) {
    return state_of(port)->maybe_group;
}
static void cb_set_layout_hint(keyboard_port_t* port, int group) {
    state_of(port)->maybe_group = group;
}
static void cb_set_layout(keyboard_port_t* port, int group) {
    set_group_impl(state_of(port), group);
}
static void cb_set_active_group(keyboard_port_t* port, int group) {
    set_active_group_impl(state_of(port), group);
}
static void cb_set_switch_key(keyboard_port_t* port, const unikey_t* key) {
    set_switch_key_impl(state_of(port), key);
}
static void cb_switch_language(keyboard_port_t* port, int group) {
    switch_language_impl(state_of(port), group);
}
static void cb_set_delay(keyboard_port_t* port, int microseconds) {
    state_of(port)->delay_us = microseconds;
}
static void cb_send_key(keyboard_port_t* port, unikey_t key, keyboard_event_t event) {
    send_key_impl(state_of(port), key, (int) event, 1);
}
static void cb_press_key(keyboard_port_t* port, unikey_t key) {
    press_key_impl(state_of(port), key);
}
static void cb_press_key_multi(keyboard_port_t* port, unikey_t key, int n) {
    press_key_multi_impl(state_of(port), key, n);
}
static void cb_clear_active_mods(keyboard_port_t* port) {
    clear_active_mods_impl(state_of(port));
}
static void cb_recover_active_mods(keyboard_port_t* port) {
    recover_active_mods_impl(state_of(port));
}
static void cb_poll(keyboard_port_t* port) {
    keyboard_x11_t* s = state_of(port);
    XQueryKeymap(s->display, s->keys);
}
static void cb_poll_saved(keyboard_port_t* port) {
    keyboard_x11_t* s = state_of(port);
    XQueryKeymap(s->display, s->saved);
}
static const char* cb_keymap(keyboard_port_t* port) {
    return state_of(port)->keys;
}
static const char* cb_keymap_saved(keyboard_port_t* port) {
    return state_of(port)->saved;
}
static void cb_swap_keymap(keyboard_port_t* port) {
    keyboard_x11_t* s = state_of(port);
    char* swap = s->saved;
    s->saved = s->keys;
    s->keys = swap;
}

void keyboard_x11_port(keyboard_port_t* port, keyboard_x11_t* state) {
    memset(port, 0, sizeof(*port));
    port->context = state;

    port->init = cb_init;
    port->state = cb_state;
    port->to_keysym = cb_to_keysym;
    port->from_keysym = cb_from_keysym;
    port->to_symbol = cb_to_symbol;
    port->from_symbol = cb_from_symbol;
    port->kind = cb_kind;
    port->layout_count = cb_layout_count;
    port->layout_ru = cb_layout_ru;
    port->layout_en = cb_layout_en;
    port->layout_hint = cb_layout_hint;
    port->set_layout_hint = cb_set_layout_hint;
    port->set_layout = cb_set_layout;
    port->set_active_group = cb_set_active_group;
    port->set_switch_key = cb_set_switch_key;
    port->switch_language = cb_switch_language;
    port->set_delay = cb_set_delay;
    port->send_key = cb_send_key;
    port->press_key = cb_press_key;
    port->press_key_multi = cb_press_key_multi;
    port->clear_active_mods = cb_clear_active_mods;
    port->recover_active_mods = cb_recover_active_mods;
    port->poll = cb_poll;
    port->poll_saved = cb_poll_saved;
    port->keymap = cb_keymap;
    port->keymap_saved = cb_keymap_saved;
    port->swap_keymap = cb_swap_keymap;

    if(!state->saved) state->saved = state->buffer1;
    if(!state->keys) state->keys = state->buffer2;
}
