/**************************************************************
 * Приложение: erswitcher - переключатель клавиатурного ввода *
 * Автор: Ярослав О. Косьмина                                 *
 * Лицензия: GPLv3                                            *
 * Местонахождение: https://github.com/darviarush/erswitcher  *
 **************************************************************/

#include <locale.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/XKB.h>
#include <X11/extensions/XTest.h>
#include <xkbcommon/xkbcommon.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/stat.h>

typedef struct {
	unsigned code:8;
	unsigned mods:8;
	unsigned group:8;
} unikey_t;

#define KEY_TO_SYM(K)		unikey_to_keysym(K)
#define KEY_TO_INT(K)		xkb_keysym_to_utf32(unikey_to_keysym(K))
#define SYM_TO_KEY(K)		keysym_to_unikey(K)
#define SYM_TO_INT(S)		xkb_keysym_to_utf32(S)
#define INT_TO_SYM(S)		xkb_utf32_to_keysym(S)
#define INT_TO_KEY(K)		keysym_to_unikey(xkb_utf32_to_keysym(K))

#define SYM_TO_STR(K)		XKeysymToString(K)
#define KEY_TO_STR(K)		XKeysymToString(KEY_TO_SYM(K))
#define INT_TO_STR(K)		XKeysymToString(INT_TO_SYM(K))

// Установлен ли бит
#define BIT(VECTOR, BIT_IDX)   ( ((char*)VECTOR)[BIT_IDX/8]&(1<<(BIT_IDX%8)) )
// дефолтная задержка
#define DELAY 		    10000
#define KEYBOARD_SIZE   (32*8)
// размер буфера word
#define BUF_SIZE	1024


Display *d;					// текущий дисплей
Window current_win;			// окно устанавливается при вводе с клавиатуры
int delay = DELAY;          // задержка
char keyboard_buf1[32], keyboard_buf2[32];
char *saved=keyboard_buf1, *keys=keyboard_buf2;
int pos = 0;
unikey_t word[BUF_SIZE];
Atom sel_data_atom;
Atom utf8_atom;
Atom clipboard_atom;

int groups = 0;			// Количество раскладок
int group_ru = -1;		// Номер русской раскладки или -1
int group_en = -1;		// Номер английской раскладки или -1

// typedef struct {
	
// } modkey_t;

// modkey_t keyboard_mods[8];

// инициализирует названия клавиатуры
static char* Russian = "Russian";
static char* English = "English";
void init_keyboard() {
	
	// инициализируем раскладки клавиатуры
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
	
	// TODO: инициализируем модификаторы
	// в иксах есть 8 модификаторов. И на них можно назначить разные модифицирующие или лочащиеся клавиши
	// int i, k = 0;
	// int min_keycode, max_keycode, keysyms_per_keycode = 0;

    // XDisplayKeycodes (dpy, &min_keycode, &max_keycode);
    // XGetKeyboardMapping (dpy, min_keycode, (max_keycode - min_keycode + 1), &keysyms_per_keycode);
	
	// for (i = 0; i < 8; i++) {
		// for (int j = 0; j < map->max_keypermod; j++) {
			// if (map->modifiermap[k]) {
				// KeySym ks;
				// int index = 0;
				// do { ks = XKeycodeToKeysym(dpy, map->modifiermap[k], index++); } while ( !ks && index < keysyms_per_keycode);
				// char* nm = XKeysymToString(ks);

				// //fprintf (fp, "%s  %s (0x%0x)", (j > 0 ? "," : ""), (nm ? nm : "BadKey"), map->modifiermap[k]);
			// }
			// k++;
		// }
    // }
}

KeySym unikey_to_keysym(unikey_t key) {
	KeySym ks = XkbKeycodeToKeysym(d, key.code, key.group, key.mods & ShiftMask? 1: 0);
	if(ks == NoSymbol) ks = XkbKeycodeToKeysym(d, key.code, group_en, key.mods & ShiftMask? 1: 0);
	if(key.mods & LockMask) { 
		KeySym lower, upper;
		XConvertCase(ks, &lower, &upper);
		if(key.mods & ShiftMask) ks = lower;
		else ks = upper;
	} 
	return ks;
}

