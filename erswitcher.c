/**************************************************************
 * Приложение: erswitcher - переключатель клавиатурного ввода *
 * Автор: Ярослав О. Косьмина                                 *
 * Лицензия: GPLv3                                            *
 * Местонахождение: https://github.com/darviarush/erswitcher  *
 **************************************************************/

#define _GNU_SOURCE

#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XKB.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <ctype.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <xkbcommon/xkbcommon.h>

//@category Отладка

#define DEBUG	1

//@category Клавиши

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

#define STR_TO_SYM(K)		XStringToKeysym(K)
#define STR_TO_INT(K)		SYM_TO_INT(XStringToKeysym(K))
#define STR_TO_KEY(K)		SYM_TO_KEY(XStringToKeysym(K))

#define EQ(S1, S2)			(strcmp(S1, S2) == 0)

// Установлен ли бит
#define BIT(VECTOR, BIT_IDX)   ( ((char*)VECTOR)[BIT_IDX/8]&(1<<(BIT_IDX%8)) )
// размер клавиатуры в битах
#define KEYBOARD_SIZE   (32*8)
// размер буфера word
#define BUF_SIZE	1024

char keyboard_buf1[32], keyboard_buf2[32];			// состояния клавиатуры: предыдущее и текущее. Каждый бит соотвествует клавише на клавиатуре: нажата/отжата
char *saved=keyboard_buf1, *keys=keyboard_buf2;		// для обмена состояний
int pos = 0;				// позиция в буфере символов
unikey_t word[BUF_SIZE];	// буфер символов
int keys_pressed;			// нажато клавишь

//@category Задержки

// дефолтная задержка
#define DELAY 		    10000

int delay = DELAY;          // задержка между программными нажатиями клавишь в микросекундах

typedef struct {
	double time; 			// время в секундах, когда таймер должен сработать
	void(*fn)();
} mytimer_t;

#define MAX_TIMERS			256
mytimer_t timers[MAX_TIMERS];
int timers_size = 0;

int after(double interval, void(*fn)()) {
	int i;
	for(i=0; i<MAX_TIMERS; i++) {
		if(timers[i].fn == NULL) break;
	}

	if(i==MAX_TIMERS) {
		fprintf(stderr, "Очередь таймеров переполнена. Установка таймера не удалась\n");
		return -1;
	}

	timers[i].fn = fn;

	struct timeval curtime;
	gettimeofday(&curtime, 0);
	timers[i].time = curtime.tv_sec + curtime.tv_usec / 1000000 + interval;

	if(timers_size <= i) timers_size = i+1;

	return i;
}

void timers_apply() {
	struct timeval curtime;
	gettimeofday(&curtime, 0);
	double now = curtime.tv_sec + curtime.tv_usec / 1000000;

	int i; int last = 0;
	for(i=0; i<timers_size; i++) {
		if(timers[i].fn == NULL) continue;
		if(timers[i].time < now) {
			if(DEBUG) fprintf(stderr, "fn by timer %p\n", timers[i].fn);
			timers[i].fn();
			timers[i].fn = NULL;
		} else last = i;
	}

	timers_size = last+1;
}

//@category Иксы

Display *d;					// текущий дисплей
Window w;					// окно этого приложения
Window current_win;			// окно устанавливается при вводе с клавиатуры
int selection_chunk_size; 	// максимально возможный размер данных для передачи через буфер обмена
char* clipboard_s = NULL;		// буфер обмена для отправки данных
int clipboard_len = 0;			// длина буфера обмена для отправки данных
char* selection_retrive = NULL; // буфер обмена для получения данных

Atom sel_data_atom;
Atom utf8_atom;
Atom clipboard_atom;
Atom targets_atom;

void event_delay(double delay);

//@category Комбинации -> Функции

typedef struct {
	unikey_t key;
	void (*fn)(char*);
	char* arg;
} keyfn_t;

#define KEYFN_NEXT_ALLOC  256
keyfn_t* keyfn = NULL;
int keyfn_size = 0;
int keyfn_max_size = 0;
int keyfn_active = -1;

int compose_map_size = 0;
wint_t *compose_map_keys = NULL;
wint_t *compose_map_values = NULL;

