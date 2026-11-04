#ifndef __ERSWITCHER_TESTS_UNIT_MOCK__H__
#define __ERSWITCHER_TESTS_UNIT_MOCK__H__

/*
 * Заглушка X11 для юнит-тестов. Подключается из test.h в каждый
 * тестовый бинарник; app.c и адаптеры линкуются как обычные объекты.
 *
 * Нужна потому, что разбор конфига неразрывно связан с иксами:
 * `Pause` превращается в KeySym через XStringToKeysym, а мнемоники [compose]
 * сравниваются с буфером ввода в utf32 (XkbKeycodeToKeysym -> xkb_keysym_to_utf32).
 *
 * Слой 1 — те же имена, что у настоящих функций. Очередь вызовов (mock_n)
 *   нужна, когда важно, *что именно* эмулятор ввёл: тест подменяет
 *   XTestFakeKeyEvent на функцию, собирающую строку, — так проверяются sendkeys.
 * Слой 2 — только то, что нужно чистым функциям и load_config.
 *
 * Всё, что не нужно тестам, намеренно не реализовано: если монолит начнёт
 * вызывать новую X-функцию в тестируемом коде, линковка упадёт и будет видно,
 * что мок надо дописать.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// @category Очередь вызовов

#define MOCK_CALLS_MAX 4096
#define MOCK_KEYMAP_MAX (KEYBOARD_SIZE * 4 * 2)

typedef struct {
    enum { MOCK_CALL_INT,
           MOCK_CALL_STR } type;
    int ints[4];
    char str[256];
} mock_call_t;

// Подключается тестом: перехват эмуляции нажатий (см. test_sendkeys.c)
// Отдельная переменная, потому что XTestFakeKeyEvent — функция из XTest.h
static int (*mock_fake_key_hook)(Display*, unsigned int, Bool, unsigned long) = NULL;

static mock_call_t mock_calls[MOCK_CALLS_MAX];
static int mock_n = 0;

// Настройки ответов X11 для тестов буфера обмена
static Window mock_selection_owner = None;
static Atom mock_property_type = None;
static const char* mock_property_data = NULL;
static unsigned long mock_property_after = 0;

// Отпечаток вызова: «имя(аргументы)» пишется в конец очереди
static void mock_push_int(char const* name, int a, int b, int c, int e) {
    if(mock_n >= MOCK_CALLS_MAX) return;
    mock_call_t* call = mock_calls + mock_n++;
    call->type = MOCK_CALL_INT;
    call->ints[0] = a;
    call->ints[1] = b;
    call->ints[2] = c;
    call->ints[3] = e;
    snprintf(call->str, sizeof call->str, "%s", name);
}

static void mock_push_str(char const* name, char const* s) {
    if(mock_n >= MOCK_CALLS_MAX) return;
    mock_call_t* call = mock_calls + mock_n++;
    call->type = MOCK_CALL_STR;
    call->ints[0] = call->ints[1] = call->ints[2] = call->ints[3] = 0;
    snprintf(call->str, sizeof call->str, "%s%s", name, s ? s : "(null)");
}

static void mock_reset() {
    mock_n = 0;
}

// Есть ли в очереди вызов с таким префиксом
static int mock_has(char const* prefix) {
    for(int i = 0; i < mock_n; i++) {
        if(strncmp(mock_calls[i].str, prefix, strlen(prefix)) == 0) return 1;
    }
    return 0;
}

// Номер в очереди первого вызова с префиксом, или -1
static int mock_find(char const* prefix) {
    for(int i = 0; i < mock_n; i++) {
        if(strncmp(mock_calls[i].str, prefix, strlen(prefix)) == 0) return i;
    }
    return -1;
}

static int mock_count(char const* prefix) {
    int n = 0;
    for(int i = 0; i < mock_n; i++) {
        if(strncmp(mock_calls[i].str, prefix, strlen(prefix)) == 0) n++;
    }
    return n;
}

// Склеивает строковые вызовы с префиксом подряд — для проверки ввода
static char* mock_strjoin(char const* prefix, char* out, size_t out_size) {
    size_t pos = 0;
    if(out_size) out[0] = '\0';
    for(int i = 0; i < mock_n; i++) {
        if(strncmp(mock_calls[i].str, prefix, strlen(prefix)) != 0) continue;
        char const* s = mock_calls[i].str + strlen(prefix);
        while(*s && pos + 1 < out_size)
            out[pos++] = *s++;
    }
    if(out_size) out[pos] = '\0';
    return out;
}


// @category Раскладка клавиатуры

/*
 * Таблица «сканкод -> символ» для нескольких раскладок. Тест наполняет её сам,
 * поэтому одна и та же буква может быть доступна в разных группах, и постучаться
 * к ней можно только с переключением группы — так проверяется поиск сканкода
 * в keysym_to_unikey().
 */

