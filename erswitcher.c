#include <locale.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/XKB.h>
#include <X11/extensions/XTest.h>
#include <xkbcommon/xkbcommon.h>
#include <string.h>
#include <stdlib.h>
#include <wctype.h>
#include <wchar.h>
#include <unistd.h>


/**

sym - префикс для функций обрабатывающих KeySym 

 */


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
wint_t word[BUF_SIZE];

typedef struct {
	int code;
	int mods;
	int group;
} unikey_t;

// ответ для get_key()
unikey_t key;

// набор клавиатур: keyboard[group][shift][scancode] -> utf32
KeySym keyboard[XkbNumKbdGroups][2][KEYBOARD_SIZE];

int groups = 0;			// Количество раскладок
int group_ru = -1;		// Номер русской раскладки или -1
int group_en = -1;		// Номер английской раскладки или -1

// инициализирует названия клавиатуры
static char* Russian = "Russian";
static char* English = "English";
void init_keyboard() {
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

// инициализирует массив keyboard
void keysym_init() {
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

KeySym invertcase(KeySym ks) {
	if(!get_key(ks)) return NoSymbol;

	KeySym to_ks = keyboard[key.group][key.mods? 0: 1][key.code];
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

// c прошлой проверки состояние клавиатуры изменилось
int keys_change() {
    XQueryKeymap(d, keys);
    for(int i=0; i<KEYBOARD_SIZE; i++) {
        if(BIT(keys, i)!=BIT(saved, i)) {
            return 1;
        }
    }
	return 0;
}

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
			case ShiftMask: printf("Shift"); break;
			case LockMask: printf("CapsLock"); break;
			case ControlMask: printf("Ctrl"); break;
			case Mod1Mask: printf("Alt"); break;
			case Mod2Mask: printf("NumLock"); break;
			case Mod3Mask: printf("Meta"); break;
			case Mod4Mask: printf("Super"); break;
			case Mod5Mask: printf("Hyper"); break;
			default:
				printf("Mod%i", i);
		}
	}

	if(begin) printf("+");
	//wint_t cs = xkb_keysym_to_utf32(ks);
	//if(cs) printf("%C", cs);
	//else 
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

// Переключает раскладку
void set_group(int group) {
	if(get_group() == group) return;	
    XkbLockGroup(d, XkbUseCoreKbd, group);
    get_group();	// без этого вызова в силу переключение не вступит
    printf("set_group: %i\n", group);
}

// Эмулирует нажатие или отжатие клавиши
void press(int code, int is_press) {
	printf("   press: %i %s\n", code, is_press? "PRESS": "RELEASE");
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

// Эмулирует нажатие и отжатие клавиши
void press_key(KeySym ks) {
	send_key(ks, 1);
	send_key(ks, 0);
}

int active_codes[KEYBOARD_SIZE];
int active_len;
int active_group;
int active_mods;
int active_use = 0;

// Возвращает активные модификаторы клавиатуры
// keys - массив клавиш KEYBOARD_SIZE = 32*8
// возвращчет количество найденных клавиш-модификаторов
void clear_active_mods() {
	if(active_use) {
		fprintf(stderr, "Вложенный clear_active_mods\n");
		exit(1);
	}
	active_use++;
	active_mods = get_mods();
	active_group = get_group();
	
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
	if(active_mods & LockMask) {
		press_key(XK_Caps_Lock);
	}
}

// восстанавливаем модификаторы
void recover_active_mods() {
	set_group(active_group);

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
	int mods = get_mods();
	if(active_mods & LockMask && !(mods & LockMask)) {
		press_key(XK_Caps_Lock);
	}
}

int get_shift_level(int mods) {
    // нажата одна из шифта или капслок
	return ((mods & (ShiftMask | LockMask))
		// и не нажаты обе
		&& !(mods & ShiftMask && mods & LockMask))? 1: 0;
}

KeySym key_to_sym(int code, int group, int mods) {
	int shiftLevel = get_shift_level(mods);
	
	KeySym ks = keyboard[group][shiftLevel][code];
	if(ks) return ks;
    //KeySym ks = XkbKeycodeToKeysym(d, code, group, shiftLevel);
	
	for(int i=0; i<XkbNumKbdGroups; i++) {
		ks = keyboard[i][shiftLevel][code];
		if(ks) return ks;
	}
	
	shiftLevel = shiftLevel? 0: 1;
	for(int i=0; i<XkbNumKbdGroups; i++) {
		ks = keyboard[i][shiftLevel][code];
		if(ks) return ks;
	}
	
	return ks;
}

void add_to_buffer(int code) {
	XkbStateRec state;
    XkbGetState(d, XkbUseCoreKbd, &state);

	// Если нажаты какие-то ещё модификаторы, контрол или альт - выходим
	if(state.mods & ~(LockMask|Mod2Mask)) {
		return;
	}

	KeySym ks = key_to_sym(code, state.group, state.mods);
	wint_t cs = xkb_keysym_to_utf32(ks);
	
	// Если это переход на другую строку, то начинаем ввод с начала
	KeySym is_control[] = {XK_Home, XK_Left, XK_Up, XK_Right, XK_Down, XK_Prior, XK_Page_Up, XK_Next, XK_Page_Down, XK_End, XK_Begin, XK_Tab, XK_Return, 0};
	if(in_sym(ks, is_control)) {
		pos = 0;
		return;
	}
	
	// Если символ не печатный, то пропускаем
	KeySym is_print[] = {XK_space, XK_underscore, 0};
	if(!iswprint(cs) && !in_sym(ks, is_print)) {
		return; 
	}
	
	// Записываем символ в буфер с его раскладкой клавиатуры
	if(pos >= BUF_SIZE) pos = 0;
	word[pos++] = cs;
	printf("press! %S\n", word);
}

int from_space() {
	int from=pos-1;
	for(; from>0 && iswspace(word[from]); from--);
	for(; from>0; from--) if(iswspace(word[from])) break;
	return from;
}

void print_translate_buffer(int from, KeySym(*translate_fn)(KeySym)) {
	clear_active_mods();
	
	// отправляем бекспейсы, чтобы удалить ввод
	for(int i=from; i<pos; i++) {
		press_key(XK_BackSpace);
	}
	
	// вводим ввод, но в альтернативной раскладке
	for(int i=from; i<pos; i++) {
		KeySym ks = translate_fn(xkb_utf32_to_keysym(word[i]));
		press_key(ks);
		word[i] = xkb_keysym_to_utf32(ks);
	}
	
	recover_active_mods();
	
	// меняем group раскладку
	int trans_group = active_group == group_en? group_ru: group_en;
	set_group(trans_group);
}

// копируем в наш буфер выделение
char* copy_selection(Atom number_buf) {
	// Создаём окно
	int black = BlackPixel(d, DefaultScreen(d));
	int root = XDefaultRootWindow(d);
	Window w = XCreateSimpleWindow(d, root, 0, 0, 1, 1, 0, black, black);

	// подписываемся на события окна
	XSelectInput(d, w, PropertyChangeMask);

	// запрос на получение выделенной области
	Atom sel_data_atom = XInternAtom(d, "XSEL_DATA", False);
	Atom utf8_atom = XInternAtom(d, "UTF8_STRING", True);
	if(!utf8_atom) utf8_atom = XA_STRING;
	Atom request_target = utf8_atom;
	//Atom selection = XA_PRIMARY;

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

		word[pos++] = ws[0];
    }

	XFree(s);
	*s1 = NULL;
}

void change_key(int code) {
    
	XkbStateRec state;
    XkbGetState(d, XkbUseCoreKbd, &state);

    KeySym ks = key_to_sym(code, group_en, 0);
    
	// XkbKeysymToModifiers()
	printf("change_key %i: ", code);
	print_sym(state.mods, key_to_sym(code, state.group, state.mods));
	printf("\n");
	fflush(stdout);
    // нажата комбинация? выполняем действие
    //int dues = get_dues(state.group, state.mods, ks);
	int mods = state.mods & ~(LockMask|Mod2Mask);
	
	if(ks == XK_BackSpace && mods == 0) {
		if(pos != 0) --pos;
	}
	else if(ks == XK_Pause && mods == 0) {
		print_translate_buffer(from_space(), translate);
	}
	else if(ks == XK_Pause && mods == ControlMask) {
		print_translate_buffer(0, translate);
	}
	else if(ks == XK_Pause && mods == ShiftMask) {
		char* s = copy_selection(XA_PRIMARY);
		to_buffer(&s);
		// to_buffer очищает память выделенную для s через XFree
		print_translate_buffer(0, translate);
	}
	else if(ks == XK_Pause && mods == AltMask) {
		print_translate_buffer(from_space(), invertcase);
	}
	else if(ks == XK_Pause && mods == (AltMask|ControlMask)) {
		print_translate_buffer(0, invertcase);
	}
	else if(ks == XK_Pause && mods == (AltMask|ShiftMask)) {
		char* s = copy_selection(XA_PRIMARY);
		to_buffer(&s);
		print_translate_buffer(0, invertcase);
	}
	else {
		// заносим в буфер
        add_to_buffer(code);
    }    
}

int main() {
	char* locale = "ru_RU.UTF-8";
	if(!setlocale(LC_ALL, locale)) {
		fprintf(stderr, "setlocale(LC_ALL, \"%s\") failed!\n", locale);
        return 1;
	}

	// FILE* f = fopen("/home/dart/1.log", "wb");
	// if(!f) { fprintf(stderr, "Not open log file!\n"); exit(1); }
	// fprintf(f, "%lx\n", (unsigned long)d);
	// fclose(f);

	open_display();

	// Начальные установки
	current_win = get_current_window();
	pos = 0;
	init_keyboard();
	keysym_init();

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