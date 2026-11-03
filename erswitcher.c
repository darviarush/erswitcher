/**************************************************************
 * Приложение: erswitcher - переключатель клавиатурного ввода *
 * Автор: Ярослав О. Косьмина                                 *
 * Лицензия: GPLv3                                            *
 * Местонахождение: https://github.com/darviarush/erswitcher  *
 **************************************************************/

// Юнит-тесты подключают этот файл через #include: main() в тестах свой
#ifndef ERSWITCHER_UNIT
#define ERSWITCHER_APPLICATION_MAIN 1
#endif

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
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <xkbcommon/xkbcommon.h>

extern char** environ;


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

//@category Иксы

Display *d;					// текущий дисплей
Window w;					// окно этого приложения
Window current_win;			// окно устанавливается при вводе с клавиатуры

int w_width = -1;			// ширина окна
int w_height = -1;			// высота окна

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
    int cursor_back;        // на сколько позиций назад вернуть курсор
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

    // XDisplayKeycodes (d, &min_keycode, &max_keycode);
    // XGetKeyboardMapping (d, min_keycode, (max_keycode - min_keycode + 1), &keysyms_per_keycode);

    // for (i = 0; i < 8; i++) {
		// for (int j = 0; j < map->max_keypermod; j++) {
			// if (map->modifiermap[k]) {
				// KeySym ks;
				// int index = 0;
				// do { ks = XKeycodeToKeysym(dpy, map->modifiermap[k], index++); } while ( !ks && index < keysyms_per_keycode);
				// char* nm = XKeysymToString(ks);

				// fprintf (fp, "%s  %s (0x%0x)", (j > 0 ? "," : ""), (nm ? nm : "BadKey"), map->modifiermap[k]);
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

    // maybe_group == -1 — ещё не смотрели ни одной раскладки; с -1
    // XkbKeycodeToKeysym() ничего не найдёт, поэтому пропускаем текущую
    if(maybe_group >= 0) {
        for(int code = 0; code < KEYBOARD_SIZE; ++code)
            for(int shift = 0; shift < 2; ++shift) {
                KeySym search_ks = XkbKeycodeToKeysym(d, code, maybe_group, shift);
                if(ks == search_ks)	return (unikey_t) {code: code, mods: shift? ShiftMask: 0, group: maybe_group};
            }
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
        if(begin) printf("+");
        else begin = 1;
        switch(mask) {
        case ShiftMask:
            printf("⇧Shift");
            break;
        case LockMask:
            printf("⇪🄰CapsLock");
            break;// ⇪🄰
        case ControlMask:
            printf("⌃⎈Ctrl");
            break;
        case Mod1Mask:
            printf("⌥⎇Alt");
            break;
        case Mod2Mask:
            printf("⇭NumLock");
            break; // ⓛ🄸
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
    //wint_t cs = xkb_keysym_to_utf32(ks);
    //if(cs) printf("%C", cs);
    //else
    switch(ks) {
    case XK_Escape:
        printf("⎋");
        break;
    case XK_BackSpace:
        printf("⌫←");
        break;
    case XK_Delete:
        printf("⌦");
        break;
    case XK_Return:
        printf("↵↩⏎⌤⌅⎆");
        break;
    case XK_Tab:
        printf("↹");
        break;
    case XK_Home:
        printf("⇱");
        break;
    case XK_End:
        printf("⇲");
        break;
    case XK_Menu:
        printf("≣");
        break;
    case XK_Pause:
        printf("⎉");
        break;
    case XK_Print:
        printf("⎙");
        break;
    case XK_Multi_key:
        printf("⎄");
        break;
    }
    printf("%s", XKeysymToString(ks));
}

// входит в массив
int in_sym(KeySym symbol, KeySym* symbols) {
    while(*symbols) if(symbol == *symbols++) return 1;
    return 0;
}

//@category Задержки

typedef struct {
    double time; 			// время в секундах, когда таймер должен сработать
    void(*fn)();
} mytimer_t;

#define MAX_TIMERS			256
mytimer_t timers[MAX_TIMERS];
int timers_size = 0;

#define TIMEVAL_TO_DBL(T)   (T.tv_sec + USEC_TO_DBL(T.tv_usec))

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
    timers[i].time = TIMEVAL_TO_DBL(curtime) + interval;

    if(timers_size <= i) timers_size = i+1;

    return i;
}

void timers_apply() {
    struct timeval curtime;
    gettimeofday(&curtime, 0);
    double now = TIMEVAL_TO_DBL(curtime);

    int i;
    int last = 0;
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
    if(!d) {
        fprintf(stderr, "Not open display!\n");
        exit(1);
    }

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
    printf("selection_chunk_size = %i\n", selection_chunk_size);
    fflush(stdout);

    // Создаём окно
    w = XCreateSimpleWindow(d, XDefaultRootWindow(d), -10, -10, 1, 1, 0, 0, 0x00DC143C);
    // подписываемся на события окна
    XSelectInput(d, w,
		  PropertyChangeMask 	// Работа с буфером
		| ButtonPressMask 		// Слушаем клики мышкой на окно
		| ExposureMask			// разрешаем запрос на перерисовку
		| StructureNotifyMask	// сработает, когда станут видны ширина и высота окна
	);

    // стыкуем окно с системным лотком
    send_systray_message(d, SYSTEM_TRAY_REQUEST_DOCK, w, 0, 0);
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
    int root_x, root_y, w_x, w_y;
    unsigned int mask;
    root = DefaultRootWindow(d);
    XQueryPointer(d, root, &dummy, &dummy,
                  &root_x, &root_y, &w_x, &w_y, &mask);
    return mask;
}