#define MOCK_GROUP_MAX 4

typedef struct {
    int code;
    int group;
    int shift;
    KeySym ks;
} mock_keymap_entry_t;

static mock_keymap_entry_t mock_keymap[MOCK_KEYMAP_MAX];
static int mock_keymap_n = 0;

static int mock_keyboard_group = 0; // активная группа, как раскладка в X11
static unsigned int mock_keyboard_mods = 0;

// Выставляется тестом: если не NULL, XStringToKeysym идёт сюда первым
// Так можно смоделировать символ, которого нет в текущей раскладке, но он есть по клавише
static KeySym (*mock_string_to_keysym)(char const* name) = NULL;

static void mock_keymap_clear() {
    mock_keymap_n = 0;
    mock_keyboard_group = 0;
    mock_keyboard_mods = 0;
    mock_string_to_keysym = NULL;
    mock_selection_owner = None;
    mock_property_type = None;
    mock_property_data = NULL;
    mock_property_after = 0;
}

static int mock_keymap_count() {
    return mock_keymap_n;
}
static void mock_keymap_set(int code, int group, int shift, KeySym ks) {
    if(mock_keymap_n >= MOCK_KEYMAP_MAX) return;
    mock_keymap[mock_keymap_n++] = (mock_keymap_entry_t){
        code : code,
        group : group,
        shift : shift,
        ks : ks
    };
}

// Тот же символ для всех групп и обоих уровней — например, для модификаторов
static void mock_keymap_set_all_groups(int code, KeySym ks) {
    for(int group = 0; group < MOCK_GROUP_MAX; group++) {
        mock_keymap_set(code, group, 0, ks);
        mock_keymap_set(code, group, 1, ks);
    }
}

// Что тест «нажал» на клавиатуре: активная группа и модификаторы
static void mock_keyboard(int group) {
    mock_keyboard_group = group;
}
static void mock_keyboard_mods_set(unsigned int mods) {
    mock_keyboard_mods = mods;
}


// @category Слой 1: имена настоящих X-функций

// Раскладки для init_keyboard(): NULL означает «XkbGetNames не удался»,
// тогда init_keyboard() выходит, не меняя groups/group_ru/group_en — так
// тесты и задают раскладку сами (см. er_layout())
XkbDescPtr XkbAllocKeyboard(void) {
    mock_push_int("XkbAllocKeyboard(", 0, 0, 0, 0);
    return NULL;
}

int XkbGetNames(Display* dpy, unsigned int which, XkbDescPtr xkb) {
    mock_push_int("XkbGetNames(", (int) which, 0, 0, 0);
    (void) dpy;
    (void) xkb;
    return BadAlloc; // «не удалось»: нулевая kb->names не разыменуется
}

char* XGetAtomName(Display* dpy, Atom atom) {
    mock_push_int("XGetAtomName(", (int) atom, 0, 0, 0);
    (void) dpy;
    return NULL;
}

