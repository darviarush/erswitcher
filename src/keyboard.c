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

int delay = DELAY;

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
int get_key(KeySym ks) {
	for(int group = 0; group < groups; ++group)
	for(int code = 0; code < KEYBOARD_SIZE; code++) 
	for(int shift = 0; shift < 2; shift++) {
		if(keyboard[group][shift][code] != ks) continue;
		key.code = code;
		key.group = group;
		key.mods = shift? ShiftMask: 0;
		return 1;
	}
	return 0;
}

// сопоставляет символ из другой раскладки
KeySym translate(KeySym ks) {
	if(!get_key(ks)) return NoSymbol;
	int group =
		key.group == group_ru? group_en:
		key.group == group_en? group_ru:
		key.group;

	KeySym to_ks = keyboard[group][key.mods? 1: 0][key.code];
	if(to_ks == NoSymbol) return ks;
	return to_ks;
}

// void XConvertCase(KeySym keysym, KeySym *lower_return, KeySym *upper_return);
// переводит символ в верхний регистр
KeySym upper(KeySym ks) {
	if(!get_key(ks)) return NoSymbol;

	KeySym to_ks = keyboard[key.group][1][key.code];
	if(to_ks == NoSymbol) return ks;
	return to_ks;
}

// переводит символ в нижний регистр
KeySym lower(KeySym ks) {
	if(!get_key(ks)) return NoSymbol;

	KeySym to_ks = keyboard[key.group][0][key.code];
	if(to_ks == NoSymbol) return ks;
	return to_ks;
}

// Переключает раскладку
void set_group(int group) {
	if(get_group() == group) return;	
    XkbLockGroup(d, XkbUseCoreKbd, group);
    get_group();	// без этого вызова в силу переключение не вступит
    printf("set_group: %i\n", group);
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

// // устанавливает модификаторы
// void set_mods(int mods) {
// 	int active = get_mods();
	
// 	if(active == mods) return;

// 	if(active) {
// 		int res = XkbLockModifiers(d, XkbUseCoreKbd, active, 0);
// 		printf("XkbLockModifiers(active) res: %i xerror: %i active: %i\n", res, xerror, active);
// 	}
// 	if(mods) {
// 		int res = XkbLockModifiers(d, XkbUseCoreKbd, mods, 1);
// 		printf("XkbLockModifiers(mods) res: %i xerror: %i mods: %i\n", res, xerror, mods);
// 	}

// 	XSync(d, False);
// }

// Эмулирует нажатие и отжатие клавиши
void press(int code, int is_press) {
	printf("   press: %i %s\n", code, is_press? "PRESS": "RELEASE");
	XTestFakeKeyEvent(d, code, is_press, CurrentTime);
    XSync(d, False);
}

// Эмулирует нажатие и отжатие клавиши
void press_key(KeySym ks) {
	send_key(ks, 1);
	send_key(ks, 0);
}

// Эмулирует нажатие или отжатие клавиши
void send_key(KeySym ks, int is_press) {
	const char *s = XKeysymToString(ks);

	if(!get_key(ks)) {
		printf("send_key: No Key `%s`!\n", s);
		return;
	}

	set_group(key.group);
	send_mods(key.mods, is_press);
	printf("send_key: %s %s\n", s, is_press? "PRESS": "RELEASE");
	press(key.code, is_press);

    XFlush(d);
    if(delay) usleep(delay/2);
}

// возвращает модификатры с кнопками мыши
unsigned int get_input_state() {
  Window root, dummy;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;
  root = DefaultRootWindow(d);

  XQueryPointer(d, root, &dummy, &dummy,
                &root_x, &root_y, &win_x, &win_y, &mask);

  return mask;
}

// устанавливает модификаторы
void send_mods(int mods, int is_press) {
  XModifierKeymap *modifiers = XGetModifierMapping(d);

  for (int mod_index = ShiftMapIndex; mod_index <= Mod5MapIndex; mod_index++) {
    if(mods & (1 << mod_index)) {
      for(int mod_key = 0; mod_key < modifiers->max_keypermod; mod_key++) {
        int code = modifiers->modifiermap[mod_index * modifiers->max_keypermod + mod_key];
        if(code) {
			press(code, is_press);
			//if(delay) usleep(delay/2);
			break;
        }
      }
    }
  }

  XFreeModifiermap(modifiers);
}

// Возвращает активные модификаторы клавиатуры
// keys - массив клавиш KEYBOARD_SIZE = 32*8
// возвращчет количество найденных клавиш-модификаторов
int get_active_mods(int *keys) {
  char keymap[32]; /* keycode map: 256 bits */
  XModifierKeymap *modifiers = XGetModifierMapping(d);
  int nkeys = 0;

  XQueryKeymap(d, keymap);

  for(int mod_index = ShiftMapIndex; mod_index <= Mod5MapIndex; mod_index++) {
    for(int mod_key = 0; mod_key < modifiers->max_keypermod; mod_key++) {
      int code = modifiers->modifiermap[mod_index * modifiers->max_keypermod + mod_key];
      if(code && BIT(keymap, code)) keys[nkeys++] = code;
    }
  } 

  XFreeModifiermap(modifiers);

  return nkeys;
}

void clear_active_mods(int* keys, int nkeys) {
	for(int i = 0; i<nkeys; i++) press(keys[i], 0);

	unsigned int state = get_input_state();
	if(state & LockMask) press_key(XK_Caps_Lock);
}

void set_active_mods(int* keys, int nkeys) {
	for(int i = 0; i<nkeys; i++) press(keys[i], 1);

	unsigned int state = get_input_state();
	if(state & LockMask) {
		press_key(XK_Caps_Lock);
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


#define TYPE_BEGIN		\
		int active_mods[KEYBOARD_SIZE];		\
		int nactive_mods = get_active_mods(active_mods); 	\
		clear_active_mods(active_mods, nactive_mods);		\
		int active_group = get_group();						\
		int active_state = get_mods();

#define TYPE_END	\
		set_group(active_group);					\
		set_active_mods(active_mods, nactive_mods);	\
		if(active_state & LockMask) press_key(XK_Caps_Lock);

// эмулирует ввод текста в utf8
// контролирующие клавиши вводятся в фигурных скобках {Caps_Lock}{Ctrl+X}
// для ввода фигурных скобок используется
void type(char* s) {
    int current_group = get_group();
    //int current_mods = get_mods();

    while(*s) press_key(*s++);

    set_group(current_group);
    //set_mods(current_mods);
}

// набирает на клаве текст в utf8
void keyboard_write(char* s) {
	TYPE_BEGIN;
	mbstate_t ps = {0};
	wchar_t dest[4];
	const char* src = s;
	int res;
	while((res = mbsrtowcs(dest, &src, 1, &ps))>0) {
		press_key(xkb_utf32_to_keysym(((wint_t*) dest)[0]));
	}
	TYPE_END;
}

// набирает на клаве текст в utf32
void keyboard_write32(wint_t* s) {
	TYPE_BEGIN;
	while(*s) press_key(xkb_utf32_to_keysym(*s++));
	TYPE_END;
}