// Эмулирует нажатие или отжатие клавиши
void press(int code, int is_press) {
    unikey_t key = keyboard_state(code);
    printf("   %s: %i ", is_press? "press": "release", code);
    print_sym(key.mods, KEY_TO_SYM(key));
    printf("\n");
    XTestFakeKeyEvent(d, code, is_press? 1: 0, CurrentTime);
    XSync(d, False);
}

// устанавливает модификаторы. Локи пропускает
void send_mods(int mods, int is_press) {
	mods &= ~(LockMask|NumMask);

    XModifierKeymap *modifiers = XGetModifierMapping(d);

    for (int mod_index = ShiftMapIndex; mod_index <= Mod5MapIndex; mod_index++) {
        if(mods & (1 << mod_index)) {

            for(int mod_key = 0; mod_key < modifiers->max_keypermod; mod_key++) {
                int code = modifiers->modifiermap[mod_index * modifiers->max_keypermod + mod_key];
                if(code) {	// кейкод клавиши-модификатора
					press(code, is_press);
                    //if(delay) usleep(delay/2);
                    break;
                }
            }
        }
    }

    XFreeModifiermap(modifiers);
}

// устанавливает или снимает клавишу и модификаторы
void press_mods(int code, int mods, int is_press) {
	//XTestGrabControl(d, True);

	if(!is_press) press(code, 0);
	send_mods(mods, is_press);
	if(is_press) press(code, 1);

	//XTestGrabControl(d, False);
	//XFlush(d);
}

// устанавливает локи. Модификаторы пропускает
void set_locks(int mods) {
	int loks[2] = {LockMask, NumMask};
	int loks_sym[2] = {XK_Caps_Lock, XK_Num_Lock};

	for(int i=0; i<2; i++) {
		if(mods & loks[i]) {
			unikey_t k = SYM_TO_KEY(loks_sym[i]);

			press_mods(k.code, k.mods, 1);
			press_mods(k.code, k.mods, 0);
			usleep(delay/2);
		}
	}
}

// Переключает раскладку
void set_group(int group) {
	if(keyboard_state(0).group == group) return;

	// // пробуем переключить через нажатие клавиши
	// unikey_t k = SYM_TO_KEY(XK_ISO_Next_Group);
	// for(int i=0; i<8; i++) {
		// press_mods(k.code, k.mods, 1);
		// press_mods(k.code, k.mods, 0);
		// usleep(delay / 2);
		// if(keyboard_state(0).group == group) { printf("   set_group ISO: %i\n", group); return; }
	// }

    XkbLockGroup(d, XkbUseCoreKbd, group);
    XkbStateRec state;
    XkbGetState(d, XkbUseCoreKbd, &state);	// без этого вызова в силу переключение не вступит
    printf("set_group: %i\n", group);
	// TODO: Добавить задержку в опции
	usleep(delay); // тут задержку делаем побольше
}


// // приводит клавиатуру в соответсвие с модификаторами (нажимает управляющие клавиши или отжимает, если нужно)
// void sync_mods(int mods) {
	// int active_mods = keyboard_state(0).mods;
	// if(!(mods | active_mods)) return;

	// XModifierKeymap *modifiers = XGetModifierMapping(d);

    // for (int mod_index = ShiftMapIndex; mod_index <= Mod5MapIndex; mod_index++) {
		// if(!(1 << mod_index) & (LockMask|NumMask|ShiftMask)) continue;

		// int maybe_press = mods & (1 << mod_index);
		// int is_press = active_mods & (1 << mod_index);

		// // если должен быть нажат и не нажат или нажат и не должен быть нажат
        // if((maybe_press && !is_press) || (is_press && !maybe_press)) {

            // for(int mod_key = 0; mod_key < modifiers->max_keypermod; mod_key++) {
                // int code = modifiers->modifiermap[mod_index * modifiers->max_keypermod + mod_key];
                // if(code) {	// кейкод клавиши-модификатора найден
					// if((is_press|maybe_press) & (LockMask|NumMask)) {
						// press(code, 1);
						// press(code, 0);
					// }
					// else press(code, maybe_press? 1: 0); // нажимаем, если не нажат или отжимаем

                    // //if(delay/2) usleep(delay/2);
					// break;
                // }
            // }

        // }
    // }

    // XFreeModifiermap(modifiers);
// }

// Эмулирует нажатие или отжатие клавиши
void send_key(unikey_t key, int is_press) {
    if(key.code == 0) {
        printf("send_key: No Key!\n");
        return;
    }

    printf("send_key: %s %s\n", KEY_TO_STR(key), is_press == 2? "": (is_press? "PRESS": "RELEASE"));

	int xdelay = delay / 2;
	if(xdelay <= 0) xdelay = 1;

	// Группу нужно переключать до нажатия шифта, иначе первую может не переключать
	set_group(key.group);
	set_locks(key.mods);
    press_mods(key.code, key.mods, is_press);

	if(is_press == 2) {
		usleep(xdelay);
		press_mods(key.code, key.mods, 0);
		set_locks(key.mods);
	}

	usleep(xdelay);
}

