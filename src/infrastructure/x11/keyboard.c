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

#define BIT(VECTOR, BIT_IDX) (((char*) (VECTOR))[BIT_IDX / 8] & (1 << (BIT_IDX % 8)))

// delay_us по умолчанию совпадает с DELAY монолита (0,01 с).
keyboard_x11_t er_kbd = {.delay_us = 10000};

static keyboard_x11_t* state_of(keyboard_port_t* port) {
    return (keyboard_x11_t*) port->context;
}

static void print_sym_impl(int mods, KeySym ks) {
    int begin = 0;
    for(int i = 0; i < 32; i++) {
        int mask = mods & (1 << i);
        if(!mask) continue;
        if(begin) printf("+");
        else begin = 1;
        switch(mask) {
        case ShiftMask:
            printf("⇧Shift");
            break;
        case LockMask:
            printf("⇪🄰CapsLock");
            break;
        case ControlMask:
            printf("⌃⎈Ctrl");
            break;
        case Mod1Mask:
            printf("⌥⎇Alt");
            break;
        case Mod2Mask:
            printf("⇭NumLock");
            break;
        case Mod3Mask:
            printf("❖Meta");
            break;
        case Mod4Mask:
            printf("⌘⊞❖Super");
            break;
        case Mod5Mask:
            printf("✦Hyper");
            break;
        default:
            printf("Mod%i", i);
        }
    }

    if(begin) printf("+");
    switch(ks) {
    case XK_Escape: printf("⎋"); break;
    case XK_BackSpace: printf("⌫←"); break;
    case XK_Delete: printf("⌦"); break;
    case XK_Return: printf("↵↩⏎⌤⌅⎆"); break;
    case XK_Tab: printf("↹"); break;
    case XK_Home: printf("⇱"); break;
    case XK_End: printf("⇲"); break;
    case XK_Menu: printf("≣"); break;
    case XK_Pause: printf("⎉"); break;
    case XK_Print: printf("⎙"); break;
    case XK_Multi_key: printf("⎄"); break;
    }
    printf("%s", XKeysymToString(ks));
}

void keyboard_x11_print_sym(int mods, uint32_t keysym) {
    print_sym_impl(mods, (KeySym) keysym);
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
    return (unikey_t) {code: code, mods: state.mods, group: state.group};
}

static KeySym to_keysym_impl(keyboard_x11_t* s, unikey_t key) {
    KeySym ks = XkbKeycodeToKeysym(s->display, key.code, key.group, key.mods & ShiftMask? 1: 0);
    if(ks == NoSymbol) ks = XkbKeycodeToKeysym(s->display, key.code, s->group_en, key.mods & ShiftMask? 1: 0);
    if(key.mods & LockMask) {
        KeySym lower, upper;
        XConvertCase(ks, &lower, &upper);
        if(key.mods & ShiftMask) ks = lower;
        else ks = upper;
    }
    return ks;
}

static int keysym_in(KeySym symbol, const KeySym* symbols) {
    while(*symbols) if(symbol == *symbols++) return 1;
    return 0;
}

static keyboard_key_kind_t kind_impl(keyboard_x11_t* s, unikey_t key) {
    KeySym ks = to_keysym_impl(s, key);
    if(ks == XK_BackSpace) return KEYBOARD_KEY_BACKSPACE;

    // Клавиши, которые сбрасывают начало ввода, как в старом add_to_buffer().
    const KeySym is_control[] = {
        XK_Home, XK_Left, XK_Up, XK_Right, XK_Down,
        XK_Prior, XK_Page_Up, XK_Next, XK_Page_Down,
        XK_End, XK_Begin, XK_Tab, XK_Return, 0
    };
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
                if(ks == search_ks) return (unikey_t) {code: code, mods: shift? ShiftMask: 0, group: s->maybe_group};
            }
    }

    for(int group = 0; group < s->groups; ++group)
        for(int code = 0; code < KEYBOARD_SIZE; ++code)
            for(int shift = 0; shift < 2; ++shift) {
                KeySym search_ks = XkbKeycodeToKeysym(s->display, code, group, shift);
                if(ks == search_ks) {
                    s->maybe_group = group;
                    return (unikey_t) {code: code, mods: shift? ShiftMask: 0, group: group};
                }
            }

    return (unikey_t) {code: 0, mods: 0, group: 0};
}

static void press_code(keyboard_x11_t* s, int code, int is_press) {
    unikey_t key = state_impl(s, code);
    printf("   %s: %i ", is_press? "press": "release", code);
    print_sym_impl(key.mods, to_keysym_impl(s, key));
    printf("\n");
    XTestFakeKeyEvent(s->display, code, is_press? 1: 0, CurrentTime);
    XSync(s->display, False);
}