KeySym XStringToKeysym(const char* name) {
    mock_push_str("XStringToKeysym(", name);
    if(mock_string_to_keysym) {
        KeySym ks = mock_string_to_keysym(name);
        if(ks) return ks;
    }
    if(strcmp(name, "space") == 0) return XK_space;
    if(strcmp(name, "underscore") == 0) return XK_underscore;
    if(strcmp(name, "BackSpace") == 0) return XK_BackSpace;
    if(strcmp(name, "Pause") == 0) return XK_Pause;
    if(strcmp(name, "Break") == 0) return XK_Break;
    if(strcmp(name, "Home") == 0) return XK_Home;
    if(strcmp(name, "End") == 0) return XK_End;
    if(strcmp(name, "Up") == 0) return XK_Up;
    if(strcmp(name, "Down") == 0) return XK_Down;
    if(strcmp(name, "Prior") == 0) return XK_Prior;
    if(strcmp(name, "Next") == 0) return XK_Next;
    if(strcmp(name, "KP_Enter") == 0) return XK_KP_Enter;
    if(strcmp(name, "Menu") == 0) return XK_Menu;
    if(strcmp(name, "Escape") == 0) return XK_Escape;
    if(strcmp(name, "Delete") == 0) return XK_Delete;
    if(strcmp(name, "Scroll_Lock") == 0) return XK_Scroll_Lock;
    if(strcmp(name, "KP_Begin") == 0) return XK_KP_Begin;
    if(strcmp(name, "Left") == 0) return XK_Left;
    if(strcmp(name, "Right") == 0) return XK_Right;
    if(strcmp(name, "Return") == 0) return XK_Return;
    if(strcmp(name, "Tab") == 0) return XK_Tab;
    if(strcmp(name, "Insert") == 0) return XK_Insert;
    if(strcmp(name, "Shift_L") == 0) return XK_Shift_L;
    if(strcmp(name, "Control_L") == 0) return XK_Control_L;
    if(strcmp(name, "F1") == 0) return XK_F1;
    if(strcmp(name, "F3") == 0) return XK_F3;
    if(strcmp(name, "F5") == 0) return XK_F5;

    // Клавиши-модификаторы как отдельные имена (нужны для раскладки и нажатий)
    if(strcmp(name, "Super_L") == 0) return XK_Super_L;
    if(strcmp(name, "Super_R") == 0) return XK_Super_R;
    if(strcmp(name, "Meta_L") == 0) return XK_Meta_L;
    if(strcmp(name, "Alt_L") == 0) return XK_Alt_L;
    if(strcmp(name, "Caps_Lock") == 0) return XK_Caps_Lock;
    if(strcmp(name, "Num_Lock") == 0) return XK_Num_Lock;
    if(strcmp(name, "Hyper_L") == 0) return XK_Hyper_L;

    // Остальные односимвольные имена — как в X11: `a` → 0x61
    // Кириллические имена X11 (Cyrillic_a и т.п.) здесь не поддержаны намеренно, см. doc/test-unit.md
    if(strlen(name) == 1) return (unsigned char) name[0];

    // F1..F12: у X11 это один диапазон 0xFFBE + номер
    if(name[0] == 'F' && name[1] >= '1' && name[1] <= '9' && name[2] == '\0') {
        return XK_F1 + (name[1] - '1');
    }

    return NoSymbol;
}

char* XKeysymToString(KeySym ks) {
    mock_push_int("XKeysymToString(", (int) ks, 0, 0, 0);
    switch(ks) {
    case XK_space: return "space";
    case XK_underscore: return "underscore";
    case XK_BackSpace: return "BackSpace";
    case XK_Pause: return "Pause";
    case XK_Scroll_Lock: return "Scroll_Lock";
    case XK_KP_Begin: return "KP_Begin";
    case XK_Left: return "Left";
    case XK_Right: return "Right";
    case XK_Return: return "Return";
    case XK_Tab: return "Tab";
    case XK_Insert: return "Insert";
    case XK_Shift_L: return "Shift_L";
    case XK_Control_L: return "Control_L";
    case XK_F1: return "F1";
    case XK_F3: return "F3";
    case XK_F5: return "F5";
    default: return NULL;
    }
}

KeySym XkbKeycodeToKeysym(Display* dpy, KeyCode code, int group, int level) {
    mock_push_int("XkbKeycodeToKeysym(", code, group, level, 0);
    (void) dpy;
    for(int i = 0; i < mock_keymap_n; i++) {
        mock_keymap_entry_t* e = mock_keymap + i;
        if(e->code == (int) code && e->group == group && e->shift == level) return e->ks;
    }
    return NoSymbol;
}

