/**************************************************************
 * Приложение: erswitcher - переключатель клавиатурного ввода *
 * Автор: Ярослав О. Косьмина                                 *
 * Лицензия: GPLv3                                            *
 * Местонахождение: https://github.com/darviarush/erswitcher  *
 **************************************************************/

//#define _GNU_SOURCE

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

// TODO: задержку указывать в конфиге
// дефолтная задержка
#define DELAY 		    10000
// .0 - обязателен, иначе деление будет целочисленным
#define USEC_TO_DBL(U)	(U / 1000000.0)

int delay = DELAY;          	// задержка между программными нажатиями клавишь в микросекундах
int loop_delay = DELAY;			// задержка между опросами клавиатуры
pid_t config_window_pid = 0;	// pid конфигуратора


typedef struct {
	double time; 			// время в секундах, когда таймер должен сработать
	void(*fn)();
} mytimer_t;

#define MAX_TIMERS			256
mytimer_t timers[MAX_TIMERS];
int timers_size = 0;

//@category Иксы

Display *d;					// текущий дисплей
Window w;					// окно этого приложения
Window current_win;			// окно устанавливается при вводе с клавиатуры
int selection_chunk_size; 	// максимально возможный размер данных для передачи через буфер обмена
char* clipboard_s = NULL;		// буфер обмена для отправки данных
char* clipboard_pos = NULL;		// позиция в буфере обмена при отправке данных частями (INCR)
int clipboard_len = 0;			// длина буфера обмена для отправки данных
Atom clipboard_target;			// тип данных в буфере обмена для отправки данных

char* selection_retrive = NULL; 	// буфер обмена для получения данных
int selection_retrive_length = 0;	// длина данных в буфере
Atom selection_retrive_type;		// тип данных в буфере: utf8_atom, например

void (*on_get_selection)(char*,int,Atom) = NULL;	// обработчик завершения считывания буфера
void (*on_copy_selection)(int,int) = NULL;			// обработчик завершения копирования буфера обмена в буфер ввода

int incr_is = False;	// передача буфера обмена частями
Atom incr_propid;		// что передаём (например, utf8_atom)
Window incr_window;		// какое окно нам передаёт данные

Atom sel_data_atom;		// указывается в запросе на выборку
Atom utf8_atom;			// запросы на чтение/запись буфера обмена - данные преобразовать в utf8
Atom clipboard_atom;	// идентификатор буфера обмена clipboard
Atom targets_atom;		// запрос на формат данных
Atom incr_atom;			// передача буфера обмена частями

// для шагов ввода через буфер
char* insert_from_clipboard_data;		// что нужно ввести (compose)
char* insert_from_clipboard_save_data;	// что было в буфере обмена
int insert_from_clipboard_save_len;
Atom insert_from_clipboard_save_target;

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

#define COMPOSE_KEY_MAX		20
#define COMPOSE_VAL_MAX		COMPOSE_KEY_MAX * 6
typedef struct {
	wint_t word[COMPOSE_KEY_MAX];
	int pos;
	char* to;
} compose_t;

int compose_map_size = 0;
int compose_map_max_size = 0;
compose_t* compose_map = NULL;

//@category Раскладки

int groups = 0;			// Количество раскладок
int group_ru = -1;		// Номер русской раскладки или -1
int group_en = -1;		// Номер английской раскладки или -1

// с какой группы начинать поиск сканкода по символу
// после поиска maybe_group переключается в группу найденного символа
int maybe_group = -1;

void set_group(int group);
unikey_t keyboard_state(int code);
void clear_active_mods();
void recover_active_mods();

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
	// groups < XkbNumKbdGroups &&
	for(groups = 0; gs[groups] != 0;) {
		char* kb_name = XGetAtomName(d, gs[groups]);
		if(strncmp(kb_name, Russian, strlen(Russian)) == 0) group_ru = groups;
		if(strncmp(kb_name, English, strlen(English)) == 0) group_en = groups;
		XFree(kb_name);
		groups++;
	}

	// mkdir(".cache", 0744);
	// char* path = ".cache/erswitcher.keyboard.csv";
	// FILE* f = fopen(path, "wb");
	// if(!f) {
		// fprintf(stderr, "Не открыть для записи %s\n", path);
		// goto EXIT_INIT_KB;
	// }

	// malloc();

	// unikey_t state = keyboard_state(0);
	// clear_active_mods();

	// fprintf(f, "\n");
	// fprintf(f, "%i;Раскладка;%s\n", group, kb_name);
	// fprintf(f, "\n");
	// fprintf(f, "Сканкод;Символ;Символ с shift\n");
	// for(int code = 0; code < KEYBOARD_SIZE; ++code) {
		// fprintf(f, "%i;", code);
		// for(int group = 0; group<groups; group++) {
			// set_group(group);
			// char* kb_name = XGetAtomName(d, gs[group]);
			// fprintf(f, "%i;%s;", group, kb_name);
			// XFree(kb_name);
			
		// }
		
			// //unikey_t key = {code: code, mods: shift? ShiftMask: 0, group: group};
		// KeySym ks = XKeycodeToKeysym(d, code);
		// KeySym ks2 = XKeycodeToKeysym(d, code);
		// fprintf(f, "%i;%s;%s", code, SYM_TO_STR(ks), SYM_TO_STR(ks2));
	// }

	// recover_active_mods();
	// fclose(f);

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
	
	//EXIT_INIT_KB:
	XkbFreeNames(kb, XkbGroupNamesMask, 0);
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