//@category Раскладки

int groups = 0;			// Количество раскладок
int group_ru = -1;		// Номер русской раскладки или -1
int group_en = -1;		// Номер английской раскладки или -1

// с какой группы начинать поиск сканкода по символу
// после поиска maybe_group переключается в группу найденного символа
int maybe_group = -1;

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
	targets_atom = XInternAtom(d, "TARGETS", False);

	selection_chunk_size = XExtendedMaxRequestSize(d) / 4;
	if(!selection_chunk_size) selection_chunk_size = XMaxRequestSize(d) / 4;

	// Создаём окно
	w = XCreateSimpleWindow(d, XDefaultRootWindow(d), -10, -10, 1, 1, 0, 0, 0);
	// подписываемся на события окна
	XSelectInput(d, w, PropertyChangeMask);
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
	printf(" %s\n", is_press? "PRESS": "RELEASE");
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

// TODO: зажатие управляющих клавишь {\Control+Alt}abc{/Control} и {Ctrl+Alt+a}
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

		//if(ws[0] == '{')

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

// получаем содержимое системного буфера
char* get_selection(Atom number_buf) {

	Window owner = XGetSelectionOwner(d, number_buf);
    if (owner == None) {
		char* name_buf = XGetAtomName(d, number_buf);
        fprintf(stderr, "Буфер %s не поддерживается владельцем\n", name_buf);
		if(name_buf) XFree(name_buf);
        return NULL;
    }
    if(DEBUG) fprintf(stderr, "owner = 0x%lX %s\n", owner, w == owner? "одинаков с w": "разный с w");

	//if(w == owner) return ;

	// требование перевода в utf8:
	XConvertSelection(d, number_buf,
		utf8_atom,
		sel_data_atom,
		w,
	    CurrentTime
	);
	XSync(d, False);

	event_delay(delay / 1000000);

	return selection_retrive;
}

// переписываем строку в буфер ввода
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

// копируем выделенное и заменяем им буфер ввода
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

	usleep(delay);

	char* s = get_selection(clipboard_atom);
	maybe_group = active_state.group;
	to_buffer(&s);
}

// нажимаем комбинацию Shift+Insert (вставка)
void shift_insert() {
	clear_active_mods();
	int save = delay;
	//delay = 0;
	unikey_t shift_left = SYM_TO_KEY(XK_Shift_L);
	send_key(shift_left, 1);
	press_key(SYM_TO_KEY(XK_Insert));
	send_key(shift_left, 0);
	delay = save;
	recover_active_mods();
	maybe_group = active_state.group;
}

// устанавливаем в clipboard строку для передачи другим приложениям
void set_clipboard(char* s) {
	clipboard_s = s;
	clipboard_len = strlen(s);

	if(clipboard_len > selection_chunk_size)
		fprintf(stderr, "WARN: Размер данных для буфера обмена превышает размер куска для отправки: %u > %u. А протокол INCR не реализован\n", clipboard_len, selection_chunk_size);

	// делаем окно нашего приложения владельцем данных для обмена.
	// После этого будет возбуждено событие SelectionRequest
	XSetSelectionOwner(d, clipboard_atom, w, CurrentTime);
	XFlush(d);

	fprintf(stderr, "set_clipboard %s\n", s);
}