int XkbGetState(Display* dpy, unsigned int deviceSpec, XkbStateRec* state) {
    mock_push_int("XkbGetState(", (int) deviceSpec, 0, 0, 0);
    (void) dpy;
    if(state) {
        memset(state, 0, sizeof *state);
        state->group = mock_keyboard_group;
        state->mods = mock_keyboard_mods;
        state->base_group = mock_keyboard_group;
        state->locked_group = mock_keyboard_group;
    }
    return Success;
}

int XkbLockGroup(Display* dpy, unsigned int deviceSpec, unsigned int group) {
    mock_push_int("XkbLockGroup(", (int) deviceSpec, group, 0, 0);
    (void) dpy;
    mock_keyboard_group = group;
    return Success;
}

void XConvertCase(KeySym sym, KeySym* lower, KeySym* upper) {
    mock_push_int("XConvertCase(", (int) sym, 0, 0, 0);
    if(sym >= XK_A && sym <= XK_Z) {
        *lower = sym - XK_A + XK_a;
        *upper = sym;
    } else if(sym >= XK_a && sym <= XK_z) {
        *lower = sym;
        *upper = sym - XK_a + XK_A;
    } else
        *lower = *upper = sym;
}

int XTestFakeKeyEvent(Display* dpy, unsigned int keycode, Bool is_press, unsigned long fake_delay) {
    mock_push_int("XTestFakeKeyEvent(", keycode, is_press, (int) fake_delay, 0);
    if(mock_fake_key_hook) return mock_fake_key_hook(dpy, keycode, is_press, fake_delay);
    (void) dpy;
    return 1;
}

void XkbFreeNames(XkbDescPtr xkb, unsigned int which, Bool freeMap) {
    mock_push_int("XkbFreeNames(", (int) which, 0, 0, 0);
    (void) xkb;
    (void) freeMap;
}

int XFree(void* data) {
    mock_push_int("XFree(", 0, 0, 0, 0);
    free(data);
    return 1;
}

int XFlush(Display* dpy) {
    mock_push_int("XFlush(", 0, 0, 0, 0);
    (void) dpy;
    return 1;
}

int XSync(Display* dpy, Bool discard) {
    mock_push_int("XSync(", discard, 0, 0, 0);
    (void) dpy;
    return 0;
}


// @category Слой 2: только то, что нужно тестам

Display* XOpenDisplay(char const* name) {
    mock_push_str("XOpenDisplay(", name);
    return (Display*) 0x1;
}

int XGetErrorText(Display* dpy, int code, char* buffer, int length) {
    mock_push_int("XGetErrorText(", code, 0, 0, 0);
    (void) dpy;
    if(buffer && length > 0) snprintf(buffer, (size_t) length, "mock X error %i", code);
    return 0;
}

int (*XSynchronize(Display* dpy, Bool onoff))(Display*) {
    mock_push_int("XSynchronize(", onoff, 0, 0, 0);
    (void) dpy;
    return NULL;
}

XErrorHandler XSetErrorHandler(XErrorHandler handler) {
    mock_push_int("XSetErrorHandler(", 0, 0, 0, 0);
    (void) handler;
    return NULL;
}

Atom XInternAtom(Display* dpy, char const* atom_name, Bool only_if_exists) {
    mock_push_str("XInternAtom(", atom_name);
    (void) dpy;
    (void) only_if_exists;
    return 1 + mock_n; // любой ненулевой: None зарезервирован за отсутствующим атомом
}

int XSelectInput(Display* dpy, Window win, long mask) {
    mock_push_int("XSelectInput(", (int) mask, 0, 0, 0);
    (void) dpy;
    (void) win;
    return 1;
}

int XSetSelectionOwner(Display* dpy, Atom selection, Window owner, Time time) {
    mock_push_int("XSetSelectionOwner(", (int) selection, (int) owner, (int) time, 0);
    (void) dpy;
    return 1;
}