// Эмулирует нажатие и отжатие клавиши
void press_key(unikey_t key) {
    send_key(key, 2);
}

int active_codes[KEYBOARD_SIZE];
int active_len;
unikey_t active_state;
int active_use = 0;

// Очищает активные модификаторы клавиатуры
// keys - массив клавиш KEYBOARD_SIZE = 32*8
// возвращает количество найденных клавиш-модификаторов
void clear_active_mods() {
    if(active_use) {
        fprintf(stderr, "ABORT: Вложенный clear_active_mods\n");
        exit(1);
    }
    active_use++;

	printf("===== clear_active_mods START =====\n");


    init_keyboard();

    active_state = keyboard_state(0);

    XModifierKeymap *modifiers = XGetModifierMapping(d);

    active_len = 0;

	// может быть зажат правый контрол, поэтому ищем среди соответствующих и нажатых
    for(int mod_index = ShiftMapIndex; mod_index <= Mod5MapIndex; mod_index++) {
        for(int mod_key = 0; mod_key < modifiers->max_keypermod; mod_key++) {
            int code = modifiers->modifiermap[mod_index * modifiers->max_keypermod + mod_key];
            if(code && BIT(keys, code)) active_codes[active_len++] = code;
        }
    }

    XFreeModifiermap(modifiers);

    // отжимаем модификаторы
    for(int i = 0; i<active_len; i++) press(active_codes[i], 0);

	set_locks(active_state.mods);

	printf("===== clear_active_mods END =====\n");
}