void event_next() {
	XEvent event;
	XNextEvent(d, &event);

	if(DEBUG) fprintf(stderr, "event_next %i, ", event.type);

	switch(event.type) {
		case SelectionNotify:
			// нет выделения
			if(event.xselection.property == None) {
				fprintf(stderr, "Нет выделения\n");
				break;
			}

			int format;	// формат строки
			unsigned long bytesafter, length;
			Atom target;

			// считываем
			XGetWindowProperty(event.xselection.display,
			    event.xselection.requestor, // window
			    event.xselection.property, 	// некое значение
				0L,
				1000000,	// максимальный размер, который мы готовы принять
			    False, (Atom) AnyPropertyType,
				&target, 	// тип возвращаемого значения: INCR - передача по частям
			    &format, &length, &bytesafter,
				(unsigned char**) &selection_retrive);
		break;
		case SelectionClear:
			fprintf(stderr, "Утрачено право собственности на буфер обмена.\n");
		break;
		case SelectionRequest:
			// подготовка формата в котором отдаём: utf8 или просто текст
			//Atom request_target = utf8_atom? utf8_atom: XA_STRING;

			// клиент хочет, чтобы ему сообщили в каком формате будут данные
			if (event.xselectionrequest.target == targets_atom) {
				Atom types[3] = { targets_atom, utf8_atom };

				XChangeProperty(d,
						event.xselectionrequest.requestor,
						event.xselectionrequest.property,
						XA_ATOM,
						32, PropModeReplace, (unsigned char *) types,
						(int) (sizeof(types) / sizeof(Atom))
				);

				if(DEBUG) fprintf(stderr, "отправляем targets\n");
			}
			else if(event.xselectionrequest.target != utf8_atom || event.xselectionrequest.property == None) {
				char* an = XGetAtomName(d, event.xselectionrequest.target);
				if(DEBUG) fprintf(stderr, "Запрошен clipboard в формате '%s'\n", an);
				if(an) XFree(an);
				event.xselectionrequest.property = None;
				break;
			}
			else if(clipboard_len > selection_chunk_size) {
				// TODO: реализовать протокол INCR
				if(DEBUG) fprintf(stderr, "Размер данных для буфера обмена превышает размер куска для отправки: %u vs %u. А протокол INCR не реализован\n", clipboard_len, selection_chunk_size);
			}
			else {
				// отправляем строку
				XChangeProperty(d,
						event.xselectionrequest.requestor,
						event.xselectionrequest.property,
						event.xselectionrequest.target, 8, PropModeReplace,
						(unsigned char *) clipboard_s, (int) clipboard_len);
				if(DEBUG) fprintf(stderr, "отправляем clipboard_s: `%s`\n", clipboard_s);
			}

			XEvent res;
			res.xselection.type = SelectionNotify;
			res.xselection.property = event.xselectionrequest.property;
			res.xselection.display = event.xselectionrequest.display;
			res.xselection.requestor = event.xselectionrequest.requestor;
			res.xselection.selection = event.xselectionrequest.selection;
			res.xselection.target = event.xselectionrequest.target;
			res.xselection.time = event.xselectionrequest.time;

			XSendEvent(d, event.xselectionrequest.requestor, 0, 0, &res);
			XFlush(d);
		break;
	}

}

void event_delay(double seconds) {
	// задержка, но коль пришли события иксов - сразу выходим
	int fd_display = ConnectionNumber(d);
	struct timeval tv = { (int) seconds, (int) ((seconds - (int) seconds) * 1000000) };
	fd_set readset;
	FD_ZERO(&readset);
	FD_SET(fd_display, &readset);
	select(fd_display + 1, &readset, NULL, NULL, &tv);

	// обрабатываем события окна, пока они есть
	while(XPending(d)) event_next();
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

	// ищем соответствие среди функций
	for(int i=0; i<keyfn_size; i++) {
		if(keyfn[i].key.mods == mods && keyfn[i].key.code == code) {
			//keyfn[i].fn(keyfn[i].arg);
			keyfn_active = i;
			return;
		}
	}

	if(ks == XK_BackSpace && mods == 0) {
		if(pos != 0) --pos;
	}
	else {
		// заносим в буфер
        add_to_buffer(key);
    }
}

int goto_home() {
	const char* home = getenv("HOME");
	if(!home) {fprintf(stderr, "WARN: нет getenv(HOME)\n");	return 0;}
	if(chdir(home)) {fprintf(stderr, "WARN: не могу перейти в каталог пользователя %s\n", home); return 0;}
	return 1;
}