unikey_t keyboard_state(int code) {
	XkbStateRec state;
    XkbGetState(d, XkbUseCoreKbd, &state);
	return (unikey_t) {code: code, mods: state.mods, group: state.group};
}

// с какой группы начинать поиск сканкода по символу
// после поиска maybe_group переключается в группу найденного символа
int maybe_group = -1;

unikey_t keysym_to_unikey(KeySym ks) {
	// вначале ищем в текущей раскладке
	//unikey_t key = keyboard_state(XKeysymToKeycode(d, ks));
	//if(key.code) return key;
	
	for(int code = 0; code < KEYBOARD_SIZE; ++code)
	for(int shift = 0; shift < 2; ++shift) {
		KeySym search_ks = XkbKeycodeToKeysym(d, code, maybe_group, shift);
		if(ks == search_ks)	return (unikey_t) {code: code, mods: shift? ShiftMask: 0, group: maybe_group};
	}
	
	// чтобы не переключать клавиатуру и shift пойдём от обратного:
	for(int group = 0; group < groups; ++group)
	for(int code = 0; code < KEYBOARD_SIZE; ++code)
	for(int shift = 0; shift < 2; ++shift) {
		KeySym search_ks = XkbKeycodeToKeysym(d, code, group, shift);
		if(ks == search_ks)	{
			maybe_group = group;
			return (unikey_t) {code: code, mods: shift? ShiftMask: 0, group: group};
		}
	}
	
	return (unikey_t) {code: 0, mods: 0, group: 0};
}

// маски модификаторов и локов. На некоторых клавиатурах разные модификаторы могут иметь одну и ту же маску. И их нельзя будет различить 
// int shift_mask = 1 << 0;
// int lock_mask = 1 << 1;
// int control_mask = 1 << 2;
// int alt_mask = 1 << 3;
// int num_mask = 1 << 4;
// int meta_mask = 1 << 5;
// int meta_mask = 1 << 5;

