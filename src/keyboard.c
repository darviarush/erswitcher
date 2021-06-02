#include <xkbcommon/xkbcommon.h>
#include <unistd.h>
#include <string.h>
#include <X11/extensions/XTest.h>

#include "keyboard.h"

#define KEYBOARD_SIZE	(32*8)
#define MAX_KEYSYM		(KEYBOARD_SIZE*XkbNumKbdGroups)

unikey_t keysym[MAX_KEYSYM];		// символы юникода, которые есть в установленных в системе раскладках клавиатуры
int keysyms = 0;	// Количество символов
int groups = 0;		// Количество раскладок
int group_ru;		// Номер русской раскладки или -1
int group_en;		// Номер английской раскладки или -1

// добавляет символ, если его ещё нет
void add_keysym(KeySym ks, int code, int group, int mods) {
	wint_t cs = xkb_keysym_to_utf32(ks);

	if(cs == 0) return;
	for (int i=0; i<keysyms; ++i) if(keysym[i].key == cs) return;

	keysym[keysyms].key = cs;
	keysym[keysyms].code = code;
	keysym[keysyms].group = group;
	keysym[keysyms].mods = mods;
	keysyms++;
}

// инициализирует массив и его длину
void keysym_init() {
	init_keyboard(d);
	for(int group = 0; group < groups; ++group)
	for(int code = 0; code < KEYBOARD_SIZE; ++code) {
		KeySym ks = XkbKeycodeToKeysym(d, code, group, 0);
		add_keysym(ks, code, group, 0);
		ks = XkbKeycodeToKeysym(d, code, group, 1);
		add_keysym(ks, code, group, ShiftMask);
	}
}

// возвращает указатель на символ по коду или NULL
unikey_t* get_key(wchar_t cs) {
	for (int i=0; i<keysyms; ++i) if(keysym[i].key == cs) return keysym+i;
	return NULL;
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
	XkbLockModifiers(d, XkbUseCoreKbd, active, 0);
	usleep(10000);
	XkbLockModifiers(d, XkbUseCoreKbd, mods, 1);
	usleep(10000);
	get_mods();
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
void press_key(wchar_t cs) {
	send_key(cs, 1);
	send_key(cs, 0);
}

// Эмулирует нажатие или отжатие клавиши
void send_key(wchar_t cs, int is_press) {
	unikey_t* key = get_key(cs);
	if(key == NULL) return;

	set_group(key->group);
    set_mods(key->mods);

    XTestFakeKeyEvent(d, key->code, is_press, CurrentTime);
    
    XSync(d, False);
    XFlush(d);
    usleep(DELAY);
}