// восстанавливаем модификаторы
void recover_active_mods() {

	printf("===== recover_active_mods START =====\n");

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

	set_locks(active_state.mods);
    set_group(active_state.group);

	// хак для телеграма

	// XLowerWindow(d, current_win);
	// XRaiseWindow(d, current_win);
	if(active_state.group == group_ru) {
		//press_key(INT_TO_KEY(L','));

		// usleep(delay);

		// XEvent e;
		// memset(&e, 0, sizeof(XEvent));

		// e.xexpose = (XExposeEvent) {type: Expose, serial: 0, send_event: True, display: d, window: current_win, x: 1, y: 1, width: 10, height: 10, count: 0};

		// XSendEvent(d, current_win, True, Expose, &e);

		// usleep(delay);

		// press_key(SYM_TO_KEY(XK_Control_L));

		// unikey_t ks = SYM_TO_KEY(XK_Tab);
		// ks.mods |= AltMask;
		// press_key(ks);
		// ks.mods |= ShiftMask;
		// press_key(ks);
	}

	// char buf[1024];

	// for(int i=0; i<2; i++) {
		// sprintf(buf, "xdotool %s %lu", i==0? "windowminimize": "windowactivate", current_win);
		// printf("%s\n", buf);

		// int res = system(buf);
		// printf("res=%i\n", res);
	// }

	// memset(&e, 0, sizeof(XEvent));

	// e.xmotion = (XMotionEvent) {type: MotionNotify, state: Button1Mask, };

	// XSendEvent(d, current_win, True, MotionNotify, &e);


	// XTestGrabControl(d, True);
	// press(SYM_TO_KEY(XK_Control_L).code, 1);
	// usleep(delay/2);
	// press(SYM_TO_KEY(XK_Control_L).code, 0);
	// usleep(delay/2);
	// XTestGrabControl(d, False);
	// XFlush(d);

	printf("===== recover_active_mods END =====\n");

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

// Начало последнего слова в буфере: после последнего пробела,
// а если пробелов нет — с нуля
int from_space() {
    for(int from = pos - 1; from >= 0; from--) {
        if(iswspace(KEY_TO_INT(word[from]))) return from + 1;
    }
    return 0;
}

void press_key_multi(unikey_t key, int n) {
    for(int i=0; i<n; i++) press_key(key);
}

int utf8_strlen(const char *s) {
    int count = 0;
    while (*s) count += (*s++ & 0xC0) != 0x80;
    return count;
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
	// сразу меняем раскладку
    active_state.group = active_state.group == group_en? group_ru: group_en;

    // отправляем бекспейсы, чтобы удалить ввод
    if(backspace) press_key_multi(SYM_TO_KEY(XK_BackSpace), pos-from);

    // вводим ввод, но в альтернативной раскладке
    for(int i=from; i<pos; i++) {
        word[i].group = word[i].group == group_en? group_ru: word[i].group == group_ru? group_en: word[i].group;
        press_key(word[i]);
    }

    recover_active_mods();

	// в некоторых приложениях переключение клавиатуры не работает, пока не пошлёшь сигнал окну
	//send_notify_to_current_window();


	// printf("cur window: %li\n", w);

	// Window inFocus;
	// int rev;
	// XGetInputFocus(d, &inFocus, &rev);
	// printf("in focus window: %li %i\n", inFocus, rev);


	// XSetInputFocus(d, None, RevertToNone, CurrentTime);
	// XFlush(d);

	// XGetInputFocus(d, &inFocus, &rev);
	// printf("in focus window 2: %li %i\n", inFocus, rev);

	// XSetInputFocus(d, w, RevertToParent, CurrentTime);
	// XFlush(d);

	// XGetInputFocus(d, &inFocus, &rev);
	// printf("in focus window 3: %li %i\n", inFocus, rev);

	//XSetInputFocus(d, w, RevertToNone, CurrentTime);
	//XFlush(d);



	// press(SYM_TO_KEY(XK_Control_L).code, 1);
	// press(SYM_TO_KEY(XK_Control_L).code, 0);

}

void print_invertcase_buffer(int from, int backspace) {
    // word[pos] = 0;
    // printf("print_invertcase_buffer: %S\n", word+from);

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
    if(DEBUG) {
        printf("get_selection: owner = 0x%lX %s\n", owner, w == owner? "одинаков с w": "разный с w");
        fflush(stdout);
    }

    // если владелец буфера мы же, то и отдаём что есть
    if(w == owner) {
        if(DEBUG) {
            printf("get_selection: буфер - мы же\n");
            fflush(stdout);
        }
        on_get_selection = NULL;

        if(clipboard_s) {
            if(DEBUG) {
                printf("get_selection: Отдаю clipboard_s: %s\n", clipboard_s);
                fflush(stdout);
            }

            char* s = malloc(clipboard_len);
            memcpy(clipboard_s, s, clipboard_len);
            fn(s, clipboard_len, clipboard_target);
        }
        else {
            if(DEBUG) {
                printf("get_selection: Отдаю пустую строку\n");
                fflush(stdout);
            }
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

// нажимаем комбинацию Shift+LeftArrow (выделить предыдущий от курсора символ)
void shift_leftArrow() {
    clear_active_mods();
    unikey_t shift_left = SYM_TO_KEY(XK_Shift_L);
    send_key(shift_left, 1);
    press_key(SYM_TO_KEY(XK_Left));
    send_key(shift_left, 0);
    recover_active_mods();
    maybe_group = active_state.group;
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

// очистить буфер раздачи данных клипборда
void clipboard_free() {
    if(clipboard_s) {
        free(clipboard_s);
        clipboard_s = NULL;
    }
}

// устанавливаем в clipboard данные для передачи другим приложениям
// s должна быть в выделенной памяти, так как при новой установке или потере буфером фокуса она будет очищена
void set_clipboard(char* s, int len, Atom target) {
	clipboard_free();

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
        printf("read selection: s=`%s` len=%lu after=%lu selection_retrive_type=%s format=%i\n", result, length, bytesafter, selection_retrive_type_name, format);
        fflush(stdout);
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
		clipboard_free();
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
            if(DEBUG) {
                printf("отправляем clipboard_s: `%s` len=%i\n", clipboard_s, clipboard_len);
                fflush(stdout);
            }
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

            if(DEBUG) {
                printf("state == PropertyDelete\n");
                fflush(stdout);
            }
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
    case ButtonPress: // нажали на иконке в систрее
        // если окно tcl запущено - закрываем его
		//system("killall -9 erswitcher-configurator.tcl");
        if(config_window_pid) {
			int res = kill(config_window_pid, SIGKILL);
			if(DEBUG) { printf("config_window_pid=%i kill %i res=%i\n", config_window_pid, SIGKILL, res); fflush(stdout); }
		}
        // запускаем новое
        config_window_pid = fork();
        if(config_window_pid < 0) {
            perror("ERROR: fork не запустился");
        } else if(config_window_pid == 0) {
			const char* path = ".local/bin/erswitcher-configurator.tcl";
			char* const argv[] = {NULL};
            execve(path, argv, environ);
            fprintf(stderr, "ERROR: конфигуратор %s не запущен! Ошибка: %s\n", path, strerror(errno));
            exit(1);
        }
        break;
	case ConfigureNotify:	// размер окна
		w_width = event.xconfigure.width;
		w_height = event.xconfigure.height;
		printf("width: %i height: %i\n", w_width, w_height); fflush(stdout);
		break;
	case Expose:		// запрос на перерисовку окна ⌘
		if(event.xexpose.count != 0) break;

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
    for(int i=0; i<32; i++) if(state & (1<<i)) printf("%i", i);
        else printf(" ");
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
    if(!home) {
        fprintf(stderr, "WARN: нет getenv(HOME)\n");
        return 0;
    }
    if(chdir(home)) {
        fprintf(stderr, "WARN: не могу перейти в каталог пользователя %s\n", home);
        return 0;
    }
    setenv("PWD", home, 1);
    return 1;
}

void init_desktop(char* program) {

	const char* app = ".local/share/applications/erswitcher.desktop";

	if(access(app, R_OK) != 0) {

		mkdir(".local", 0700);
		mkdir(".local/share", 0700);
		mkdir(".local/share/applications", 0700);

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
	}

	const char* autostart = ".config/autostart/erswitcher.desktop";

	if(access(autostart, R_OK) != 0) {
		mkdir(".config", 0700);
		mkdir(".config/autostart", 0700);

		FILE* f = fopen(autostart, "wb");
		if(!f) {
			fprintf(stderr, "WARN: не могу создать %s/%s\n", getenv("HOME"), autostart);
			return;
		}

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
}

void check_any_instance() {
    // TODO: килять другой экземпляр процесса, если он есть программно
    // char* s = NULL;
    // asprintf(&s, "ps aux | grep erswitcher | awk '{print $2}' | grep -v '^%i$' | xargs kill -9", getpid());
    // int rc = system(s);
    // if(rc) fprintf(stderr, "%s завершилась с кодом %i - %s\n", s, rc, strerror(rc));
}

void insert_from_clipboard(char* s) {
	set_clipboard(strdup(s),
                  strlen(s),
                  utf8_atom);	// начинаем раздавать

    after(0.10, shift_insert);
}

/*

-2. У команды есть таймаут.
-1. Нельзя запустить несколько команд одновременно.
0. Копируем выделенное в буфер, а из него – в файл, если указан %i
1. Запускаем команду в отдельном процессе
2. Опрашиваем по таймеру: завершился ли процесс
3. Когда процесс завершился, то считываем файл %o и вставляем из буфера

*/

char* curcommand = NULL;
char* curcommand_infile = "/tmp/erswitcher-input";
char* curcommand_outfile = "/tmp/erswitcher-output";
pid_t curcommand_pid = 0;
float command_timeout = 13;    // В секундах
float command_interval = 0.1;  // Интервал через который будет запущена _interval_run_command
float curcommand_progress = 0;


int is_blank(char* s) {
    while(isspace(*s)) s++;
    return *s == '\0';
}

// Из формата делает путь
char* command_fmt2ptr(char* clipboard, char* curcommand) {
    FILE* f = fopen(curcommand_infile, "wb");
    if(!f) { fprintf(stderr, "Временный infile `%s` не открыт!\n", curcommand_infile); }
    fprintf(f, "%s", clipboard);
    fclose(f);

    char *ptr;
    size_t ptr_size;

    f = open_memstream(&ptr, &ptr_size);
    if(!f) { fprintf(stderr, "Файл в памяти не открыт!\n"); }

    char* v, *s = curcommand;
    while(NULL != (v = strchr(s, '%'))) {
        fprintf(f, "%.*s", (int)(v-s), s);
        v++;
        if(*v == 'i') fprintf(f, "%s", curcommand_infile);
        else if(*v == 'o') fprintf(f, "%s", curcommand_outfile);
        else if(*v == '%') fprintf(f, "%%");
        else {
            fprintf(stderr, "Замена в формате не валидна! `%s`\n", curcommand);
            fprintf(f, "%%%c", *v);
        }
        s = v + 1;
    }
    fprintf(f, "%s", s);

    // hello world!
    fclose(f);
    return ptr;
}

// Запускается каждые 0.1с и выводит \ | /
void _interval_run_command() {

    int status;
    pid_t completed = waitpid(curcommand_pid, &status, WNOHANG);
    if(DEBUG) fprintf(stderr, "completed %i exited %i signaled %i\n", completed, WIFEXITED(status), WIFSIGNALED(status));

    curcommand_progress += command_interval;

    // Не завершился
    if(completed == 0) {
        // Проверка таймаута
        if (curcommand_progress >= command_timeout) {
            fprintf(stderr, "TIMEOUT(%gs): pid=%i для команды `%s`\n", command_timeout, curcommand_pid, curcommand);
            kill(-curcommand_pid, SIGKILL);
            // Удаляем зомби
            while(waitpid(-1, NULL, WNOHANG) > 0);
            curcommand = NULL;
            curcommand_pid = 0;

            char* s = NULL;
            int result = asprintf(&s, "TIMEOUT %gs!", command_timeout);
            if(result == -1) {
                insert_from_clipboard("TIMEOUT!");
            } else {
                insert_from_clipboard(s);
                free(s);
            }
            return;
        }

        // printf("%g - %i = %g\n", curcommand_progress, (int) curcommand_progress, curcommand_progress - ((int) curcommand_progress));
        // if(curcommand_progress - ((int) curcommand_progress) < 0.02) {
        //     int x = ((int) curcommand_progress) % 4;
        //     insert_from_clipboard(
        //         x == 0? "/": (x == 1? "-": (x == 2? "\\": "|"))
        //     );
        //     shift_leftArrow();
        // }

        fprintf(stderr, "new after\n");

        after(command_interval, _interval_run_command);
        return;
    }

    if(strstr(curcommand, "%o")) {
        fprintf(stderr, "end cmd with %%o\n");

        FILE* f = fopen(curcommand_outfile, "rb");
        if(!f) {
            fprintf(stderr, "Файл команды не открыт\n");
        }
        else { // Отправляем в клипборд файл вывода
            fseek(f, 0L, SEEK_END);
            size_t size = ftell(f);
            char* buf_ptr = (char*) malloc(size + 1);
            fseek(f, 0L, SEEK_SET);
            fread(buf_ptr, size, 1, f);
            fclose(f);

            buf_ptr[size] = '\0';
            if(size) {
                if(buf_ptr[size-1] == '\n') buf_ptr[size-1] = '\0';
            }

            insert_from_clipboard(buf_ptr);
            free(buf_ptr);
        }
    }

    fprintf(stderr, "end cmd real\n");

    unlink(curcommand_outfile);
    unlink(curcommand_infile);
    curcommand = NULL;
}

void on_run_command(char* clipboard, int, Atom) {
    // Команда из формата. Не забыть очистить
    char* command_ptr = command_fmt2ptr(clipboard, curcommand);

    if(DEBUG) fprintf(stderr, "on_run_command: %s\n", command_ptr);

    curcommand_pid = fork();
    if(curcommand_pid == 0) {
        setpgid(0, 0);
        int status = system(command_ptr);
        exit(status);
    }

    if(curcommand_pid < 0) {
        fprintf(stderr, "ERROR: команда %s не форкнулась! Ошибка: %s\n", command_ptr, strerror(errno));
        free(command_ptr);
        return;
    }

    free(command_ptr);

    curcommand_progress = 0;
    after(command_interval, _interval_run_command);
}

void run_command(char* s) {
    if(!s) {
        fprintf(stderr, "Нет команды!\n");
        return;
    }

    if(is_blank(s)) {
        fprintf(stderr, "Пустая команда!\n");
        return;
    }

    if(curcommand) {
        fprintf(stderr, "Команда выполняется!\n");
        return;
    }

    curcommand = s;

    if(strstr(s, "%i")) {
        control_insert();
        get_selection(clipboard_atom, utf8_atom, on_run_command);       
    }
    else on_run_command("", 0, (Atom) NULL);
}

// Нажата клавиша компосе - нужно заменить мнемонику на значение
int cursor_back = 0;
void cursor_back__fn() {
    fprintf(stderr, "Курсор влево на %i позиций\n", cursor_back);
    press_key_multi(SYM_TO_KEY(XK_Left), cursor_back);
    cursor_back = 0;
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
            cursor_back = compose_map[i].cursor_back;
            break;
        }

    }

    if(to == NULL) {
        fprintf(stderr, "compose: Ничего не найдено!\n");
        return;
    }

    if(DEBUG) {
        printf("compose -> %s\n", to);
        fflush(stdout);
    }

    // удаляем мнемонику введённую ранее
    // Модификаторов тут не нужно: раскладку берём из последнего введённого символа,
    // как при печати — так работает и под тестами, где XkbGetState не настоящий
    unikey_t key = pos? word[pos-1]: keyboard_state(0);
    press_key_multi(SYM_TO_KEY(XK_BackSpace), remove_sym);
    set_group(key.group);
    // подменяем буфер собой и вставляем из него композэ
	insert_from_clipboard(to);

    if(cursor_back) after(0.30, cursor_back__fn);
}

void word_translate(char*) {
    print_translate_buffer(from_space(), 1);
}
void text_translate(char*) {
    print_translate_buffer(0, 1);
}
void selection_translate(char*) {
    copy_selection(print_translate_buffer);
}

void word_invertcase(char*) {
    print_invertcase_buffer(from_space(), 1);
}
void text_invertcase(char*) {
    print_invertcase_buffer(0, 1);
}
void selection_invertcase(char*) {
    copy_selection(print_invertcase_buffer);
}

int need_load_config = 0;
void signal_load_config(int) { need_load_config = 1; }

void load_config() {
    if(DEBUG) fprintf(stderr, "Конфигурация применяется ...\n");

    // clipboard_s = NULL;
    // clipboard_len = 0;
    // clipboard_target = utf8_atom;

    for(keyfn_t *k = keyfn, *n = keyfn + keyfn_size; k<n; k++) if(k->arg) free(k->arg);
    free(keyfn);
    keyfn = NULL;
    keyfn_max_size = keyfn_size = 0;

    for(int k = 0; k<compose_map_size; k++) free(compose_map[k].to);
    free(compose_map);
    compose_map = NULL;
    compose_map_max_size = compose_map_size = 0;

    char* path = ".config/erswitcher.conf";

    FILE* f = fopen(path, "rb");
    if(!f) {
        // каталог .config может отсутствовать — создаём
        mkdir(".config", 0755);
        f = fopen(path, "wb");
        if(!f) {
            fprintf(stderr, "WARN: не могу создать конфиг `%s`\n", path);
            return;
        }

		const char* etc = "/usr/share/erswitcher.conf.simple";
		FILE* c = fopen(etc, "rb");
		if(c) {
			#define BUF_SIZE_1M 	(1024*1024)
			char buf[BUF_SIZE_1M];
			int size;
			while(0 != (size = fread(buf, BUF_SIZE_1M, 1, c))) fwrite(buf, size, 1, f);
			fclose(c);
		}
		else
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
                "[options]\n"
                "# Задержка между вызовами клавиатуры в секундах\n"
                "loop_delay=0,01\n"
                "# Задержка между нажатиями клавишь в секундах\n"
                "delay=0,01\n"
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
                "n=¶\n"
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
                "**=∏\n"
                "*=×\n"
                "q=√\n"
                "<\\==≤\n"
                ">\\==≥\n"
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
                // ▄︻┻┳═ㅤㅤㅤ’’\̵͇ \з=(• - ̪●)=ε/̵͇ /’ ’ ㅤㅤㅤ═┳┻︻▄
                // 0:47 ━━●━━━━━━━━━━━ 3:30
                // ⇆ㅤㅤㅤㅤ◁ㅤㅤ❚❚ㅤㅤ▷ㅤㅤㅤㅤ↻
                "## SQL\n"
                "sele=SELECT *\\nFROM \\^ p\\nWHERE \\nORDER BY p.id DESC\\nLIMIT 100\\n\n"
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
                "Alt+Control+d=date > %%o\n"
                "Alt+Control+t=trans ru:en -b < %%i > %%o\n"
                "Alt+Control+y=trans en:ru -b < %%i > %%o\n"
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
#define SEC_OPTIONS     ((void (*)(char*)) -2)
    void (*fn)(char*) = NULL;

NEXT_LINE:
    while(fgets(buf, LINE_BUF_SIZE, f)) {
        lineno++;

        char* s = buf;
        while(isspace(*s)) s++;

        if(*s == '#' || *s == '\0') continue;
		s = buf;	// возвращаемся

        // удаляем пробелы в конце строки или только \n
        char* z = s;
        while(*z) z++;
        if(*s!='[' && fn != SEC_FUNCTIONS) {
            if(z[-1] == '\n') z--;
        }
        else {
            while(isspace(z[-1])) z--;
        }
        *z = '\0';


        if(*s == '[') { // это секция. Сверяемся со списком
            if(EQ(s, "[functions]")) fn = SEC_FUNCTIONS;
            else if(EQ(s, "[options]")) fn = SEC_OPTIONS;
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
        // TODO: выделение внутрь сниппетов

        // разбираем ключ
        char* v = s;
        while(*v) {
			if(*v == '\\') { v++; if(*v == '\0') break; v++; continue; }
			if(*v == '=') break;
			v++;
		}
        if(*v == '\0') {
            fprintf(stderr, "WARN: %s:%i: ошибка синтаксиса: нет `=` в `%s`. Пропущено\n", path, lineno, s);
            continue;
        }
        *v = '\0';
        v++;

        // меняем \char на символы
        char* compose_position_cursor = NULL;
        for(char* x = v; *x; x++) {
            if(*x == '\\') {
                if(x[1] == '^') {
                    for(char* f=x; *f; f++) *f = f[2];
                    compose_position_cursor = x;
                    continue;
                }

                int to = 0;
                switch(x[1]) {
                    case '\\': to = '\\'; break;
                    case 'n': to = '\n'; break;
                    case 't': to = '\t'; break;
                }

                if(to) {
                    *x=to;
                    for(char* f=x+1; *f; f++) *f = f[1];
                }
            }
        }

        // после =
        if(fn == SEC_OPTIONS) {
            if(EQ(s, "delay")) {
                delay = atof(v) * 1000000;
                if(delay <= 0 ) {
                    fprintf(stderr, "WARN: %s:%i: ошибка: delay в секундах, а не `%s`. Пропущено\n", path, lineno, v);
                    delay = DELAY;
                }
                else printf("delay = %i микросекунд\n", delay);
            }
            else if(EQ(s, "loop_delay")) {
                loop_delay = atof(v) * 1000000;
                if(loop_delay <= 0 ) {
                    fprintf(stderr, "WARN: %s:%i: ошибка: loop_delay в секундах, а не `%s`. Пропущено\n", path, lineno, v);
                    loop_delay = DELAY;
                }
                else printf("loop_delay = %i микросекунд\n", loop_delay);
            }
            else if(EQ(s, "command_timeout")) {
                command_timeout = atof(v);
                if(command_timeout <= 0 ) {
                    fprintf(stderr, "WARN: %s:%i: ошибка: command_timeout в секундах, а не `%s`. Пропущено\n", path, lineno, v);
                    command_timeout = 13;
                }
                else printf("command_timeout = %g секунд\n", command_timeout);
            }
            else if(EQ(s, "command_interval")) {
                command_interval = atof(v);
                if(command_interval <= 0 ) {
                    fprintf(stderr, "WARN: %s:%i: ошибка: command_interval в секундах, а не `%s`. Пропущено\n", path, lineno, v);
                    command_interval = 0.1;
                }
                else printf("command_interval = %g секунд\n", command_interval);
            }
            else {
                fprintf(stderr, "WARN: %s:%i: ошибка: неизвестная опция `%s`. Пропущено\n", path, lineno, s);
            }

            continue;
        }

        // это мнемоника
        if(fn == compose) {
            // выделяем память под массив, если нужно
            if(compose_map_size >= compose_map_max_size) {
                compose_map_max_size += KEYFN_NEXT_ALLOC;
                compose_map = realloc(compose_map, compose_map_max_size * sizeof(compose_t));
            }

            // разбираем ключ
            wint_t* word = compose_map[compose_map_size].word;
            int i = 0;
            int m = 0;

            FOR_UTF8(s) {
                STEP_UTF8(s, ws);
                if(ws == L'\\') {
                    if(m==0) {
                        m=1;
                        continue;
                    }
                }
                m = 0;

                if(i >= COMPOSE_KEY_MAX) {
                    fprintf(stderr, "WARN: %s:%i: строка слишком длинная \"%s\" > %i. Пропущенo\n", path, lineno, s, COMPOSE_KEY_MAX);
                    goto NEXT_LINE;
                }

                word[i++] = ws;
            }

            if(compose_position_cursor) {
                compose_map[compose_map_size].cursor_back = utf8_strlen(compose_position_cursor);
                printf("cursor_back = %i\n", compose_map[compose_map_size].cursor_back);
            }
            else compose_map[compose_map_size].cursor_back = 0;

            compose_map[compose_map_size].pos = i;
            compose_map[compose_map_size++].to = strdup(v);

			if(DEBUG) {printf("compose: %s ⟶ %s\n", s, v); fflush(stdout);}

            continue;
        }

        // определяем комбинацию клавиш
        unikey_t key = {0,0,0};
        int mods = 0;
        int key_set_flag = 0;

        char* x = strtok(s, "+");
        while (x != NULL) {
            if(key_set_flag) {
                fprintf(stderr, "WARN: %s:%i: ошибка синтаксиса: несколько клавишь-немодификаторов подряд (как %s в %s). Пропущено\n", path, lineno, x, s);
                goto NEXT_LINE;
            }

            if(EQ(x, "Shift")) mods |= ShiftMask;
            else if(EQ(x, "Control") || EQ(x, "Ctrl")) mods |= ControlMask;
            else if(EQ(x, "Alt") || EQ(x, "Option")) mods |= AltMask;
            else if(EQ(x, "Meta")) mods |= MetaMask;
            else if(EQ(x, "Super") || EQ(x, "Win") || EQ(x, "Command")) mods |= SuperMask;
            else if(EQ(x, "Hyper")) mods |= HyperMask;
            else {
                key = STR_TO_KEY(x);
                if(key.code == 0) {
                    fprintf(stderr, "WARN: %s:%i: не распознан символ %s. Пропущено\n", path, lineno, x);
                    goto NEXT_LINE;
                }

                key.mods = mods;
                key_set_flag = 1;
            }

            x = strtok(NULL, "+");
        };

        if(!key_set_flag) {
            fprintf(stderr, "WARN: %s:%i: ошибка синтаксиса: нет клавиши-немодификатора (в выражении %s). Пропущено\n", path, lineno, s);
            continue;
        }

        // определяем функцию
        char* arg = NULL;
        void (*fn1)(char*);

        if(fn != SEC_FUNCTIONS) {
            arg = strdup(v);
            fn1 = fn;
        }

        else if(EQ(v, "word translate")) fn1 = word_translate;
        else if(EQ(v, "text translate")) fn1 = text_translate;
        else if(EQ(v, "selected translate")) fn1 = selection_translate;

        else if(EQ(v, "word invertcase")) fn1 = word_invertcase;
        else if(EQ(v, "text invertcase")) fn1 = text_invertcase;
        else if(EQ(v, "selected invertcase")) fn1 = selection_invertcase;

        else if(EQ(v, "compose")) fn1 = compose;

        else {
            fprintf(stderr, "WARN: %s:%i: нет функции %s. Пропущено\n", path, lineno, v);
            continue;
        }

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
    // Пустой массив: compose_map == NULL, но qsort с пустым массивом
    // всё равно может вызвать сравнение и разыменовать NULL
    if(compose_map_size > 1) qsort(compose_map, compose_map_size, sizeof(compose_t), compose_map_cmp);

    if(DEBUG) fprintf(stderr, "Конфигурация применена!\n");
}

void app_run(int ac, char** av);

#ifdef ERSWITCHER_APPLICATION_MAIN
int main(int ac, char** av);
#include "main.c"
#endif
#undef main	// у тестов свой main(), см. ERSWITCHER_UNIT

void app_run(int ac, char** av) {

    if(!ac) fprintf(stderr, "ERROR: а где путь к программе?\n");

    char* locale = "ru_RU.UTF-8";
    if(!setlocale(LC_ALL, locale)) {
        fprintf(stderr, "setlocale(LC_ALL, \"%s\") failed!\n", locale);
        exit(1);
    }

    char* program = realpath(av[0], NULL);
    if(!program) {
        fprintf(stderr, "WARN: не найден реальный путь к программе %s\n", av[0]);
        exit(1);
    }

	// меняет директорию на дир. пользователя
    if(!goto_home()) exit(1);

    init_desktop(program); // создаёт десктопный файл, если его нет
    free(program);

    open_display();
    check_any_instance();
    memset(timers, 0, sizeof timers);	// инициализируем таймеры

    // Начальные установки
    current_win = get_current_window();
    pos = 0;
    init_keyboard();

    // конфигурация должна загружаться после инициализации клавиатуры
    load_config();

    signal(SIGHUP, signal_load_config);
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

		// обработка сигналов
		if(need_load_config) {
			need_load_config = 0;
			load_config();
		}
    }
}

//@category Заглушки для юнит-тестов

// Мок подключается последним: ему нужны уже описанные выше set_group(),
// keyboard_state() и прочие функции монолита. Иксы заменяются целиком,
// поэтому тестам не нужен живой X-сервер
#ifdef ERSWITCHER_UNIT
#include "tests/unit/mock.c"
#endif