// Тест может сделать вид, что владелец буфера — наше окно или чужой клиент
Window XGetSelectionOwner(Display* dpy, Atom selection) {
    mock_push_int("XGetSelectionOwner(", (int) selection, 0, 0, 0);
    (void) dpy;
    return mock_selection_owner;
}

int XConvertSelection(Display* dpy, Atom selection, Atom target, Atom property,
                      Window requestor, Time time) {
    mock_push_int("XConvertSelection(", (int) selection, (int) target, (int) property, (int) requestor);
    (void) dpy;
    (void) time;
    return 1;
}

int XChangeProperty(Display* dpy, Window win, Atom property, Atom type, int format,
                    int mode, const unsigned char* data, int nelements) {
    mock_push_int("XChangeProperty(", (int) property, format, mode, nelements);
    (void) dpy;
    (void) win;
    (void) type;
    (void) data;
    return 1;
}

// Тест может подсунуть ответ на SelectionNotify. По умолчанию — пусто
int XGetWindowProperty(Display* dpy, Window win, Atom property, long long_offset,
                       long long_length, Bool del, Atom req_type, Atom* actual_type,
                       int* actual_format, unsigned long* nitems,
                       unsigned long* bytes_after, unsigned char** prop) {
    mock_push_int("XGetWindowProperty(", (int) property, (int) long_length, del, 0);
    (void) dpy;
    (void) win;
    (void) long_offset;
    (void) req_type;

    size_t len = mock_property_data ? strlen(mock_property_data) : 0;

    if(actual_type) *actual_type = mock_property_data ? mock_property_type : None;
    if(actual_format) *actual_format = mock_property_data ? 8 : 0;
    if(nitems) *nitems = len;
    if(bytes_after) *bytes_after = mock_property_after;
    if(prop) {
        *prop = NULL;
        if(mock_property_data) {
            *prop = malloc(len + 1);
            if(*prop) {
                memcpy(*prop, mock_property_data, len);
                (*prop)[len] = '\0';
            }
        }
    }
    return Success;
}

Status XGetWindowAttributes(Display* dpy, Window win, XWindowAttributes* attr) {
    mock_push_int("XGetWindowAttributes(", (int) win, 0, 0, 0);
    (void) dpy;
    if(attr) {
        memset(attr, 0, sizeof *attr);
        attr->width = 1;
        attr->height = 1;
    }
    return 1;
}

int XGetInputFocus(Display* dpy, Window* focus, int* revert_to) {
    mock_push_int("XGetInputFocus(", 0, 0, 0, 0);
    (void) dpy;
    if(focus) *focus = (Window) 42;
    if(revert_to) *revert_to = RevertToParent;
    return 1;
}

Bool XQueryPointer(Display* dpy, Window win, Window* root, Window* child,
                   int* root_x, int* root_y, int* win_x, int* win_y,
                   unsigned int* mask) {
    mock_push_int("XQueryPointer(", (int) win, 0, 0, 0);
    (void) dpy;
    if(root) *root = None;
    if(child) *child = None;
    if(root_x) *root_x = 0;
    if(root_y) *root_y = 0;
    if(win_x) *win_x = 0;
    if(win_y) *win_y = 0;
    if(mask) *mask = 0;
    return True;
}

int XQueryKeymap(Display* dpy, char keys_return[32]) {
    mock_push_int("XQueryKeymap(", 0, 0, 0, 0);
    (void) dpy;
    if(keys_return) memset(keys_return, 0, 32);
    return 1;
}

Window XCreateSimpleWindow(Display* dpy, Window parent, int x, int y,
                           unsigned int width, unsigned int height,
                           unsigned int border_width, unsigned long border,
                           unsigned long background) {
    mock_push_int("XCreateSimpleWindow(", (int) parent, x, y, (int) width);
    (void) dpy;
    (void) height;
    (void) border_width;
    (void) border;
    (void) background;
    return (Window) 42;
}

Window XDefaultRootWindow(Display* dpy) {
    mock_push_int("XDefaultRootWindow(", 0, 0, 0, 0);
    (void) dpy;
    return (Window) 1;
}

// @category Модификаторы

