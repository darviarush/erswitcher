#include <locale.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wctype.h>
#include <xkbcommon/xkbcommon.h>

#include "keyboard.h"


#define BIT(c, x)   ( c[x/8]&(1<<(x%8)) )

// Объявления функций
void print_state(XkbStateRec* state);
Window get_current_window();

// Обработка ошибок
int xerror = 0;
int *null_X_error (Display *d, XErrorEvent *err) {
	xerror = 1;
	#define BUFLEN		(1024*64)
	char buf[BUFLEN];
    XGetErrorText(d, err->error_code, buf, BUFLEN);
	fprintf(stderr, "x error! %s\n", buf);
}


Display *d;					// текущий дисплей
Window current_win;			// окно устанавливается при вводе с клавиатуры

#define SIZE	1024
wint_t word_buf[SIZE];
wint_t *word = word_buf;
wint_t trans_buf[SIZE];
wint_t *trans = trans_buf;
int pos = 0;


void change_key(char* keys, int code) {
	printf("change_key! code=%d\n", code); fflush(stdout);

	XkbStateRec state;
	XkbGetState(d, XkbUseCoreKbd, &state);
	//print_state(&state);

	// Если нажаты какие-то ещё модификаторы, контрол или альт - выходим
	if(state.mods & ~(ShiftMask|LockMask)) {
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
	wint_t cs_pause = xkb_keysym_to_utf32(ks_pause);

	printf ("0x%04llx -> 0x%04lx: %s `%C` en: %s\n", ks, cs, s1, cs, s_pause);

	// BackSpace - удаляем последний символ
	if(ks_pause == 0xff08) {
		if(pos != 0) --pos;
		word[pos] = trans[pos] = 0;
		printf("backspace: %S -> %S\n", word, trans);
		return;
	}

	// нажата Pause - переводим
	if(ks_pause == 0xff13) {

		word[pos] = trans[pos] = 0;
		printf("pause! %S -> %S\n", word, trans);

		// Если нечего переводить, то показываем сообщение на месте ввода
		if(pos == 0) {
			//...
		}

		// отправляем бекспейсы, чтобы удалить ввод
		for(int i=0; i<pos; i++) {
			press_key(8);
		}		

		// вводим ввод, но в альтернативной раскладке
		for(int i=0; i<pos; i++) {
			press_key(trans[i]);
		}

		// меняем буфера местами
		wint_t *x = trans;
		trans = word;
		word = x;

		// сбрасываем shift, чтобы она не осталась зажата
		set_mods(state.mods);

		// меняем раскладку
		set_group(trans_group);
		return;
	}

	//if(cs == 0) { pos = 0; return; } // разные управляющие клавиши
	//if(iswspace(cs)) { pos = 0;	return;	}
	// Если символ не печатный, то начинаем ввод с начала
	if(!iswprint(cs)) {	
		//pos = 0; 
		printf("not isprint! `%C`\n", cs);
		return; 
	}

	// Записываем символ в буфер с его раскладкой клавиатуры
	if(pos >= SIZE) pos = 0;
	trans[pos] = cs_trans;
	word[pos++] = cs;
	trans[pos] = word[pos] = 0;
	printf("press! %S -> %S\n", word, trans);
}


void main(int ac, char** av) {
	setlocale(LC_ALL, "ru_RU.UTF-8");
	// wchar_t c = L'Л';
	// printf("test(%C) = %i\n", c, iswprint(c));
	// exit(0);

	char* display = XDisplayName(NULL);

	printf("display: %s\n", display);
	printf("size KeySym: %i\n", sizeof(KeySym));
	printf("size wchar_t: %i\n", sizeof(wchar_t));
	printf("size wint_t: %i\n", sizeof(wint_t));
	printf("size *S: %i\n", sizeof(*(L"Л")));

	d = XOpenDisplay(display);
	if(!d) { fprintf(stderr, "Not open display!\n"); exit(1); }

	XSetErrorHandler((XErrorHandler) null_X_error);

	XSynchronize(d, True);	

	// Начальные установки
	current_win = get_current_window();
	pos = 0;
	keysym_init();

	char buf1[32], buf2[32];
	char *saved=buf1, *keys=buf2;
   	XQueryKeymap(d, saved);

   	while (1) {
   		XQueryKeymap(d, keys);
      	for(int i=0; i<32*8; i++) {
      		if(BIT(keys, i)!=BIT(saved, i)) {
      			if(BIT(keys, i)!=0) { // клавиша нажата
      				change_key(keys, i);
      			} else {	// клавиша отжата

      			}
      		} 
      	}

      	char* char_ptr=saved;
      	saved=keys;
      	keys=char_ptr;

      	usleep(DELAY);
   	}
}


// возвращает текущее окно
Window get_current_window() {
	Window w = 0;
  	int revert_to = 0;
	XGetInputFocus(d, &w, &revert_to);
	return w;
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