#include <xkbcommon/xkbcommon.h>
#include <unistd.h>
#include <string.h>
#include <X11/extensions/XTest.h>

#include "keyboard.h"
#include "erswitcher.h"

typedef struct {
	int code;
	int mods;
	int group;
} unikey_t;

// ответ для get_key()
unikey_t key;

#define KEYBOARD_SIZE	(32*8)

// набор клавиатур: keyboard[group][shift][scancode] -> utf32
KeySym keyboard[XkbNumKbdGroups][2][KEYBOARD_SIZE];

int groups = 0;			// Количество раскладок
int group_ru = -1;		// Номер русской раскладки или -1
int group_en = -1;		// Номер английской раскладки или -1

// инициализирует массив keyboard
void keysym_init() {
	init_keyboard(d);
	for(int group = 0; group < groups; ++group)
	for(int code = 0; code < KEYBOARD_SIZE; ++code)
	for(int shift = 0; shift < 2; ++shift)
		keyboard[group][shift][code] = XkbKeycodeToKeysym(d, code, group, shift);
}

// сопоставляет сканкод, раскладку и модификатор символу юникода
// устанавливает key
int get_key(wint_t cs) {
	KeySym ks = xkb_utf32_to_keysym(cs);

	for(int group = 0; group < groups; ++group)
	for(int code = 0; code < KEYBOARD_SIZE; ++code) 
	for(int shift = 0; shift < 2; ++shift) {
		if(keyboard[group][shift][code] != ks) continue;
		key.code = code;
		key.group = group;
		key.mods = shift? ShiftMask: 0;
		return 1;
	}
	return 0;
}

// сопоставляет символ из другой раскладки
wint_t translate(wint_t cs, int to_group) {
	if(!get_key(cs)) return 0;
	return xkb_keysym_to_utf32(keyboard[to_group][key.mods? 1: 0][key.code]);
}

// Переключение раскладки
void set_group(int group) {
    XkbLockGroup(d, XkbUseCoreKbd, group);
    get_group();	// без этого вызова в силу переключение не вступит
}

// возвращает текущую раскладку
int get_group() {
	XkbStateRec state;
    XkbGetState(d, XkbUseCoreKbd, &state);
    return state.group;	
}

// возвращает модификаторы
int get_mods() {
	XkbStateRec state;
    XkbGetState(d, XkbUseCoreKbd, &state);
    return state.mods;
}

// устанавливает модификаторы
void set_mods(int mods) {
	int active = get_mods();
	if(active) {
		XkbLockModifiers(d, XkbUseCoreKbd, active, 0);
		usleep(10000);
	}
	if(mods) {
		XkbLockModifiers(d, XkbUseCoreKbd, mods, 1);
		usleep(10000);
	}
}

// инициализирует названия клавиатуры
static char* Russian = "Russian";
static char* English = "English";
void init_keyboard(Display* d) {
	XkbDescRec* kb = XkbAllocKeyboard();
	if(!kb) return;

	kb->dpy = d;
	if(XkbGetNames(d, XkbGroupNamesMask, kb) != Success) return;

	Atom* gs = kb->names->groups;
	for(groups = 0; groups < XkbNumKbdGroups && gs[groups] != 0;) {
		char* kb_name = XGetAtomName(d, gs[groups]);
		if(strncmp(kb_name, Russian, strlen(Russian)) == 0) group_ru = groups;
		if(strncmp(kb_name, English, strlen(English)) == 0) group_en = groups;
		groups++;
	}

	XkbFreeNames(kb, XkbGroupNamesMask, 0);
}

// эмулирует ввод текста
void type(wchar_t* s) {
    int current_group = get_group();
    int current_mods = get_mods();

    while(*s) press_key(*s++);

    set_group(current_group);
    set_mods(current_mods);
}

// Эмулирует нажатие и отжатие клавиши
void press_key(wint_t cs) {
	send_key(cs, 1);
	send_key(cs, 0);
}

// Эмулирует нажатие или отжатие клавиши
void send_key(wint_t cs, int is_press) {
	if(!get_key(cs)) return;

	set_group(key.group);
    set_mods(key.mods);

    XTestFakeKeyEvent(d, key.code, is_press, CurrentTime);
    
    XSync(d, False);
    XFlush(d);
    usleep(DELAY);
}