#define AltMask			Mod1Mask
#define NumMask			Mod2Mask
#define MetaMask		Mod3Mask
#define SuperMask		Mod4Mask
#define HyperMask		Mod5Mask
void print_sym(int mods, KeySym ks) {
	int begin = 0;
	for(int i=0; i<32; i++) {
		int mask = mods & (1<<i);
		if(!mask) continue;
		if(begin) printf("+"); else begin = 1;
		switch(mask) {
			case ShiftMask: printf("⇧Shift"); break;
			case LockMask: printf("⇪🄰CapsLock"); break;// ⇪🄰
			case ControlMask: printf("⌃⎈Ctrl"); break;
			case Mod1Mask: printf("⌥⎇Alt"); break;
			case Mod2Mask: printf("⇭NumLock"); break; // ⓛ🄸
			case Mod3Mask: printf("❖Meta"); break;
			case Mod4Mask: printf("⌘⊞❖Super"); break;
			case Mod5Mask: printf("✦Hyper"); break;
			default:
				printf("Mod%i", i);
		}
	}

	if(begin) printf("+");
	//wint_t cs = xkb_keysym_to_utf32(ks);
	//if(cs) printf("%C", cs);
	//else 
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

// входит в массив
int in_sym(KeySym symbol, KeySym* symbols) {
	while(*symbols) if(symbol == *symbols++) return 1;
	return 0;
}

// Обработка ошибок
int xerror = 0;
static int null_X_error(Display *d, XErrorEvent *err) {
	xerror = True;
	#define BUFLEN		(1024*64)
	char buf[BUFLEN];
    XGetErrorText(d, err->error_code, buf, BUFLEN);
	printf("!!!!!!!!!!!!! X ERROR !!!!!!!!!!!!! %s\n", buf);
    return False;
}

// подключается к дисплею
void open_display() {
	d = XOpenDisplay(NULL);
	if(!d) { fprintf(stderr, "Not open display!\n"); exit(1); }

	XSetErrorHandler(null_X_error);

	XSynchronize(d, True);
	
	sel_data_atom = XInternAtom(d, "XSEL_DATA", False);
	utf8_atom = XInternAtom(d, "UTF8_STRING", True);
	clipboard_atom = XInternAtom(d, "CLIPBOARD", False);
}

// возвращает текущее окно
Window get_current_window() {
	Window w = 0;
  	int revert_to = 0;
	XGetInputFocus(d, &w, &revert_to);
	return w;
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

// Переключает раскладку
void set_group(int group) {
	if(keyboard_state(0).group == group) return;	
    XkbLockGroup(d, XkbUseCoreKbd, group);
    XkbStateRec state;
    XkbGetState(d, XkbUseCoreKbd, &state);	// без этого вызова в силу переключение не вступит
    printf("set_group: %i\n", group);
}

// Эмулирует нажатие или отжатие клавиши
void press(int code, int is_press) {
	unikey_t key = keyboard_state(code);
	printf("   press: %i ", code);
	print_sym(key.mods, KEY_TO_SYM(key));
	printf("%s\n", is_press? "PRESS": "RELEASE");
	XTestFakeKeyEvent(d, code, is_press, CurrentTime);
    XSync(d, False);
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

// Эмулирует нажатие или отжатие клавиши
void send_key(unikey_t key, int is_press) {
	if(key.code == 0) {
		printf("send_key: No Key!\n");
		return;
	}

	set_group(key.group);
	send_mods(key.mods, is_press);
	printf("send_key: %s %s\n", KEY_TO_STR(key), is_press? "PRESS": "RELEASE");
	press(key.code, is_press);

    XFlush(d);
    if(delay) usleep(delay/2);
}

// Эмулирует нажатие и отжатие клавиши
void press_key(unikey_t key) {
	send_key(key, 1);
	send_key(key, 0);
}

int active_codes[KEYBOARD_SIZE];
int active_len;
unikey_t active_state;
int active_use = 0;

// Возвращает активные модификаторы клавиатуры
// keys - массив клавиш KEYBOARD_SIZE = 32*8
// возвращчет количество найденных клавиш-модификаторов
void clear_active_mods() {
	if(active_use) {
		fprintf(stderr, "ABORT: Вложенный clear_active_mods\n");
		exit(1);
	}
	active_use++;
	
	init_keyboard();
	
	active_state = keyboard_state(0);
	
	XModifierKeymap *modifiers = XGetModifierMapping(d);

	active_len = 0;

	for(int mod_index = ShiftMapIndex; mod_index <= Mod5MapIndex; mod_index++) {
		for(int mod_key = 0; mod_key < modifiers->max_keypermod; mod_key++) {
			int code = modifiers->modifiermap[mod_index * modifiers->max_keypermod + mod_key];
			if(code && BIT(keys, code)) active_codes[active_len++] = code;
		}
	} 

	XFreeModifiermap(modifiers);

	// отжимаем модификаторы
	for(int i = 0; i<active_len; i++) press(active_codes[i], 0);

	// снимаем Капс-лок (он не входит в модификаторы)
	if(active_state.mods & LockMask) {
		press_key(SYM_TO_KEY(XK_Caps_Lock));
	}
}

// восстанавливаем модификаторы
void recover_active_mods() {
	
	// снимаем нажатые модификаторы
	unikey_t state = keyboard_state(0);
	send_mods(state.mods, 0);
	
	char active_keys[32];
	XQueryKeymap(d, active_keys);
	for(int i=0; i<active_len; i++) {
		int code = active_codes[i];
		// клавиша не была отжата пока происходил ввод
		if(BIT(keys, code)==BIT(active_keys, code)) {
			press(code, 1);
		}
	}
	
	// восстанавливаем Капс-лок (он не входит в модификаторы)
	if(active_state.mods & LockMask && !(state.mods & LockMask)) {
		press_key(SYM_TO_KEY(XK_Caps_Lock));
	}
	
	set_group(active_state.group);
	
	active_use--;
}

void add_to_buffer(unikey_t key) {
	// Если нажаты какие-то ещё модификаторы, контрол или альт - выходим
	if(key.mods & ~(ShiftMask|LockMask|NumMask)) {
		return;
	}

	KeySym ks = KEY_TO_SYM(key);
	
	// Если это переход на другую строку, то начинаем ввод с начала
	KeySym is_control[] = {XK_Home, XK_Left, XK_Up, XK_Right, XK_Down, XK_Prior, XK_Page_Up, XK_Next, XK_Page_Down, XK_End, XK_Begin, XK_Tab, XK_Return, 0};
	if(in_sym(ks, is_control)) {
		pos = 0;
		return;
	}
	
	wint_t cs = SYM_TO_INT(ks);
	
	// Если символ не печатный, то пропускаем
	KeySym is_print[] = {XK_space, XK_underscore, 0};
	if(!iswprint(cs) && !in_sym(ks, is_print)) {
		return; 
	}
	
	// Записываем символ в буфер с его раскладкой клавиатуры
	if(pos >= BUF_SIZE) pos = 0;
	//word[pos++] = {code: code, mods: state.mods, group: state};
	word[pos++] = key;

	//printf("add_to_buffer: %S\n", word);
}

int from_space() {
	if(pos == 0) return 0;
	int from=pos-1;
	for(; from>0 && iswspace(KEY_TO_INT(word[from])); from--);
	for(; from>0; from--) if(iswspace(KEY_TO_INT(word[from]))) break;
	return from;
}

void press_key_multi(unikey_t key, int n) {
	for(int i=0; i<n; i++) press_key(key);
}

void sendkeys(char* s) { // печатает с клавиатуры строку в utf8
	clear_active_mods();
	
	mbstate_t mbs = {0};
	for(size_t charlen, i = 0;
        (charlen = mbrlen(s+i, MB_CUR_MAX, &mbs)) != 0
        && charlen > 0
        && i < BUF_SIZE;
        i += charlen
    ) {
        wchar_t ws[4];
        int res = mbstowcs(ws, s+i, 1);
        if(res != 1) break;

		press_key(INT_TO_KEY(ws[0]));
    }

	recover_active_mods();
}

void print_translate_buffer(int from, int backspace) {
	//word[pos] = 0;
	//printf("print_translate_buffer: %S\n", word+from);

	clear_active_mods();
	int trans_group = active_state.group == group_en? group_ru: group_en;
	
	// отправляем бекспейсы, чтобы удалить ввод
	if(backspace) press_key_multi(SYM_TO_KEY(XK_BackSpace), pos-from);
	
	// вводим ввод, но в альтернативной раскладке
	for(int i=from; i<pos; i++) {
		word[i].group = word[i].group == group_en? group_ru: word[i].group == group_ru? group_en: word[i].group;
		press_key(word[i]);
	}

	recover_active_mods();
	
	// меняем group раскладку
	set_group(trans_group);
}

void print_invertcase_buffer(int from, int backspace) {
	//word[pos] = 0;
	//printf("print_invertcase_buffer: %S\n", word+from);

	clear_active_mods();
	
	// отправляем бекспейсы, чтобы удалить ввод
	if(backspace) press_key_multi(SYM_TO_KEY(XK_BackSpace), pos-from);
	
	// вводим ввод, но в альтернативной раскладке
	for(int i=from; i<pos; i++) {
		word[i].mods = word[i].mods & ShiftMask? word[i].mods & ~ShiftMask: word[i].mods | ShiftMask;
		press_key(word[i]);
	}

	recover_active_mods();
}

// получаем выделение
char* get_selection(Atom number_buf) {
		
	// Создаём окно
	int black = BlackPixel(d, DefaultScreen(d));
	int root = XDefaultRootWindow(d);
	Window w = XCreateSimpleWindow(d, root, 0, 0, 1, 1, 0, black, black);

	// подписываемся на события окна
	XSelectInput(d, w, PropertyChangeMask);

	// запрос на получение выделенной области
	Atom request_target = utf8_atom? utf8_atom: XA_STRING;

	XConvertSelection(d, number_buf,
		request_target, 
		sel_data_atom, 
		w,
	    CurrentTime
	);
	XSync(d, False);
	
	// строка которую получим
	char* s = NULL;
	int format;	// в этом формате
	unsigned long bytesafter, length;
	Atom target;

	// получаем событие
	while(1) {
		XEvent event;
		XNextEvent(d, &event);

		// пришло какое-то другое событие... ну его
		if(event.type != SelectionNotify) continue;
		if(event.xselection.selection != number_buf) continue;

		// нет выделения
		if(event.xselection.property == None) break;

		// считываем
		XGetWindowProperty(event.xselection.display,
			    event.xselection.requestor,
			    event.xselection.property, 0L, 1000000,
			    False, (Atom) AnyPropertyType, &target,
			    &format, &length, &bytesafter, (unsigned char**) &s);

		break;
	}

	XDestroyWindow(d, w);

	return s;
}

void to_buffer(char** s1) {
	char* s = *s1;
	pos = 0;
	if(s == NULL) return;

	//printf("to_buffer: %s\n", s);

	// utf8 переводим в символы юникода, затем в символы x11, после - в сканкоды
	mbstate_t mbs = {0};
	for(size_t charlen, i = 0;
        (charlen = mbrlen(s+i, MB_CUR_MAX, &mbs)) != 0
        && charlen > 0
        && i < BUF_SIZE;
        i += charlen
    ) {
        wchar_t ws[4];
        int res = mbstowcs(ws, s+i, 1);
        if(res != 1) break;

		word[pos++] = INT_TO_KEY(ws[0]);
    }

	XFree(s);
	*s1 = NULL;
}

void copy_selection() {
	clear_active_mods();
	int save = delay;
	delay = 0;
	unikey_t control_left = SYM_TO_KEY(XK_Control_L);
	send_key(control_left, 1);
	press_key(SYM_TO_KEY(XK_Insert));
	send_key(control_left, 0);
	delay = save;
	recover_active_mods();

	char* s = get_selection(clipboard_atom);
	maybe_group = active_state.group;
	to_buffer(&s);
}

void change_key(int code) {
    
	unikey_t key = keyboard_state(code);

    KeySym ks = KEY_TO_SYM(key);
    
	// XkbKeysymToModifiers()
	unsigned int state = get_input_state();
	printf("change_key %i (", code);
	for(int i=0; i<32; i++) if(state & (1<<i)) printf("%i", i); else printf(" ");
	printf(") ");
	if(key.group == group_en) printf("en");
	else if(key.group == group_ru) printf("ru");
	else printf("%i", key.group);
	printf(": ");
	print_sym(key.mods, ks);
	printf("\n");
	fflush(stdout);
    // нажата комбинация? выполняем действие
	int mods = key.mods & ~(LockMask|NumMask);
	
	if(ks == XK_Pause && mods == SuperMask) {
		sendkeys("Готово.");
	}
	else if(ks == XK_BackSpace && mods == 0) {
		if(pos != 0) --pos;
	}
	else if(ks == XK_Pause && mods == 0) {
		print_translate_buffer(from_space(), 1);
	}
	else if(ks == XK_Pause && mods == ControlMask) {
		print_translate_buffer(0, 1);
	}
	else if(ks == XK_Break && mods == ShiftMask) {
		copy_selection();
		// to_buffer очищает память выделенную для s через XFree
		print_translate_buffer(0, 0);
	}
	else if(ks == XK_Break && mods == (AltMask|ShiftMask)) {
		print_invertcase_buffer(from_space(), 1);
	}
	else if(ks == XK_Pause && mods == (AltMask|ControlMask)) {
		print_invertcase_buffer(0, 1);
	}
	else if(ks == XK_Pause && mods == AltMask) {
		copy_selection();
		print_invertcase_buffer(0, 0);
	}
	else {
		// заносим в буфер
        add_to_buffer(key);
    }    
}

void init_desktop(char* av0) {
	// FILE* f = fopen("/usr/share/application/erswitcher.desktop", "rb");
	// if(f) {fclose(f); return;}
	
	char* program = realpath(av0, NULL);
	if(!program) {fprintf(stderr, "WARN: нет пути к программе\n"); return;}
	
	#define INIT_DESKTOP_FREE	free(program)
	
	const char* home = getenv("HOME");
	if(!home) {fprintf(stderr, "WARN: нет getenv(HOME)\n");	INIT_DESKTOP_FREE; return;}
	if(chdir(home)) {fprintf(stderr, "WARN: нет каталога пользователя %s\n", home);	INIT_DESKTOP_FREE; return;}
		
	mkdir(".local", 0700);
	mkdir(".local/share", 0700);
	mkdir(".local/share/applications", 0700);
	
	const char* app = ".local/share/applications/erswitcher.desktop";
	
	FILE* f = fopen(app, "wb");
	if(!f) fprintf(stderr, "WARN: не могу создать %s/%s\n", home, app);
	else {
		fprintf(f,
			"[Desktop Entry]\n"
			"Encoding=UTF-8\n"
			"Terminal=false\n"
			"Type=Application\n"
			"Name=EN-RU Switcher\n"
			"Comment=Transliterator for keyboard input\n"
			"GenericName[ru]=Русско-английский клавиатурный переключатель\n"
			"Comment[ru]=Транслитератор ввода с клавиатуры\n"
			"Exec=%s\n"
			"X-GNOME-Autostart-enabled=true\n"
			"Icon=preferences-desktop-keyboard\n"
			"Categories=Keyboard;Transliterate;Development;System;\n",
			program
		);
		
		fclose(f);
	}
	
	mkdir(".config", 0700);
	mkdir(".config/autostart", 0700);
	
	const char* autostart = ".config/autostart/erswitcher.desktop";
	
	f = fopen(autostart, "wb");
	if(!f) {fprintf(stderr, "WARN: не могу создать %s/%s\n", home, autostart); INIT_DESKTOP_FREE; return;}
	
	fprintf(f,
		"[Desktop Entry]\n"
		"Name=EN-RU Switcher\n"
		"Comment=Transliterator for keyboard input\n"
		"Exec=%s\n"
		"Icon=preferences-desktop-keyboard\n"
		"Terminal=false\n"
		"Type=Application\n"
		"Categories=Keyboard;Transliterate;Development;System;\n"
		"Keywords=keyboard;transliterate;erswitcher;switch\n"
		"X-GNOME-UsesNotifications=t\n",
		program
	);
	
	fclose(f);
	
	INIT_DESKTOP_FREE;
}

void check_any_instance() {
	
}

void load_config(char* path) {
	FILE* f = fopen(path, "rb");
	if(!f) {
		f = fopen(path, "wb");
		if(!f) {
			fprintf(stderr, "WARN: не могу создать конфиг `%s`\n", path);
			return;
		}
		
		fprintf(f, 
			"[translate]\n"
			"Pause=word\n"
			"Control+Pause=text\n"
			"Shift+Break=selected\n"
			"[invertcase]\n"
			"Alt+Shift+Break=word\n"
			"Alt+Control+Pause=text\n"
			"Alt+Pause=selected\n"
			"[template]\n"
			"Control+F2=Готово.\n"
		);
		
		fclose(f);
		return;
	}
	
	char buf[BUF_SIZE];
	int lineno = 0;
	while(fgets(buf, BUF_SIZE, f)) {
		lineno++;
		char* s = buf;
		while(isspace(*s)) s++;
	
		if(*s == '#' || *s == '\0') continue;
		
		char* v = strchr(s, '=');
		if(!v) { fprintf(stderr, "WARN: %s:%i: ошибка синтаксиса: нет `=`. Пропущено\n", path, lineno); continue; }
		*v = '\0'; v++;
		
		// комбинации клавиш
		char* x = strtok(s, "+");
		while (x != NULL) {
			//KeySym ks = XStringToKeysym(x);
			
			x = strtok(s, "+");
		};
	}
	fclose(f);
}

int main(int ac, char** av) {
	
	if(!ac) fprintf(stderr, "ERROR: а где путь к программе?\n");
	
	char* locale = "ru_RU.UTF-8";
	if(!setlocale(LC_ALL, locale)) {
		fprintf(stderr, "setlocale(LC_ALL, \"%s\") failed!\n", locale);
        return 1;
	}

	open_display();
	init_desktop(av[0]);
	check_any_instance();

	// Начальные установки
	current_win = get_current_window();
	pos = 0;
	init_keyboard();

    unsigned int state1, state2 = 0;
   	XQueryKeymap(d, saved);

   	while(1) {
        // сбросим ввод, коль нажата мышка
        state1 = get_input_state();
        state1 &= Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask;
        if(state1 != state2) pos = 0;
        state2 = state1;
		
		// Если сменилось окно, то начинаем ввод с начала
		Window w = get_current_window();
		if(w != current_win) {
			pos = 0;
			current_win = w;
		}

        XQueryKeymap(d, keys);
      	for(int i=0; i<KEYBOARD_SIZE; i++) {
      		if(BIT(keys, i)!=BIT(saved, i)) {
      			if(BIT(keys, i)!=0) { // клавиша нажата
      				change_key(i);
      			} else {	// клавиша отжата

      			}
      		}
      	}

      	char* char_ptr=saved;
      	saved=keys;
      	keys=char_ptr;

      	usleep(delay);
   	}

   	return 0;
}