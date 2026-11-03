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

#include "src/domain/unikey/unikey.h"
#include "src/domain/input_buffer/input_buffer.h"
#include "src/domain/text/text.h"
#include "src/domain/case_invert/case_invert.h"
#include "src/domain/compose/compose.h"
#include "src/domain/command_template/command_template.h"
#include "src/use_cases/input/input.h"
#include "src/use_cases/translate/translate.h"
#include "src/ports/keyboard.h"
#include "src/ports/clipboard.h"
#include "src/ports/scheduler.h"
#include "src/ports/event_loop.h"
#include "src/infrastructure/x11/keyboard.h"
#include "src/infrastructure/x11/clipboard.h"
#include "src/infrastructure/x11/events.h"
#include "src/infrastructure/system/scheduler_time.h"

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
// размер буфера word (совпадает с доменным InputBuffer)
#define BUF_SIZE	INPUT_BUFFER_CAPACITY

input_buffer_t input;		// буфер ввода: word[], symbol[], pos
int keys_pressed;			// нажато клавишь


//@category Задержки

// TODO: задержку указывать в конфиге
// дефолтная задержка
#define DELAY 		    10000
// .0 - обязателен, иначе деление будет целочисленным
#define USEC_TO_DBL(U)	(U / 1000000.0)

int loop_delay = DELAY;			// задержка между опросами клавиатуры
pid_t config_window_pid = 0;	// pid конфигуратора

//@category Иксы

Display *d;					// текущий дисплей
Window w;					// окно этого приложения
Window current_win;			// окно устанавливается при вводе с клавиатуры

int w_width = -1;			// ширина окна
int w_height = -1;			// высота окна

// Временные макросы совместимости: состояние буфера обмена уже принадлежит
// X11-адаптеру. Они исчезнут вместе с монолитом после переноса app_run.
#define selection_chunk_size er_clipboard.selection_chunk_size
#define clipboard_s          er_clipboard.send_data
#define clipboard_pos        er_clipboard.send_pos
#define clipboard_len        er_clipboard.send_len
#define clipboard_target     er_clipboard.send_target
#define selection_retrive    er_clipboard.receive_data
#define selection_retrive_length er_clipboard.receive_len
#define selection_retrive_type   er_clipboard.receive_type
#define on_get_selection     er_clipboard.pending_callback
#define incr_is              er_clipboard.incr_is
#define incr_propid          er_clipboard.incr_property
#define incr_window          er_clipboard.incr_window
#define sel_data_atom        er_clipboard.sel_data_atom
#define utf8_atom            er_clipboard.utf8_atom
#define clipboard_atom       er_clipboard.clipboard_atom
#define targets_atom         er_clipboard.targets_atom
#define incr_atom            er_clipboard.incr_atom

void (*on_copy_selection)(int,int) = NULL;			// обработчик завершения копирования буфера обмена в буфер ввода

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

int compose_map_size = 0;
int compose_map_max_size = 0;
compose_t* compose_map = NULL;


//@category Раскладки

// Обёртки над портом клавиатуры: старые имена остаются для монолита и
// тестов, реализация Xkb/XTest живёт в infrastructure/x11/keyboard.c.
static keyboard_port_t kbd_port;

static void keyboard_port_ensure(void) {
    if(!kbd_port.init) keyboard_x11_port(&kbd_port, &er_kbd);
    er_kbd.display = d;
}

static input_use_case_t input_uc;
static void input_use_case_ensure(void) {
    keyboard_port_ensure();
    if(!input_uc.buffer) input_use_case_init(&input_uc, &input, &kbd_port);
    input_uc.keyboard = &kbd_port;
}

//@category Буфер обмена

static clipboard_port_t clipboard_port_state;

static void clipboard_port_ensure(void) {
    if(!clipboard_port_state.get) clipboard_x11_port(&clipboard_port_state, &er_clipboard);
    er_clipboard.display = d;
    er_clipboard.window = w;
}

static void get_clipboard(clipboard_get_callback_t fn, void* context) {
    clipboard_port_ensure();
    clipboard_port_state.get(&clipboard_port_state,
                              CLIPBOARD_SELECTION_CLIPBOARD,
                              CLIPBOARD_FORMAT_UTF8,
                              fn, context);
}