void init_desktop(char* av0) {
	// FILE* f = fopen("/usr/share/application/erswitcher.desktop", "rb");
	// if(f) {fclose(f); return;}

	char* program = realpath(av0, NULL);
	if(!program) {fprintf(stderr, "WARN: нет пути к программе\n"); return;}

	#define INIT_DESKTOP_FREE	free(program)

	if(!goto_home()) { INIT_DESKTOP_FREE; return; }

	mkdir(".local", 0700);
	mkdir(".local/share", 0700);
	mkdir(".local/share/applications", 0700);

	const char* app = ".local/share/applications/erswitcher.desktop";

	FILE* f = fopen(app, "wb");
	if(!f) fprintf(stderr, "WARN: не могу создать %s/%s\n", getenv("HOME"), app);
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
	if(!f) {fprintf(stderr, "WARN: не могу создать %s/%s\n", getenv("HOME"), autostart); INIT_DESKTOP_FREE; return;}

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
	// TODO: килять другой экземпляр процесса, если он есть программно
	// char* s = NULL;
	// asprintf(&s, "ps aux | grep erswitcher | awk '{print $2}' | grep -v '^%i$' | xargs kill -9", getpid());
	// int rc = system(s);
	// if(rc) fprintf(stderr, "%s завершилась с кодом %i - %s\n", s, rc, strerror(rc));
}

void run_command(char* s) {
	if(fork() == 0) {
		fprintf(stderr, "Будет запущена команда: %s\n", s);
		int rc = system(s);
		fprintf(stderr, "Команда %s завершилась с кодом %i - %s\n", s, rc, strerror(rc));
		exit(rc);
	}
}

void insert_from_clipboard(char* s) {
	set_clipboard(s);
	event_delay(delay / 1000000 * 2);
	shift_insert();
}

void compose(char*) {
	if(pos == 0) return;
	
	wint_t cs = KEY_TO_INT(word[pos-1]);
	wint_t to = 0;
	for(int i=0; i<compose_map_size; i++) {
		if(compose_map_keys[i] == cs) {
			to = compose_map_values[i];
			break;
		}
	}
	
	if(to == 0) return;
	
	press_key(SYM_TO_KEY(XK_BackSpace));
	press_key(INT_TO_KEY(to));
}

void word_translate(char*) { print_translate_buffer(from_space(), 1); }
void text_translate(char*) { print_translate_buffer(0, 1); }
void selection_translate(char*) { copy_selection(); print_translate_buffer(0, 0); }

void word_invertcase(char*) { print_invertcase_buffer(from_space(), 1); }
void text_invertcase(char*) { print_invertcase_buffer(0, 1); }
void selection_invertcase(char*) { copy_selection(); print_invertcase_buffer(0, 0); }