//@category Задержки

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
	timers[i].time = curtime.tv_sec + curtime.tv_usec / 1000000.0 + interval;

	if(timers_size <= i) timers_size = i+1;

	return i;
}

void timers_apply() {
	struct timeval curtime;
	gettimeofday(&curtime, 0);
	double now = curtime.tv_sec + curtime.tv_usec / 1000000.0;

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

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

void send_systray_message(Display* d, long message, long data1, long data2, long data3) {
    XEvent ev;

    Atom selection_atom = XInternAtom(d, "_NET_SYSTEM_TRAY_S0", False);
    Window tray = XGetSelectionOwner(d, selection_atom);

    if(tray != None) XSelectInput(d, tray, StructureNotifyMask);

    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = tray;
    ev.xclient.message_type = XInternAtom(d, "_NET_SYSTEM_TRAY_OPCODE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = message;
    ev.xclient.data.l[2] = data1; // <--- your window is only here
    ev.xclient.data.l[3] = data2;
    ev.xclient.data.l[4] = data3;

    XSendEvent(d, tray, False, NoEventMask, &ev);
    XSync(d, False);
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
	if(utf8_atom == None) utf8_atom = XA_STRING; // для поддержки на старых системах
	clipboard_atom = XInternAtom(d, "CLIPBOARD", False);
	targets_atom = XInternAtom(d, "TARGETS", False);
	incr_atom = XInternAtom(d, "INCR", False);

	// получаем максимально возможный размер данных для передачи через буфер обмена
	selection_chunk_size = XExtendedMaxRequestSize(d) / 4;
	if(!selection_chunk_size) selection_chunk_size = XMaxRequestSize(d) / 4;
	printf("selection_chunk_size = %i\n", selection_chunk_size); fflush(stdout);

	// Создаём окно
	w = XCreateSimpleWindow(d, XDefaultRootWindow(d), -10, -10, 1, 1, 0, 0, 0x00FF0000);
	// подписываемся на события окна
	XSelectInput(d, w, PropertyChangeMask | ButtonPressMask);
	
	// стыкуем окно с системным лотком
	//send_systray_message(d, SYSTEM_TRAY_REQUEST_DOCK, w, 0, 0);
}

// возвращает текущее окно
Window get_current_window() {
	Window w = 0;
  	int revert_to = 0;
	XGetInputFocus(d, &w, &revert_to);
	return w;
}


//@category клавиатура и мышь

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

	send_mods(key.mods, is_press);
	set_group(key.group);
	printf("send_key: %s %s\n", KEY_TO_STR(key), is_press? "PRESS": "RELEASE");
	press(key.code, is_press);

    XFlush(d);
    if(delay) usleep(delay);
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

// Очищает активные модификаторы клавиатуры
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
	
	// снимаем Нам-лок (он не входит в модификаторы)
	if(active_state.mods & NumMask) {
		press_key(SYM_TO_KEY(XK_Num_Lock));
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
	
	// восстанавливаем Num-лок (он не входит в модификаторы)
	if(active_state.mods & NumMask && !(state.mods & NumMask)) {
		press_key(SYM_TO_KEY(XK_Num_Lock));
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

#define FOR_UTF8(S)			\
	mbstate_t mbs_UTF8 = {0}; 	\
	for(size_t charlen_UTF8, i_UTF8 = 0;	\
        (charlen_UTF8 = mbrlen(S+i_UTF8, MB_CUR_MAX, &mbs_UTF8)) != 0	\
        && charlen_UTF8 > 0;			\
        i_UTF8 += charlen_UTF8			\
    )
	
#define STEP_UTF8(S, W)		\
		wchar_t ws_UTF8[4];		\
        int res_UTF8 = mbstowcs(ws_UTF8, S+i_UTF8, 1);		\
        if(res_UTF8 != 1) break; \
		wint_t W = ws_UTF8[0];

// TODO: зажатие управляющих клавишь {\Control+Alt}abc{/Control} и {Ctrl+Alt+a}
void sendkeys(char* s) { // печатает с клавиатуры строку в utf8
	clear_active_mods();

	FOR_UTF8(s) {
        STEP_UTF8(s, ws);

		press_key(INT_TO_KEY(ws));
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

char* get_event_type(int n) {
	if(n < 2 || n > LASTEvent) return NULL;
	
	char* names[] = {NULL, NULL,
		"KeyPress",
		"KeyRelease",
		"ButtonPress",
		"ButtonRelease",
		"MotionNotify",
		"EnterNotify",
		"LeaveNotify",
		"FocusIn",
		"FocusOut",
		"KeymapNotify",
		"Expose",
		"GraphicsExpose",
		"NoExpose",
		"VisibilityNotify",
		"CreateNotify",
		"DestroyNotify",
		"UnmapNotify",
		"MapNotify",
		"MapRequest",
		"ReparentNotify",
		"ConfigureNotify",
		"ConfigureRequest",
		"GravityNotify",
		"ResizeRequest",
		"CirculateNotify",
		"CirculateRequest",
		"PropertyNotify",
		"SelectionClear",
		"SelectionRequest",
		"SelectionNotify",
		"ColormapNotify",
		"ClientMessage",
		"MappingNotify",
		"GenericEvent",
		"LASTEvent",
	};

	return names[n];
}

// заказываем получить содержимое буфера обмена с типом response_format_atom
// после получения произойдёт вызов обработчика on_get_selection
void get_selection(Atom number_buf, Atom response_format_atom, void (*fn)(char*,int,Atom)) {

	Window owner = XGetSelectionOwner(d, number_buf);
    if (owner == None) {
		char* name_buf = XGetAtomName(d, number_buf);
        fprintf(stderr, "Буфер %s не поддерживается владельцем\n", name_buf);
		if(name_buf) XFree(name_buf);
		
		fn(strdup(""), 0, utf8_atom);
        return;
    }
    if(DEBUG) { printf("get_selection: owner = 0x%lX %s\n", owner, w == owner? "одинаков с w": "разный с w");fflush(stdout); }

	// если владелец буфера мы же, то и отдаём что есть
	if(w == owner) {
		if(DEBUG) { printf("get_selection: буфер - мы же\n");fflush(stdout); }
		on_get_selection = NULL;
		
		if(clipboard_s) {
			if(DEBUG) { printf("get_selection: Отдаю clipboard_s: %s\n", clipboard_s); fflush(stdout); }
			
			char* s = malloc(clipboard_len);
			memcpy(clipboard_s, s, clipboard_len);
			fn(s, clipboard_len, clipboard_target);
		}
		else {
			if(DEBUG) { printf("get_selection: Отдаю пустую строку\n"); fflush(stdout); }
			fn(strdup(""), 0, utf8_atom);
		}
		return;
	}

	// требование перевода в utf8:
	XConvertSelection(d, number_buf,
		response_format_atom, //utf8_atom,
		sel_data_atom,
		w,
	    CurrentTime
	);
	XSync(d, False);

	on_get_selection = fn;
	
	char* fmt = XGetAtomName(d, response_format_atom);
	fprintf(stderr, "get_selection %s!\n", fmt);
	XFree(fmt);
}

// переписываем строку в буфер ввода
void to_buffer(char* s) {
	pos = 0;

	// utf8 переводим в символы юникода, затем в символы x11, после - в сканкоды
	FOR_UTF8(s) {
        STEP_UTF8(s, ws);
		if(pos >= BUF_SIZE) break;
		word[pos++] = INT_TO_KEY(ws);
    }
}

// нажимаем комбинацию Control+Insert (выделение добавить в буфер обмена (clipboard))
void control_insert() {
	clear_active_mods();
	unikey_t control_left = SYM_TO_KEY(XK_Control_L);
	send_key(control_left, 1);
	press_key(SYM_TO_KEY(XK_Insert));
	send_key(control_left, 0);
	recover_active_mods();
	maybe_group = active_state.group;
}

// копируем выделенное и заменяем им буфер ввода
void copy_selection_next(char* s, int, Atom) {
	to_buffer(s);
	free(s);
	on_copy_selection(0, 0);
}
void copy_selection(void (*fn)(int, int)) {
	on_copy_selection = fn;
	control_insert();
	get_selection(clipboard_atom, utf8_atom, copy_selection_next);
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

// устанавливаем в clipboard данные для передачи другим приложениям
void set_clipboard(char* s, int len, Atom target) {	
	clipboard_s = s;
	clipboard_len = len;
	clipboard_target = target;

	if(clipboard_len > selection_chunk_size)
		fprintf(stderr, "WARN: Размер данных для буфера обмена превышает размер куска для отправки: %u > %u. Передача будет осуществляться по протоколу INCR\n", clipboard_len, selection_chunk_size);

	// делаем окно нашего приложения владельцем данных для обмена.
	// После этого будет возбуждено событие SelectionRequest
	XSetSelectionOwner(d, clipboard_atom, w, CurrentTime);
	XFlush(d);

	Window owner = XGetSelectionOwner(d, clipboard_atom);

	if(DEBUG) fprintf(stderr, "set_clipboard `%s`, my window %s owner\n", s, w == owner? "is": "no");
}

// очистить буфер раздачи данных клипборда
void clipboard_free() {
	if(clipboard_s) {
		free(clipboard_s);
		clipboard_s = NULL;
	}
}

void event_next() {
	XEvent event;
	XNextEvent(d, &event);

	if(DEBUG) fprintf(stderr, "[%i %s], ", event.type, get_event_type(event.type));

	switch(event.type) {
		case SelectionNotify: // получить данные из буфера обмена
			if(event.xselection.selection != clipboard_atom) {
				fprintf(stderr, "SelectionNotify: Какой-то другой буфер запрошен\n");
				break;
			}
			// нет выделения
			if(event.xselection.property == None) {
				fprintf(stderr, "SelectionNotify: Нет выделения\n");
				
				if(on_get_selection) {
					void(*fn)(char*,int,Atom) = on_get_selection;
					on_get_selection = NULL;
					fn(NULL, 0, None);
				}
				if(selection_retrive) {
					free(selection_retrive);
					selection_retrive = NULL;
				}
				break;
			}

			if(incr_is) {
				if(incr_window != event.xselection.requestor) {
					fprintf(stderr, "SelectionNotify: Не совпал incr_window\n");
					break;
				}
				if(incr_propid != event.xselection.property) {
					fprintf(stderr, "SelectionNotify: Не совпал incr_propid\n");
					break;
				}
				if(event.xproperty.state != PropertyNewValue) {
					fprintf(stderr, "SelectionNotify: Не совпал PropertyNewValue\n");
					break;
				}
			} else {
				incr_propid = event.xselection.property;
				incr_window = event.xselection.requestor;
			}
			
			int format;	// формат строки
			unsigned long bytesafter, length;
			unsigned char* result = NULL;

			// считываем
			XGetWindowProperty(event.xselection.display,
			    incr_window, 	// window
			    incr_propid, 	// некое значение
				0L,
				1024*1024*64,	// максимальный размер, который мы готовы принять
			    incr_is, (Atom) AnyPropertyType,
				&selection_retrive_type, 	// тип возвращаемого значения: INCR - передача по частям
			    &format, &length, &bytesafter,
				&result);
				
			char* selection_retrive_type_name = XGetAtomName(d, selection_retrive_type);
			printf("read selection: s=`%s` len=%lu after=%lu selection_retrive_type=%s format=%i\n", result, length, bytesafter, selection_retrive_type_name, format); fflush(stdout);
			if(selection_retrive_type_name) XFree(selection_retrive_type_name);

			// завершаем
			void send_on_get_selection() {
				fprintf(stderr, "send_on_get_selection\n");
				if(on_get_selection) {
					void(*fn)(char*,int,Atom) = on_get_selection;
					on_get_selection = NULL;
					fn(selection_retrive, selection_retrive_length, selection_retrive_type);
				} else {
					fprintf(stderr, "Нет on_get_selection!\n");
					free(selection_retrive);
				}
				selection_retrive = NULL;
			}

			if(incr_is) {	// мы в цикле INCR
				memcpy(selection_retrive + selection_retrive_length, result, length);
				selection_retrive[selection_retrive_length += length] = '\0';
				
				if(bytesafter == 0) {
					incr_is = False;
					
					send_on_get_selection();
				}
			} else {		// стандартно
			
				if(selection_retrive) {
					fprintf(stderr, "ALERT: selection_retrive не удалена. Такого быть не должно!\n");
					free(selection_retrive);
				}
				
				selection_retrive = (char*) malloc(length + bytesafter + 1);
				memcpy(selection_retrive, result, length);
				selection_retrive[selection_retrive_length = length] = '\0';
				
				if(selection_retrive_type == incr_atom) incr_is = True;
				else {
					send_on_get_selection();
				}
			}
			
			if(result) XFree(result);
		break;
		case SelectionClear:
			fprintf(stderr, "SelectionClear: Утрачено право собственности на буфер обмена.\n");
		break;
		case SelectionRequest: // запрос данных из буфера обмена
			
			Window win = event.xselectionrequest.requestor;
			Atom pty = event.xselectionrequest.property;
			Atom target_sel = event.xselectionrequest.target;

			// клиент хочет, чтобы ему сообщили в каком формате будут данные
			if (target_sel == targets_atom) {
				Atom types[] = { clipboard_target, targets_atom };

				XChangeProperty(d,
						win,
						pty,
						XA_ATOM,
						32, PropModeReplace, (unsigned char *) types,
						sizeof(types) / sizeof(Atom)
				);

				if(DEBUG) fprintf(stderr, "отправляем targets\n");
			}
			else if(target_sel != clipboard_target || pty == None) {
				char* an = XGetAtomName(d, target_sel);
				if(DEBUG) fprintf(stderr, "Запрошен clipboard в формате '%s'\n", an);
				if(an) XFree(an);
				pty = None;
				break;
			}
			else if(clipboard_len > selection_chunk_size) {
				// придётся отправлять по протоколу INCR, так как данные слишком велики
				XChangeProperty(d, win, pty, incr_atom, 32, PropModeReplace, 0, 0);
				XSelectInput(d, win, PropertyChangeMask);
				
				clipboard_pos = clipboard_s;
			}
			else {
				// отправляем строку
				XChangeProperty(d, win,	pty,
						target_sel, 8, PropModeReplace,
						(unsigned char *) clipboard_s, (int) clipboard_len);
				if(DEBUG) { printf("отправляем clipboard_s: `%s` len=%i\n", clipboard_s, clipboard_len); fflush(stdout); }
			}

			XEvent res;
			res.xselection.type = SelectionNotify;
			res.xselection.display = event.xselectionrequest.display;
			res.xselection.requestor = win;
			res.xselection.property = pty;
			res.xselection.selection = event.xselectionrequest.selection;
			res.xselection.target = target_sel;
			res.xselection.time = event.xselectionrequest.time;

			XSendEvent(d, event.xselectionrequest.requestor, 0, 0, &res);
			XFlush(d);
		break;
		case PropertyNotify:	// протокол INCR: отправка по частям
			if(event.xproperty.state == PropertyDelete) {
				if(!clipboard_s) break;
				if(clipboard_len <= selection_chunk_size) break;
				
				if(DEBUG) {printf("state == PropertyDelete\n"); fflush(stdout);}
				Window win = event.xselectionrequest.requestor;
				Atom pty = event.xselectionrequest.property;
				Atom target_notify = event.xselectionrequest.target;
				
				//if(win != ) break;
				
				int len = clipboard_pos+selection_chunk_size < clipboard_s+clipboard_len?
					selection_chunk_size:
					clipboard_s+clipboard_len-clipboard_pos;
				
				XChangeProperty(d, win, pty, target_notify, 8, PropModeReplace, 
					len==0? NULL: (unsigned char*) clipboard_pos, 
					len);
				XFlush(d);
				
				clipboard_pos += len;
				//if(clipboard_pos == clipboard_s+clipboard_len) // конец
			}
		break;
		case ClientMessage:	// окно пристыковано к системному лотку
			XWindowAttributes attr;
			XGetWindowAttributes(d, w, &attr);
			
			printf("win = %i x %i\n", attr.width, attr.height);
		break;
		case ButtonPress: // нажали на иконке
			// // если окно tcl запущено - закрываем его
			// if(config_window_pid) kill(-9, config_window_pid);
			// // запускаем новое
			// config_window_pid = fork();
			// if(config_window_pid < 0) {
				// perror("ERROR: fork не запустился");
			// } else if(config_window_pid == 0) {
				// char* path = "erswitcher-config";
				// char* tclsh = "/usr/bin/tclsh";
				// execl(tclsh, path);
				// fprintf(stderr, "ERROR: в конфигураторе %s\n", path);
				// exit(1);
			// }
		break;
	}

}

void event_delay(double seconds) {
	// задержка, но коль пришли события иксов - сразу выходим
	struct timeval tv = { (int) seconds, (int) ((seconds - (int) seconds) * 1000000) };
	
	fd_set readset;
	FD_ZERO(&readset);
	int fd_display = ConnectionNumber(d);
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
	setenv("PWD", home, 1);	
	return 1;
}

void init_desktop(char* program) {

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
	if(!f) {fprintf(stderr, "WARN: не могу создать %s/%s\n", getenv("HOME"), autostart); return;}

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

// начинаем раздавать сохранённые и тщательно отобранные у другого приложения данные
void insert_from_clipboard_step3() {
	printf("--> insert_from_clipboard_step3: reset buffer len=%i\n", insert_from_clipboard_save_len); fflush(stdout);
	set_clipboard(insert_from_clipboard_save_data,
                  insert_from_clipboard_save_len,
                  insert_from_clipboard_save_target);
}
// получаем данные из буфера обмена, сохраняем и вставляем из буфера обмена
void insert_from_clipboard_step2(char* s, int len, Atom target) {
	fprintf(stderr, "--> insert_from_clipboard_step2: save buffer %s\n", s);
	insert_from_clipboard_save_data = s;
	insert_from_clipboard_save_len = len;
	insert_from_clipboard_save_target = target;
	
	set_clipboard(insert_from_clipboard_data, 
                  strlen(insert_from_clipboard_data),
                  utf8_atom);	// начинаем раздавать

	insert_from_clipboard_data = NULL;

	after(0.10, shift_insert);
	after(0.31, insert_from_clipboard_step3);	// раздаём из буфера сохранённые данные
}
// получаем какие типы данных есть в буфере обмена
void insert_from_clipboard_step1(char* s, int len, Atom target) {
	printf("--> insert_from_clipboard_step1: targets len=%i\n", len); fflush(stdout);
	// в буфере ничего нет
	if(target == None) { insert_from_clipboard_step2(strdup(""), 0, utf8_atom); return; }
	
	Atom* targets = (Atom*) s;
	int size = len;// / sizeof(Atom);
	
	if(DEBUG) {
		printf("\nTargets: len=%i sizeof(Atom)=%li size=%i\n", len, sizeof(Atom), size); 
		for(int i=0; i<size; i++) {
			char* target_name = XGetAtomName(d, targets[i]);
			printf("%i) %lu - %s\n", i, targets[i], target_name); 
			XFree(target_name);
		}
		printf("\n");
		fflush(stdout);
	}

	Atom mytarget = None;
	for(int i=0; i<size; i++) {
		if(targets[i] != targets_atom) { mytarget = targets[i]; break; };
	}
	free(s);
	if(mytarget != None) get_selection(clipboard_atom, mytarget, insert_from_clipboard_step2);
	else insert_from_clipboard_step2(strdup(""), 0, utf8_atom);
}
// запоминаем содержимое буфера обмена, подменяем буфер собой и вставляем из него композэ
void insert_from_clipboard(char* s) {
	
	printf("--> insert_from_clipboard %s\n", s); fflush(stdout);
	
	insert_from_clipboard_data = s;
	
	Window owner = XGetSelectionOwner(d, clipboard_atom);	// владелец буфера
	if(owner == None) {	// нет владельца - значит и раздавать ничего не надо
		clipboard_free();
		insert_from_clipboard_step2(strdup(""), 0, utf8_atom);
	} else if(owner == w) {	// владелец - мы и уже раздаём. Сохраняем, что у нас есть
		insert_from_clipboard_step2(clipboard_s, clipboard_len, clipboard_target);
	}
	else {
		clipboard_free();
		get_selection(clipboard_atom, targets_atom, insert_from_clipboard_step1);
	}
}

void compose(char*) {
	if(pos == 0) return;
	
	char* to = NULL;
	int remove_sym = 0;
	for(int i=0; i<compose_map_size; i++) {
		int k, n;
		for(k = pos-1, n = compose_map[i].pos - 1; k >= 0 && n >= 0; k--, n--) {
			if(compose_map[i].word[n] != KEY_TO_INT(word[k])) break;
		}
		
		if(n == -1) {	// дошли до конца
			to = compose_map[i].to;
			remove_sym = compose_map[i].pos;
			break;
		}
		
	}
	
	if(to == NULL) { fprintf(stderr, "compose: Ничего не найдено!\n"); return; }
	
	if(DEBUG) { printf("compose -> %s\n", to); fflush(stdout); }

	// удаляем мнемонику введённую ранее
	unikey_t key = keyboard_state(0);
	press_key_multi(SYM_TO_KEY(XK_BackSpace), remove_sym);
	set_group(key.group);
	// запоминаем содержимое буфера обмена, подменяем буфер собой и вставляем из него композэ
	insert_from_clipboard(to);
}

void word_translate(char*) { print_translate_buffer(from_space(), 1); }
void text_translate(char*) { print_translate_buffer(0, 1); }
void selection_translate(char*) { copy_selection(print_translate_buffer); }

void word_invertcase(char*) { print_invertcase_buffer(from_space(), 1); }
void text_invertcase(char*) { print_invertcase_buffer(0, 1); }
void selection_invertcase(char*) { copy_selection(print_invertcase_buffer); }


void load_config(int) {
	if(DEBUG) fprintf(stderr, "Конфигурация применяется ...\n");

	clipboard_s = NULL;
	clipboard_len = 0;
	clipboard_target = utf8_atom;

	for(keyfn_t *k = keyfn, *n = keyfn + keyfn_size; k<n; k++) if(k->arg) free(k->arg);
	free(keyfn); keyfn = NULL; keyfn_max_size = keyfn_size = 0;
	
	for(int k = 0; k<compose_map_size; k++) free(compose_map[k].to);
	free(compose_map); compose_map = NULL; compose_map_max_size = compose_map_size = 0;

	char* path = ".config/erswitcher.conf";

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
			"# Замены (мнемоники) - при нажатии на ScrollLock или меню (центральная клавиша на расширенной клавиатуре) заменяет последний введённый символ на соответствующий\n"
			"\n"
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
			"# Кавычки\n"
			"//=«\n"
			"\\\\=»\n"
			",,=„\n"
			"..=”\n"
			"``=“\n"
			",=‘\n"
			".=’\n"
			"# Тире - длинное, среднее и цифровое\n"
			"-=—\n"
			"-,=–\n"
			"-.=‒\n"
			"# Пунктуация\n"
			"...=…\n"
			"p=§\n"
			"P=§§\n"
			":-=÷\n"
			"-//-=―〃―\n"
			"***=⁂\n"
			"x=⌘\n"
			"?\?=؟\n"
			"?=¿\n"
			"!=¡\n"
			"# Неразрывный пробел (\u00A0, &nbsp;)\n"
			" = \n"
			"# Утопающий и кричащий о помощи смайлик\n"
			"хелп=‿︵‿ヽ(°□° )ノ︵‿︵\n"
			"# Лежащий на боку и наблюдающий за чем-то смайлик\n"
			"вау=∠( ᐛ 」∠)＿\n"
			"# Ветер унёс зонтик бедняги во время дождя\n"
			"нет=｀、ヽ(ノ＞＜)ノ ｀、ヽ｀☂ヽ\n"
			"# Медвед\n"
			"миш=ʕ ᵔᴥᵔ ʔ\n"
			"# Пожималкин\n"
			"хз=¯\\_(ツ)_/¯\n"
			"# Смайлы\n"
			")=ヅ\n"
			";)=ゾ\n"
			":)=ジ\n"
			"# Математические символы\n"
			"+-=±\n"
			"++=∑\n"
			"<<=≤\n"
			">>=≥\n"
			"2=½\n"
			"3=⅓\n"
			"4=¼\n"
			"5=⅕\n"
			"6=⅙\n"
			"7=⅐\n"
			"# Cтрелочки\n"
			"->=→\n"
			"-->=⟶\n"
			"<-=←\n"
			"<--=⟵\n"
			"t=// TODO: \n"
			"T=# TODO: \n"
			"\n"
			"[sendkeys]\n"
			"# Шаблоны - вводятся с клавиатуры, их символы должны быть в одной из действующих раскладок\n"
			"Super+Pause=Готово.\n"
			"\n"
			"[snippets]\n"
			"# Сниппеты - вводятся через буфер обмена (clipboard), могут содержать любые символы\n"
			"Super+Shift+Break=Готово.\n"
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
	
	#define SEC_FUNCTIONS 	((void (*)(char*)) -1)
	void (*fn)(char*) = NULL;	
	
	NEXT_LINE:
	while(fgets(buf, LINE_BUF_SIZE, f)) {
		lineno++;
		
		char* s = buf;
		while(isspace(*s)) s++;

		if(*s == '#' || *s == '\0') continue;

		// удаляем пробелы в конце строки или только \n
		char* z = s;
		while(*z) z++;
		if(*s!='[' && fn != SEC_FUNCTIONS) { if(z[-1] == '\n') z--; } else { while(isspace(z[-1])) z--; }
		*z = '\0';


		if(*s == '[') { // это секция. Сверяемся со списком
			if(EQ(s, "[functions]")) fn = SEC_FUNCTIONS;
			else if(EQ(s, "[compose]")) fn = compose;
			else if(EQ(s, "[sendkeys]")) fn = sendkeys;
			else if(EQ(s, "[snippets]")) fn = insert_from_clipboard;
			else if(EQ(s, "[commands]")) fn = run_command;
			else {
				fprintf(stderr, "WARN: %s:%i: не распознана секция %s. Пропущена\n", path, lineno, s);
				fn = NULL;
			}
			continue;
		}
		
		if(fn == NULL) continue;

		// TODO: \= и другие символы \n, \x{12}
		// TODO: key=value на несколько строк
		// TODO: возврат курсора на указанную позицию - необходимо для сниппетов
		// TODO: выделение в нутрь сниппетов
		
		char* v = strchr(s, '=');
		if(!v) { fprintf(stderr, "WARN: %s:%i: ошибка синтаксиса: нет `=`. Пропущено\n", path, lineno); continue; }
		*v = '\0'; v++;

		// это мнемоника
		if(fn == compose) {
			// выделяем память под массив, если нужно
			if(compose_map_size >= compose_map_max_size) {
				compose_map_max_size += KEYFN_NEXT_ALLOC;
				compose_map = realloc(compose_map, compose_map_max_size * sizeof(compose_t));
			}
			
			wint_t* w = compose_map[compose_map_size].word;
			int i = 0;
			
			FOR_UTF8(s) {
				STEP_UTF8(s, ws);
				
				if(i >= COMPOSE_KEY_MAX) {fprintf(stderr, "WARN: %s:%i: строка слишком длинная \"%s\" > %i. Пропущенo\n", path, lineno, s, COMPOSE_KEY_MAX); goto NEXT_LINE;}
				
				w[i++] = ws;
			}
			
			compose_map[compose_map_size].pos = i;
			compose_map[compose_map_size++].to = strdup(v);
			continue;
		}

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

		// определяем функцию
		char* arg = NULL;
		void (*fn1)(char*);

		if(fn != SEC_FUNCTIONS) { arg = strdup(v); fn1 = fn; }

		else if(EQ(v, "word translate")) fn1 = word_translate;
		else if(EQ(v, "text translate")) fn1 = text_translate;
		else if(EQ(v, "selected translate")) fn1 = selection_translate;

		else if(EQ(v, "word invertcase")) fn1 = word_invertcase;
		else if(EQ(v, "text invertcase")) fn1 = text_invertcase;
		else if(EQ(v, "selected invertcase")) fn1 = selection_invertcase;
		
		else if(EQ(v, "compose")) fn1 = compose;

		else { fprintf(stderr, "WARN: %s:%i: нет функции %s. Пропущено\n", path, lineno, v); continue; }

		// выделяем память под массив, если нужно
		if(keyfn_size == keyfn_max_size) {
			keyfn_max_size += KEYFN_NEXT_ALLOC;
			keyfn = realloc(keyfn, keyfn_max_size * sizeof(keyfn_t));
		}

		keyfn[keyfn_size++] = (keyfn_t) {key: key, fn: fn1, arg: arg};
		if(DEBUG) {
			keyfn_t *k = keyfn+keyfn_size-1;
			printf("------------------------------\n");
			KeySym ks = KEY_TO_SYM(k->key);
			print_sym(k->key.mods, ks);
			printf(" = %s\n", k->arg);
			fflush(stdout);
		}
	}

	fclose(f);
	
	// сортируем мнемоники по убыванию их длины
	int compose_map_cmp(const void* a, const void *b) {
		return ((compose_t*) b)->pos - ((compose_t*) a)->pos;
	}
	qsort(compose_map, compose_map_size, sizeof(compose_t), compose_map_cmp);
	
	if(DEBUG) fprintf(stderr, "Конфигурация применена!\n");
}

int main(int ac, char** av) {

	if(!ac) fprintf(stderr, "ERROR: а где путь к программе?\n");

	char* locale = "ru_RU.UTF-8";
	if(!setlocale(LC_ALL, locale)) {
		fprintf(stderr, "setlocale(LC_ALL, \"%s\") failed!\n", locale);
        return 1;
	}

	char* program = realpath(av[0], NULL);
	if(!program) {fprintf(stderr, "WARN: не найден реальный путь к программе %s\n", av[0]); return 1;}

	if(!goto_home()) return 1;

	init_desktop(program);	// меняет директорию на дир. пользователя
	free(program);
	
	open_display();
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

		event_delay(USEC_TO_DBL(loop_delay));
   	}

   	return 0;
}