static int keyboard_current_group(void) {
    keyboard_port_ensure();
    return kbd_port.state(&kbd_port, 0).group;
}

void init_keyboard() {
    keyboard_port_ensure();
    kbd_port.init(&kbd_port);
}

KeySym unikey_to_keysym(unikey_t key) {
    keyboard_port_ensure();
    return (KeySym) kbd_port.to_keysym(&kbd_port, key);
}

unikey_t keyboard_state(int code) {
    keyboard_port_ensure();
    return kbd_port.state(&kbd_port, code);
}

unikey_t keysym_to_unikey(KeySym ks) {
    keyboard_port_ensure();
    return kbd_port.from_keysym(&kbd_port, (uint32_t) ks);
}

void set_group(int group) {
    keyboard_port_ensure();
    kbd_port.set_layout(&kbd_port, group);
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
    keyboard_x11_print_sym(mods, (uint32_t) ks);
}

// входит в массив
int in_sym(KeySym symbol, KeySym* symbols) {
    while(*symbols) if(symbol == *symbols++) return 1;
    return 0;
}

//@category Задержки

static scheduler_port_t scheduler_state;

static void scheduler_port_ensure(void) {
    if(!scheduler_state.after) scheduler_system_port(&scheduler_state, &er_scheduler);
    er_scheduler.debug = DEBUG;
}

// Единая точка постановки таймера: лениво инициализирует порт.
static int after(double interval, scheduler_callback_t fn, void* context) {
    scheduler_port_ensure();
    return scheduler_state.after(&scheduler_state, interval, fn, context);
}