void load_config(int) {
	if(DEBUG) fprintf(stderr, "Конфигурация применяется ...\n");

	clipboard_s = NULL;
	clipboard_len = 0;

	for(keyfn_t *k = keyfn, *n = keyfn + keyfn_size; k<n; k++) if(k->arg) free(k->arg);
	free(keyfn); keyfn = NULL; keyfn_max_size = keyfn_size = 0;

	if(!goto_home()) return;

	char* path;
	asprintf(&path, "%s/.config/erswitcher.conf", getenv("HOME"));

	FILE* f = fopen(path, "rb");
	if(!f) {
		f = fopen(path, "wb");
		if(!f) {
			fprintf(stderr, "WARN: не могу создать конфиг `%s`\n", path);
			return;
		}

		fprintf(f,
			"#####################################################################\n"
			"#              Конфигурацонный файл erswitcher-а                    #\n"
			"#                                                                   #\n"
			"# Автор: Ярослав О. Косьмина                                        #\n"
			"# Сайт:  https://github.com/darviarush/erswitcher                   #\n"
			"#                                                                   #\n"
			"# комбинация=:текст - будет введён этот текст с клавиатуры          #\n"
			"#   (допускаются символы которые есть в раскладках клавиатуры)      #\n"
			"# комбинация=^текст - будет введён этот текст через буфер обмена    #\n"
			"#   (допускаются любые символы)                                     #\n"
			"# комбинация=|текст - будет выполнена команда (выражение шелла)     #\n"
			"#   (например: Alt+Shift+Control+F1=opera ya.ru - откроет страницу  #\n"
			"#     ya.ru в опере, при нажатии на эту комбинацию клавишь)         #\n"
			"#                                                                   #\n"
			"# Комбинации клавишь указываются в английской раскладке             #\n"
			"#   и действуют во всех раскладках                                  #\n"
			"# Названия символов взяты из Иксов. Посмотреть их можно запустив    #\n"
			"#   ~/local/bin/erswitcher в консоли и введя комбинацию на          #\n"
			"#   клавиатуре                                                      #\n"
			"#####################################################################\n"
			"\n"
			"[functions]\n"
			"# Транслитерация последнего введённого слова\n"
			"Pause=word translate\n"
			"# Транслитерация последнего ввода\n"
			"Control+Pause=text translate\n"
			"# Транслитерация выделенного участка или буфера обмена\n"
			"Shift+Break=selected translate\n"
			"# Изменение регистра последнего введённого слова\n"
			"Alt+Shift+Break=word invertcase\n"
			"# Изменение регистра последнего ввода\n"
			"Alt+Control+Pause=text invertcase\n"
			"# Изменение регистра последнего введённого слова\n"
			"Alt+Pause=selected invertcase\n"
			"# Транслитерация последнего введённого символа в дополнительные символы\n"
			"KP_Begin=compose\n"
			"Scroll_Lock=compose\n"
			"\n"
			"[compose]\n"
			"# Замены - при нажатии на ScrollLock или меню (центральная клавиша на расширенной клавиатуре) заменяет последний введённый символ на соответствующий\n"
			"# Дополнительные символы кириллицы для украинского и белорусского алфавитов\n"
			"ы=і\n"
			"Ы=І\n"
			"ъ=ї\n"
			"Ъ=Ї\n"
			"щ=ў\n"
			"Щ=Ў\n"
			"э=є\n"
			"Э=Є\n"
			"г=ґ\n"
			"Г=Ґ\n"
			"ё=’\n"
			"Ё=₴\n"			
			"\n"
			"[sendkeys]\n"
			"# Шаблоны - вводятся с клавиатуры, их символы должны быть в одной из действующих раскладок\n"
			"Super+Pause=Готово.\n"
			"\n"
			"[snippets]\n"
			"# Сниппеты - вводятся через буфер обмена (clipboard), могут содержать любые символы\n"
			"Super+Shift+Break=\n"
			"\n"
			"[commands]\n"
			"# Команды операционной системы. Выполняются в оболочке шелла\n"
			"Alt+Control+Shift+Break=kate ~/.config/erswitcher.conf && killall -HUP erswitcher\n"
		);

		fclose(f);

		f = fopen(path, "rb");
		if(!f) {
			fprintf(stderr, "WARN: не могу открыть только что созданный конфиг `%s`\n", path);
			return;
		}
	}

	#define LINE_BUF_SIZE	(1024*1024)

	char buf[LINE_BUF_SIZE];
	int lineno = 0;
	int no_section = 0; // сигнализирует о том, что секция не распознана
	
	
	NEXT_LINE:
	while(fgets(buf, LINE_BUF_SIZE, f)) {
		lineno++;
		
		char* s = buf;
		while(isspace(*s)) s++;

		if(*s == '#' || *s == '\0') continue;

		if(*s == '[') { // это секция. Сверяемся со списком
			if(EQ(s, "[functions]"))
		}
		
		if(no_section) continue;

		char* v = strchr(s, '=');
		if(!v) { fprintf(stderr, "WARN: %s:%i: ошибка синтаксиса: нет `=`. Пропущено\n", path, lineno); continue; }
		*v = '\0'; v++;

		// определяем комбинацию клавиш
		unikey_t key = {0,0,0};
		int mods = 0;
		int key_set_flag = 0;

		char* x = strtok(s, "+");
		while (x != NULL) {
			if(key_set_flag) { fprintf(stderr, "WARN: %s:%i: ошибка синтаксиса: несколько клавишь-немодификаторов подряд (как %s в %s). Пропущено\n", path, lineno, x, s); goto NEXT_LINE; }

			if(EQ(x, "Shift")) mods |= ShiftMask;
			else if(EQ(x, "Control") || EQ(x, "Ctrl")) mods |= ControlMask;
			else if(EQ(x, "Alt") || EQ(x, "Option")) mods |= AltMask;
			else if(EQ(x, "Meta")) mods |= MetaMask;
			else if(EQ(x, "Super") || EQ(x, "Win") || EQ(x, "Command")) mods |= SuperMask;
			else if(EQ(x, "Hyper")) mods |= HyperMask;
			else {
				key = STR_TO_KEY(x);
				if(key.code == 0) { fprintf(stderr, "WARN: %s:%i: не распознан символ %s. Пропущено\n", path, lineno, x); goto NEXT_LINE; }

				key.mods = mods;
				key_set_flag = 1;
			}

			x = strtok(NULL, "+");
		};

		if(!key_set_flag) { fprintf(stderr, "WARN: %s:%i: ошибка синтаксиса: нет клавиши-немодификатора (в выражении %s). Пропущено\n", path, lineno, s); continue; }

		// удаляем пробелы в конце строки или только \n
		char* z = v;
		while(*z) z++;
		if(*v == '^' || *v == ':') { if(z[-1] == '\n') z--; } else { while(isspace(z[-1])) z--; }
		*z = '\0';

		// определяем функцию
		void (*fn)(char*);
		char* arg = NULL;

		if(*v == '^') {	fn = insert_from_clipboard; arg = strdup(v+1); }
		else if(*v == ':') { fn = sendkeys; arg = strdup(v+1); }
		else if(*v == '|') { fn = run_command; arg = strdup(v+1); }

		else if(EQ(v, "word translate")) fn = word_translate;
		else if(EQ(v, "text translate")) fn = text_translate;
		else if(EQ(v, "selected translate")) fn = selection_translate;

		else if(EQ(v, "word invertcase")) fn = word_invertcase;
		else if(EQ(v, "text invertcase")) fn = text_invertcase;
		else if(EQ(v, "selected invertcase")) fn = selection_invertcase;
		
		else if(EQ(v, "compose")) fn = compose;

		else { fprintf(stderr, "WARN: %s:%i: нет функции %s. Пропущено\n", path, lineno, v); continue; }

		// выделяем память под массив, если нужно
		if(keyfn_size == keyfn_max_size) {
			keyfn_max_size += KEYFN_NEXT_ALLOC;
			keyfn = realloc(keyfn, keyfn_max_size * sizeof(keyfn_t));
		}

		keyfn[keyfn_size++] = (keyfn_t) {key: key, fn: fn, arg: arg};
		if(DEBUG) {
			keyfn_t *k = keyfn+keyfn_size-1;
			printf("------------------------------\n");
			KeySym ks = KEY_TO_SYM(k->key);
			print_sym(k->key.mods, ks);
			printf(" = %s\n", k->arg);
			fflush(stdout);
		}
	}

	free(path);
	fclose(f);
	if(DEBUG) fprintf(stderr, "Конфигурация применена!\n");
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
	memset(timers, 0, sizeof timers);	// инициализируем таймеры

	// Начальные установки
	current_win = get_current_window();
	pos = 0;
	init_keyboard();

	// конфигурация должна загружаться после инициализации клавиатуры
	load_config(0);

	signal(SIGHUP, load_config);
	signal(SIGCHLD, SIG_IGN);

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

		// опрашиваем клавиатуру
        XQueryKeymap(d, keys);
		keys_pressed = -1;
      	for(int i=0; i<KEYBOARD_SIZE; i++) {

			if(keys_pressed < 0) {
				keys_pressed = 0;
				for(int i=0; i<KEYBOARD_SIZE; i++) if(BIT(keys, i) != 0) keys_pressed++;
			}

      		if(BIT(keys, i) != BIT(saved, i)) {
      			if(BIT(keys, i) != 0) { // клавиша нажата
      				change_key(i);
      			} else {	// клавиша отжата
					if(keys_pressed == 0 && keyfn_active != -1) {
						keyfn[keyfn_active].fn(keyfn[keyfn_active].arg);
						keyfn_active = -1;
					}
      			}
      		}

      	}

      	char* char_ptr=saved;
      	saved=keys;
      	keys=char_ptr;

		// крошим зомбиков
		int status;
		waitpid(-1, &status, WNOHANG);

		// сработают таймеры, чьё время пришло
		timers_apply();

		event_delay(delay / 1000000);
   	}

   	return 0;
}