/*
 * Карта модификаторов X11: какую клавишу какой модификатор переключает.
 * fill() раздаёт коды по слотам ShiftMapIndex..Mod5MapIndex по одному на слот,
 * поскольку clear_active_mods()/send_mods() понимают маски как 1 << mod_index.
 * У этого макета ShiftMask == 1, ControlMask == 4 и так далее — те же биты,
 * что и в X11. Без Shift/Control внутри Mod1..Mod5, иначе маски съедутся.
 */

#define MOCK_MODIFIER_SLOTS 8
#define MOCK_MODIFIER_PER_SLOT 2

static KeyCode mock_modifier_codes[MOCK_MODIFIER_SLOTS][MOCK_MODIFIER_PER_SLOT];

#define MOCK_SHIFT_INDEX 0
#define MOCK_LOCK_INDEX 1
#define MOCK_CONTROL_INDEX 2
#define MOCK_MOD1_INDEX 3
#define MOCK_MOD2_INDEX 4
#define MOCK_MOD3_INDEX 5
#define MOCK_MOD4_INDEX 6
#define MOCK_MOD5_INDEX 7

static KeyCode mock_keycode_for(KeySym ks) {
    for(int i = 0; i < mock_keymap_n; i++) {
        if(mock_keymap[i].ks == ks && mock_keymap[i].group == 0 && mock_keymap[i].shift == 0) {
            return mock_keymap[i].code;
        }
    }
    return 0;
}

/**
 * Раскладка с модификаторами и локами — как на настоящей клавиатуре.
 * Коды клавиш берутся из таблицы (mock_keymap_set_all_groups() для модификаторов),
 * без них добавляются синтетические в свободные слоты.
 */
static void mock_modifiers_fill() {
    memset(mock_modifier_codes, 0, sizeof mock_modifier_codes);

    struct {
        int index;
        KeySym ks;
    } slots[] = {
        {MOCK_SHIFT_INDEX, XK_Shift_L},
        {MOCK_LOCK_INDEX, XK_Caps_Lock},
        {MOCK_CONTROL_INDEX, XK_Control_L},
        {MOCK_MOD2_INDEX, XK_Num_Lock},
        {MOCK_MOD1_INDEX, XK_Alt_L},
        {MOCK_MOD3_INDEX, XK_Meta_L},
        {MOCK_MOD4_INDEX, XK_Super_L},
        {MOCK_MOD5_INDEX, XK_Hyper_L},
    };

    for(size_t i = 0; i < sizeof slots / sizeof slots[0]; i++) {
        KeyCode code = mock_keycode_for(slots[i].ks);
        if(!code) {
            code = 200 + i; // свободные сканкоды: выше всех обычных клавиш
            mock_keymap_set_all_groups(code, slots[i].ks);
        }
        mock_modifier_codes[slots[i].index][0] = code;
    }
}

static XModifierKeymap mock_modifier_map;

XModifierKeymap* XGetModifierMapping(Display* dpy) {
    mock_push_int("XGetModifierMapping(", 0, 0, 0, 0);
    (void) dpy;
    memset(&mock_modifier_map, 0, sizeof mock_modifier_map);
    mock_modifier_map.max_keypermod = MOCK_MODIFIER_PER_SLOT;
    mock_modifier_map.modifiermap = &mock_modifier_codes[0][0];
    return &mock_modifier_map;
}

int XFreeModifiermap(XModifierKeymap* map) {
    mock_push_int("XFreeModifiermap(", 0, 0, 0, 0);
    (void) map; // карта односторонняя, в куче не живёт
    return 1;
}

int XNextEvent(Display* dpy, XEvent* event) {
    mock_push_int("XNextEvent(", 0, 0, 0, 0);
    (void) dpy;
    if(event) memset(event, 0, sizeof *event);
    return 0;
}

Status XSendEvent(Display* dpy, Window win, Bool propagate, long mask, XEvent* event) {
    mock_push_int("XSendEvent(", (int) win, propagate, (int) mask, 0);
    (void) dpy;
    (void) event;
    return 1;
}

#endif /* __ERSWITCHER_TESTS_UNIT_MOCK__H__ */
