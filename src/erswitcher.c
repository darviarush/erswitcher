#include <locale.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <xkbcommon/xkbcommon.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <wctype.h>

#include "keyboard.h"
#include "selection.h"
#include "erswitcher.h"


// распечатывает состояние клавиш
void print_state(XkbStateRec* state);

// входит в массив
int in_sym(KeySym symbol, KeySym* symbols) {
	while(*symbols) if(symbol == *symbols++) return 1;
	return 0;
}

void print_mods(int mods) {
	char s[2] = "";
	printf("mods: %i - ", mods);
	if(mods & ShiftMask) { printf("Shift"); s[0]='+';}
	if(mods & LockMask) { printf("%sCapsLock", s); s[0]='+';}
	if(mods & ControlMask) { printf("%sCtrl", s); s[0]='+';}
	if(mods & Mod1Mask) { printf("%sAlt", s); s[0]='+';}
	if(mods & Mod2Mask) { printf("%sNumLock", s); s[0]='+';}
	if(mods & Mod3Mask) { printf("%sMod3Mask", s); s[0]='+';}
	if(mods & Mod4Mask) { printf("%sWinKey", s); s[0]='+';}
	if(mods & Mod5Mask) { printf("%sMod5Mask", s); s[0]='+';}
	printf("\n");
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

Display *d;					// текущий дисплей
Window current_win;			// окно устанавливается при вводе с клавиатуры

// буфера
static wint_t word_buf[BUF_SIZE];
wint_t *word = word_buf;
static wint_t trans_buf[BUF_SIZE];
wint_t *trans = trans_buf;
int pos = 0;

void change_key(int code, char* keys) {
	printf("change_key! code=%d\n", code); fflush(stdout);

	XkbStateRec state;
	XkbGetState(d, XkbUseCoreKbd, &state);
	//print_state(&state);
	//print_mods(state.mods);

	// Если нажаты какие-то ещё модификаторы, контрол или альт - выходим
	if(state.mods & ~(ShiftMask|LockMask|Mod2Mask)) {
		return;
	}

	// нажата одна из шифта или капслок
	int shiftLevel = ((state.mods & (ShiftMask | LockMask))
		// и не нажаты обе
		&& !(state.mods & ShiftMask && state.mods & LockMask))? 1: 0;
	//printf("shiftLevel=%i\n", shiftLevel);

	// транспонированная раскладка
	int trans_group = state.group == group_en? group_ru: group_en;

	// 8-байтный символ иксов
	KeySym ks = XkbKeycodeToKeysym(d, code, state.group, shiftLevel);
	KeySym ks_trans = XkbKeycodeToKeysym(d, code, trans_group, shiftLevel);
	KeySym ks_pause = XkbKeycodeToKeysym(d, code, group_en, 0);

    // Пробел почему-то существует только в английской раскладке
    if(ks == NoSymbol) ks = ks_trans;
    if(ks_trans == NoSymbol) ks_trans = ks;

	const char *s1 = XKeysymToString(ks);
	const char *s_pause = XKeysymToString(ks_pause);
	//if(strlen(s1) != 1) return;

	// Если сменилось окно, то начинаем ввод с начала
	Window w = get_current_window();
	if(w != current_win) {
		pos = 0;
		current_win = w;
	}

	wint_t cs = xkb_keysym_to_utf32(ks);
	wint_t cs_trans = xkb_keysym_to_utf32(ks_trans);
	//wint_t cs_pause = xkb_keysym_to_utf32(ks_pause);

	printf ("0x%04lx -> 0x%04x: %s `%C` en: %s\n", ks, cs, s1, cs, s_pause);

	// BackSpace - удаляем последний символ
	if(ks_pause == XK_BackSpace) {
		if(pos != 0) --pos;
		word[pos] = trans[pos] = 0;
		printf("backspace: %S -> %S\n", word, trans);
		return;
	}

	// нажата Pause - переводим
	if(ks_pause == XK_Pause) {
		// сохраняем состояние
		int active_mods[KEYBOARD_SIZE];
		int nactive_mods = get_active_mods(active_mods);
		//clear_active_mods(active_mods, nactive_mods);
		// отжимаем модификаторы
		for(int i = 0; i<nactive_mods; i++) press(active_mods[i], 0);

		// снимаем Капс-лок (он не входит в модификаторы)
        if(state.mods & LockMask) {
            press_key(XK_Caps_Lock);
        }

		int from = 0;

		// нажата Shift+Pause - переводим выделенный фрагмент
		if(state.mods & ShiftMask) {

			// printf("primary: %s\n", copy_selection(XA_PRIMARY));
			// printf("secondary: %s\n", copy_selection(XA_SECONDARY));

			// send_key(XK_Control_L, 1);
			// press_key(XK_c);
			// send_key(XK_Control_L, 0);

			// printf("primary: %s\n", copy_selection(XA_PRIMARY));
			// printf("secondary: %s\n", copy_selection(XA_SECONDARY));


			// send_key(XK_Control_L, 1);
			// press_key(XK_Right);
			// send_key(XK_Control_L, 0);
			

			// printf("primary: %s\n", copy_selection(XA_PRIMARY));
			// printf("secondary: %s\n", copy_selection(XA_SECONDARY));

			// XA_PRIMARY, XA_SECONDARY
			// if(!copy_selection(XA_PRIMARY)) 

			char* s = copy_selection(XA_PRIMARY);
			int res = to_buffer(&s);
			// to_buffer удаляет s

			if(!res) goto END_TRANS;
		}
		else {
			from=pos-1;
			for(; from>0 && iswspace(trans[from]); from--);
			for(; from>0; from--) if(iswspace(trans[from])) break;
			// отправляем бекспейсы, чтобы удалить ввод
			for(int i=from; i<pos; i++) {
				press_key(XK_BackSpace);
			}
		}

		word[pos] = trans[pos] = 0;
		printf("PRESS PAUSE: `%S` -> `%S`\n", word, trans);

		// Если нечего переводить, то показываем сообщение на месте ввода
		if(pos == 0) {
			//...
			// и меняем раскладку и буфера
			set_group(trans_group);
			swap_buf();
			goto END_TRANS;
		}

		// вводим ввод, но в альтернативной раскладке
		for(int i=from; i<pos; i++) {
			press_key(xkb_utf32_to_keysym(trans[i]));
		}

		// меняем буфера местами
		swap_buf();

		
		END_TRANS:

		// восстанавливаем модификаторы
		set_group(state.group);

		char active_keys[32];
		XQueryKeymap(d, active_keys);
      	for(int i=0; i<nactive_mods; i++) {
      		int code = active_mods[i];
      		// клавиша не была отжата пока происходил ввод
      		if(BIT(keys, code)==BIT(active_keys, code)) {
      			press(code, 1);
      		}
      	}

		//set_active_mods(active_mods, nactive_mods);

        // восстанавливаем Капс-лок (он не входит в модификаторы)
        int mods = get_mods();
        if(state.mods & LockMask && !(mods & LockMask)) {
            press_key(XK_Caps_Lock);
        }

		// меняем group раскладку
		set_group(trans_group);

		return;
	}

	// Если это переход на другую строку, то начинаем ввод с начала
	KeySym is_control[] = {XK_Home, XK_Left, XK_Up, XK_Right, XK_Down, XK_Prior, XK_Page_Up, XK_Next, XK_Page_Down, XK_End, XK_Begin, XK_Tab, XK_Return, 0};
	if(in_sym(ks_pause, is_control)) {
		printf("is control!\n");
		pos = 0;
		return;
	}

	//if(cs == 0) { pos = 0; return; } // разные управляющие клавиши
	//if(iswspace(cs)) { pos = 0;	return;	}
	// Если символ не печатный, то пропускаем
	KeySym is_print[] = {XK_space, XK_underscore, 0};
	if(!iswprint(cs) && !in_sym(ks, is_print)) {
		printf("not isprint! `%C`\n", cs);
		return; 
	}

	// Записываем символ в буфер с его раскладкой клавиатуры
	if(pos >= BUF_SIZE) pos = 0;
	trans[pos] = cs_trans;
	word[pos++] = cs;
	trans[pos] = word[pos] = 0;
	printf("press! %S -> %S\n", word, trans);
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

	char* display = XDisplayName(NULL);

	printf("display: %s\n", display);
	printf("size KeySym: %li\n", sizeof(KeySym));
	printf("size wchar_t: %li\n", sizeof(wchar_t));
	printf("size wint_t: %li\n", sizeof(wint_t));
	printf("size *S: %li\n", sizeof(*(L"Л")));

	open_display();

	// Начальные установки
	current_win = get_current_window();
	pos = 0;
	keysym_init();

    unsigned int state1, state2 = 0;
	char buf1[32], buf2[32];
	char *saved=buf1, *keys=buf2;
   	XQueryKeymap(d, saved);

   	while (1) {
        // сбросим ввод, коль нажата мышка
        state1 = get_input_state();
        state1 &= Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask;
        if(state1 != state2) pos = 0;
        state2 = state1;

        XQueryKeymap(d, keys);
      	for(int i=0; i<KEYBOARD_SIZE; i++) {
      		if(BIT(keys, i)!=BIT(saved, i)) {
      			if(BIT(keys, i)!=0) { // клавиша нажата
      				change_key(i, keys);
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


// возвращает текущее окно
Window get_current_window() {
	Window w = 0;
  	int revert_to = 0;
	XGetInputFocus(d, &w, &revert_to);
	return w;
}

// подключается к дисплею
void open_display() {
	d = XOpenDisplay(NULL);
	if(!d) { fprintf(stderr, "Not open display!\n"); exit(1); }

	XSetErrorHandler(null_X_error);

	XSynchronize(d, True);
}

// переподключается к дисплею
void reset_display() {
	XCloseDisplay(d);
	open_display();
}

// печатает в stdout состояние клавиатуры
void print_state(XkbStateRec* state) {
	printf("unsigned char	group: %i\n", (unsigned int) (state->group));
	printf("unsigned char   locked_group: %i\n", (unsigned int) (state->locked_group));
	printf("unsigned short	base_group: %i\n", (unsigned int) (state->base_group));
	printf("unsigned short	latched_group: %i\n", (unsigned int) (state->latched_group));
	printf("unsigned char	mods: %i\n", (unsigned int) (state->mods));
	printf("unsigned char	base_mods: %i\n", (unsigned int) (state->base_mods));
	printf("unsigned char	latched_mods: %i\n", (unsigned int) (state->latched_mods));
	printf("unsigned char	locked_mods: %i\n", (unsigned int) (state->locked_mods));
	printf("unsigned char	compat_state: %i\n", (unsigned int) (state->compat_state));
	printf("unsigned char	grab_mods: %i\n", (unsigned int) (state->grab_mods));
	printf("unsigned char	compat_grab_mods: %i\n", (unsigned int) (state->compat_grab_mods));
	printf("unsigned char	lookup_mods: %i\n", (unsigned int) (state->lookup_mods));
	printf("unsigned char	compat_lookup_mods: %i\n", (unsigned int) (state->compat_lookup_mods));
	printf("unsigned short	ptr_buttons: %i\n", (unsigned int) (state->ptr_buttons));
}

// // транслитерирует текст с русской раскладки в английскую
// static wint_t *ru = L"ё1234567890-="
// 					L"йцукенгшщзхъ"
// 					L"фывапролджэ\\"
// 					L"/ячсмитьбю."
// 					;
// static wint_t *en = L"`1234567890-=" 
// 					L"qwertyuiop[]"
// 					L"asdfghjkl;'\\"
// 					L"<zxcvbnm,./"
// 					;
// static wint_t *RU = L"Ё!\"№;%:?*()_+"
// 					L"ЙЦУКЕНГШЩЗХЪ"
// 					L"ФЫВАПРОЛДЖЭ/"
// 					L"|ЯЧСМИТЬБЮ,"
// 					;
// static wint_t *EN = L"~!@#$%^&*()_+" 
// 					L"QWERTYUIOP{}"
// 					L"ASDFGHJKL:\"|"
// 					L">ZXCVBNM<>?"
// 					;
// void translation(wint_t *s, int from, int to) {

// }

// // возвращает номер раскладки по имени: en или ru
// int get_group_by_name(char* name) {
// 	XkbDescRec* kb = XkbAllocKeyboard();
// 	if(!kb) return -1;

// 	kb->dpy = d;
// 	if(XkbGetNames(d, XkbGroupNamesMask, kb) != Success) return -1;

// 	Atom* gs = kb->names->groups;
// 	for(int i = 0; i < XkbNumKbdGroups && gs[i] != 0; ++i) {
// 		char* kb_name = XGetAtomName(d, gs[i]);
// 		//printf("kb_name: %s\n", kb_name);
// 		if(strncmp(kb_name, name, strlen(name)) == 0) return i;
// 	}
	
// 	XkbFreeNames(kb, XkbGroupNamesMask, 0);

// 	return -1;
// }

// /* отправляет символ в активное окно
//  * 
//  * code - номер клавиши в клавиатуре
//  * group - раскладка клавиатуры
//  * mods - какие модифицирующие клавиши нажаты
//  * is_press - клавиша нажата или отжата
//  */
// void send_key_event(int code, int group, int mods, int is_press) {
// 	XKeyEvent xk;

// 	xk.display = d;
// 	xk.subwindow = None;
// 	xk.time = CurrentTime;
// 	xk.same_screen = True;
// 	xk.send_event = True; // событие послано другим клиентом через вызов XSendEvent

// 	// Позиция курсора мыши в окне и на экране
// 	xk.x = xk.y = xk.x_root = xk.y_root = 1;

// 	xk.window = current_win;
//     xk.keycode = code;
//     xk.state = mods | (group << 13);
//     xk.type = is_press? KeyPress: KeyRelease;
//     XSendEvent(d, xk.window, True, KeyPressMask, (XEvent *)&xk);

//     XFlush(d);
// }

// // выводит подсказку с текстом
// void show_hint(wchar_t *s) {
// 	// номер основного экрана
// 	int ScreenNumber = DefaultScreen(d);

// 	// окно
// 	Window window = XCreateSimpleWindow(
// 		d,
// 		RootWindow(d, ScreenNumber),
// 		X, Y, WIDTH, HEIGHT, BORDER_WIDTH,
// 		BlackPixel(d, ScreenNumber),
// 		WhitePixel(d, ScreenNumber)
// 	);

// 	 Задаем рекомендации для менеджера окон 
// 	SetWindowManagerHints ( display, PRG_CLASS, argv, argc,
// 	window, X, Y, WIDTH, HEIGHT, WIDTH_MIN,
// 	HEIGHT_MIN, TITLE, ICON_TITLE, 0 );

// 	// Выбираем события,  которые будет обрабатывать программа */
// 	//XSelectInput ( display, window, ExposureMask | KeyPressMask);

// 	// Отображает окно
// 	XMapWindow(d, window);


// }

/*
const char* key2str(int code) {
	KeySym ks = XkbKeycodeToKeysym(d, code, 0, 0);
	return XKeysymToString(ks);
}

int mod_pressed(char* keys) {
	XModifierKeymap *mmap = XGetModifierMapping(d);
	int count_modifiers = mmap->max_keypermod;
	int keys_pressed = 0;
	int modkey_code = -1;
	for(int i=0; i<count_modifiers; i++)
	for(int j=0; j<8; j++) {
		int mod_code = mmap->modifiermap[j*count_modifiers+i];
		if(mod_code && 0!=BIT(keys, mod_code)) {
			keys_pressed++;
			modkey_code = j;
			printf("mod %s     i=%i j=%i\n", key2str(mod_code), i, j);
		}
		else {
			//printf("UNmod %s     i=%i j=%i press\n", key2str(mod_code), i, j);
		}
	}

	// Если нажата контролирующая клавиша, исключая shift, то что же вы хотите?
	if(keys_pressed>1) return 1;
	if(keys_pressed==1 && modkey_code != ShiftMapIndex) return 1;
	return 0;
} */