// устанавливает модификаторы. Локи пропускает
static void send_mods(keyboard_x11_t* s, int mods, int is_press) {
    mods &= ~(LockMask | Mod2Mask);

    // Для обычных букв модификаторов нет. Не делаем лишний X-запрос
    // XGetModifierMapping на каждое нажатие: при переводе длинного текста
    // это заметно замедляет ввод в графических приложениях.
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

static void set_group_impl(keyboard_x11_t* s, int group) {
    if(state_impl(s, 0).group == group) return;

    XkbLockGroup(s->display, XkbUseCoreKbd, group);
    XkbStateRec state;
    XkbGetState(s->display, XkbUseCoreKbd, &state);
    printf("set_group: %i\n", group);
    usleep(s->delay_us);
}

static void send_key_impl(keyboard_x11_t* s, unikey_t key, int is_press) {
    if(key.code == 0) {
        printf("send_key: No Key!\n");
        return;
    }

    KeySym ks = to_keysym_impl(s, key);
    const char* name = XKeysymToString(ks);
    printf("send_key: %s %s\n", name ? name : "(null)", is_press == 2? "": (is_press? "PRESS": "RELEASE"));

    int xdelay = s->delay_us / 2;
    if(xdelay <= 0) xdelay = 1;

    set_group_impl(s, key.group);
    set_locks(s, key.mods);
    press_mods(s, key.code, key.mods, is_press);

    if(is_press == 2) {
        usleep(xdelay);
        press_mods(s, key.code, key.mods, 0);
        set_locks(s, key.mods);
    }

    usleep(xdelay);
}

static void press_key_impl(keyboard_x11_t* s, unikey_t key) {
    send_key_impl(s, key, KEYBOARD_PRESS_AND_RELEASE);
}

static void press_key_multi_impl(keyboard_x11_t* s, unikey_t key, int n) {
    for(int i = 0; i < n; i++) press_key_impl(s, key);
}

static void clear_active_mods_impl(keyboard_x11_t* s) {
    if(s->active_use) {
        fprintf(stderr, "ABORT: Вложенный clear_active_mods\n");
        exit(1);
    }
    s->active_use++;

    printf("===== clear_active_mods START =====\n");

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

    for(int i = 0; i < s->active_len; i++) press_code(s, s->active_codes[i], 0);

    set_locks(s, s->active_state.mods);

    printf("===== clear_active_mods END =====\n");
}

static void recover_active_mods_impl(keyboard_x11_t* s) {
    printf("===== recover_active_mods START =====\n");

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
    set_group_impl(s, s->active_state.group);

    printf("===== recover_active_mods END =====\n");

    // Парный к active_use++ в clear_active_mods_impl(). Без этого второй
    // вызов clear_active_mods() в том же процессе считает, что вложенный.
    s->active_use--;
}

static void cb_init(keyboard_port_t* port) { init_impl(state_of(port)); }
static unikey_t cb_state(keyboard_port_t* port, int code) { return state_impl(state_of(port), code); }
static uint32_t cb_to_keysym(keyboard_port_t* port, unikey_t key) { return (uint32_t) to_keysym_impl(state_of(port), key); }
static unikey_t cb_from_keysym(keyboard_port_t* port, uint32_t keysym) { return from_keysym_impl(state_of(port), (KeySym) keysym); }
static uint32_t cb_to_symbol(keyboard_port_t* port, unikey_t key) { return xkb_keysym_to_utf32(to_keysym_impl(state_of(port), key)); }
static unikey_t cb_from_symbol(keyboard_port_t* port, uint32_t symbol) { return from_keysym_impl(state_of(port), xkb_utf32_to_keysym(symbol)); }
static keyboard_key_kind_t cb_kind(keyboard_port_t* port, unikey_t key) { return kind_impl(state_of(port), key); }

static int cb_layout_count(keyboard_port_t* port) { return state_of(port)->groups; }
static int cb_layout_ru(keyboard_port_t* port) { return state_of(port)->group_ru; }
static int cb_layout_en(keyboard_port_t* port) { return state_of(port)->group_en; }
static int cb_layout_hint(keyboard_port_t* port) { return state_of(port)->maybe_group; }
static void cb_set_layout_hint(keyboard_port_t* port, int group) { state_of(port)->maybe_group = group; }
static void cb_set_layout(keyboard_port_t* port, int group) { set_group_impl(state_of(port), group); }
static void cb_set_active_group(keyboard_port_t* port, int group) { set_active_group_impl(state_of(port), group); }
static void cb_set_delay(keyboard_port_t* port, int microseconds) { state_of(port)->delay_us = microseconds; }
static void cb_send_key(keyboard_port_t* port, unikey_t key, keyboard_event_t event) { send_key_impl(state_of(port), key, (int) event); }
static void cb_press_key(keyboard_port_t* port, unikey_t key) { press_key_impl(state_of(port), key); }
static void cb_press_key_multi(keyboard_port_t* port, unikey_t key, int n) { press_key_multi_impl(state_of(port), key, n); }
static void cb_clear_active_mods(keyboard_port_t* port) { clear_active_mods_impl(state_of(port)); }
static void cb_recover_active_mods(keyboard_port_t* port) { recover_active_mods_impl(state_of(port)); }
static const char* cb_keymap(keyboard_port_t* port) { return state_of(port)->keys; }
static const char* cb_keymap_saved(keyboard_port_t* port) { return state_of(port)->saved; }
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
    port->set_delay = cb_set_delay;
    port->send_key = cb_send_key;
    port->press_key = cb_press_key;
    port->press_key_multi = cb_press_key_multi;
    port->clear_active_mods = cb_clear_active_mods;
    port->recover_active_mods = cb_recover_active_mods;
    port->keymap = cb_keymap;
    port->keymap_saved = cb_keymap_saved;
    port->swap_keymap = cb_swap_keymap;

    if(!state->saved) state->saved = state->buffer1;
    if(!state->keys) state->keys = state->buffer2;
}