static void scheduler_tick(void) {
    scheduler_port_ensure();
    scheduler_state.tick(&scheduler_state);
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

    // Не включаем отладочный синхронный режим Xlib: он делает каждый
    // XQueryKeymap/XGetInputFocus/XQueryPointer полноценным round-trip'ом.
    // При опросе клавиатуры каждые loop_delay это заметно увеличивает
    // задержку ввода, особенно под композитором/на медленном X-сервере.
    XSynchronize(d, False);

    // Создаём окно
    w = XCreateSimpleWindow(d, XDefaultRootWindow(d), -10, -10, 1, 1, 0, 0, 0x00DC143C);

    // Атомы и размер куска для Selection/INCR принадлежат clipboard-адаптеру
    clipboard_x11_init(&er_clipboard, d, w);
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
void send_key(unikey_t key, int is_press) {
    keyboard_port_ensure();
    kbd_port.send_key(&kbd_port, key, (keyboard_event_t) is_press);
}

// Эмулирует нажатие и отжатие клавиши
void press_key(unikey_t key) {
    keyboard_port_ensure();
    kbd_port.press_key(&kbd_port, key);
}

// Очищает активные модификаторы клавиатуры
void clear_active_mods() {
    keyboard_port_ensure();
    kbd_port.clear_active_mods(&kbd_port);
}

// восстанавливаем модификаторы
void recover_active_mods() {
    keyboard_port_ensure();
    kbd_port.recover_active_mods(&kbd_port);
}

void add_to_buffer(unikey_t key) {
    input_use_case_ensure();
    input_use_case_handle_key(&input_uc, key);
}

// Начало последнего слова в буфере: после последнего пробела,
// а если пробелов нет — с нуля
int from_space() {
    input_use_case_ensure();
    return input_use_case_from_space(&input_uc);
}

void press_key_multi(unikey_t key, int n) {
    keyboard_port_ensure();
    kbd_port.press_key_multi(&kbd_port, key, n);
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
    keyboard_port_ensure();
    translate_use_case_buffer(&kbd_port, input.word, input.pos, from, backspace);
}

void print_invertcase_buffer(int from, int backspace) {
    keyboard_port_ensure();
    translate_use_case_invertcase(&kbd_port, input.word, input.pos, from, backspace);
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

// переписываем строку в буфер ввода
void to_buffer(const char* s) {
    input_buffer_reset(&input);

    // utf8 переводим в символы юникода, затем в символы x11, после - в сканкоды
    FOR_UTF8(s) {
        STEP_UTF8(s, ws);
        if(input.pos >= BUF_SIZE) break;
        input_buffer_push(&input, INT_TO_KEY(ws), ws);
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
    kbd_port.set_layout_hint(&kbd_port, keyboard_current_group());
}

// нажимаем комбинацию Control+Insert (выделение добавить в буфер обмена (clipboard))
void control_insert() {
    clear_active_mods();
    unikey_t control_left = SYM_TO_KEY(XK_Control_L);
    send_key(control_left, 1);
    press_key(SYM_TO_KEY(XK_Insert));
    send_key(control_left, 0);
    recover_active_mods();
    kbd_port.set_layout_hint(&kbd_port, keyboard_current_group());
}

// копируем выделенное и заменяем им буфер ввода
void copy_selection_next(void* context, const char* s, int len, clipboard_format_t format) {
    (void) context;
    (void) len;
    (void) format;

    to_buffer(s);
    on_copy_selection(0, 0);
}
void copy_selection(void (*fn)(int, int)) {
    on_copy_selection = fn;
    control_insert();
    get_clipboard(copy_selection_next, NULL);
}

// нажимаем комбинацию Shift+Insert (вставка)
void shift_insert() {
    clear_active_mods();
    unikey_t shift_left = SYM_TO_KEY(XK_Shift_L);
    send_key(shift_left, 1);
    press_key(SYM_TO_KEY(XK_Insert));
    send_key(shift_left, 0);
    recover_active_mods();
    kbd_port.set_layout_hint(&kbd_port, keyboard_current_group());
}

// очистить буфер раздачи данных клипборда
void clipboard_free() {
    clipboard_port_ensure();
    clipboard_port_state.clear(&clipboard_port_state);
}

// s должна быть в выделенной памяти: порт забирает её себе
static void set_clipboard_utf8(char* s, int len) {
    clipboard_port_ensure();
    clipboard_port_state.set(&clipboard_port_state, s, len, CLIPBOARD_FORMAT_UTF8);
}

static event_loop_port_t event_loop_state;

static void event_loop_port_ensure(void) {
    if(!event_loop_state.wait) event_loop_x11_port(&event_loop_state, &er_event_loop);
    er_event_loop.display = d;
}

static void handle_x_event(void* context, const void* native_event) {
    (void) context;
    XEvent* event = (XEvent*) native_event;

    if(DEBUG) fprintf(stderr, "[%i %s], ", event->type, get_event_type(event->type));

    switch(event->type) {
    case SelectionNotify: // получить данные из буфера обмена
    case SelectionClear:
    case SelectionRequest: // запрос данных из буфера обмена
    case PropertyNotify:   // протокол INCR: отправка по частям
        clipboard_port_ensure();
        clipboard_x11_handle_event(&er_clipboard, event);
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
		w_width = event->xconfigure.width;
		w_height = event->xconfigure.height;
		printf("width: %i height: %i\n", w_width, w_height); fflush(stdout);
		break;
	case Expose:		// запрос на перерисовку окна ⌘
		if(event->xexpose.count != 0) break;

		break;
    }

}

void event_delay(double seconds) {
    // задержка, но коль пришли события иксов - сразу выходим
    event_loop_port_ensure();
    event_loop_state.wait(&event_loop_state, seconds);
    event_loop_state.drain(&event_loop_state, handle_x_event, NULL);
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
    if(key.group == kbd_port.layout_en(&kbd_port)) printf("en");
    else if(key.group == kbd_port.layout_ru(&kbd_port)) printf("ru");
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

    input_use_case_ensure();
    input_use_case_handle_key(&input_uc, key);
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

static void after_shift_insert(void* context) {
    (void) context;
    shift_insert();
}

void insert_from_clipboard(char* s) {
    set_clipboard_utf8(strdup(s), strlen(s)); // начинаем раздавать

    after(0.10, after_shift_insert, NULL);
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
char* command_fmt2ptr(const char* clipboard, const char* curcommand) {
    FILE* f = fopen(curcommand_infile, "wb");
    if(!f) { fprintf(stderr, "Временный infile `%s` не открыт!\n", curcommand_infile); }
    fprintf(f, "%s", clipboard);
    fclose(f);

    int invalid = 0;
    char* ptr = command_template_expand(curcommand, curcommand_infile, curcommand_outfile, &invalid);
    if(invalid) fprintf(stderr, "Замена в формате не валидна! `%s`\n", curcommand);
    return ptr;
}

// Запускается каждые 0.1с и выводит \ | /
void _interval_run_command(void* context) {
    (void) context;

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

        after(command_interval, _interval_run_command, NULL);
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

void on_run_command(void* context, const char* clipboard, int len, clipboard_format_t format) {
    (void) context;
    (void) len;
    (void) format;

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
    after(command_interval, _interval_run_command, NULL);
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
        get_clipboard(on_run_command, NULL);
    }
    else on_run_command(NULL, "", 0, CLIPBOARD_FORMAT_UTF8);
}

// Нажата клавиша компосе - нужно заменить мнемонику на значение
int cursor_back = 0;
void cursor_back__fn(void* context) {
    (void) context;
    fprintf(stderr, "Курсор влево на %i позиций\n", cursor_back);
    press_key_multi(SYM_TO_KEY(XK_Left), cursor_back);
    cursor_back = 0;
}

void compose(char*) {
    if(input.pos == 0) return;

    wint_t symbols[BUF_SIZE];
    for(int i = 0; i < input.pos; i++) symbols[i] = KEY_TO_INT(input.word[i]);

    int match = compose_find(compose_map, compose_map_size, symbols, input.pos);
    if(match < 0) {
        fprintf(stderr, "compose: Ничего не найдено!\n");
        return;
    }

    compose_t* entry = &compose_map[match];
    char* to = entry->to;
    int remove_sym = entry->pos;

    cursor_back = entry->cursor_back;

    if(DEBUG) {
        printf("compose -> %s\n", to);
        fflush(stdout);
    }

    // удаляем мнемонику введённую ранее
    // Модификаторов тут не нужно: раскладку берём из последнего введённого символа,
    // как при печати — так работает и под тестами, где XkbGetState не настоящий
    unikey_t key = input.pos? input.word[input.pos-1]: keyboard_state(0);
    press_key_multi(SYM_TO_KEY(XK_BackSpace), remove_sym);
    set_group(key.group);
    // подменяем буфер собой и вставляем из него композэ
	insert_from_clipboard(to);

    if(cursor_back) after(0.30, cursor_back__fn, NULL);
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
                int delay_us = atof(v) * 1000000;
                if(delay_us <= 0 ) {
                    fprintf(stderr, "WARN: %s:%i: ошибка: delay в секундах, а не `%s`. Пропущено\n", path, lineno, v);
                    delay_us = DELAY;
                }
                else printf("delay = %i микросекунд\n", delay_us);
                keyboard_port_ensure();
                kbd_port.set_delay(&kbd_port, delay_us);
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
    scheduler_system_reset(&er_scheduler);	// инициализируем таймеры

    // Начальные установки
    current_win = get_current_window();
    input_buffer_init(&input);
    init_keyboard();

    // конфигурация должна загружаться после инициализации клавиатуры
    load_config();

    signal(SIGHUP, signal_load_config);
    signal(SIGCHLD, SIG_IGN);

    unsigned int state1, state2 = 0;
    kbd_port.poll_saved(&kbd_port);


    while(1) {

        // сбросим ввод, коль нажата мышка
        state1 = get_input_state();
        state1 &= Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask;
        if(state1 != state2) input_buffer_reset(&input);
        state2 = state1;

        // Если сменилось окно, то начинаем ввод с начала
        Window w = get_current_window();
        if(w != current_win) {
            input_buffer_reset(&input);
            current_win = w;
        }

        // опрашиваем клавиатуру
        const char* keys = kbd_port.keymap(&kbd_port);
        const char* saved = kbd_port.keymap_saved(&kbd_port);
        kbd_port.poll(&kbd_port);
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

        kbd_port.swap_keymap(&kbd_port);

        // крошим зомбиков
        int status;
        waitpid(-1, &status, WNOHANG);

        // сработают таймеры, чьё время пришло
        scheduler_tick();

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
