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


#define BIT(c, x)   ( c[x/8]&(1<<(x%8)) )
#define TRUE    1
#define FALSE   0

// Объявления функций
//int mod_pressed(char* keys);
void print_state(XkbStateRec* state);
Window get_current_window();
void set_active_group(int group);

static int xerror = 0;
static int *null_X_error (Display *d, XErrorEvent *e) {
	xerror = 1;
	fprintf(stderr, "x error!\n");
}


Display *d;					// текущий дисплей
Window current_win;			// окно устанавливается при вводе с клавиатуры
useconds_t delay = 12000;	// интервал проверки клавиатуры и ввода символов
#define SIZE	1024
wint_t word[SIZE];
wint_t backspace[SIZE];
int pos = 0;


void change_key(char* keys, int code) {
	printf("change_key! code=%d\n", code); fflush(stdout);


	// нажата функциональная клавиша и это не shift
	// if(mod_pressed(keys)) return;

	XkbStateRec state;
	XkbGetState(d, XkbUseCoreKbd, &state);
	print_state(&state);

	int shiftLevel = ((state.mods & (ShiftMask | LockMask)) 
		&& !(state.mods & ShiftMask && state.mods & LockMask))? 1: 0;
	printf("shiftLevel=%i\n", shiftLevel);
	// ks на самом деле - символ юникода (wchar_t)
	KeySym ks = XkbKeycodeToKeysym(d, code, state.group, shiftLevel);
	//if(ks == NoSymbol) return;

	const char *s1 = XKeysymToString(ks);
	//if(strlen(s1) != 1) return;

	// Если сменилось окно, то начинаем ввод с начала
	Window w = get_current_window();
	if(w != current_win) {
		pos = 0;
		current_win = w;
	}

	wint_t cs = xkb_keysym_to_utf32(ks);

	printf ("0x%04llx -> 0x%04lx: %s `%C`\n", ks, cs, s1, cs);

	// BackSpace - удаляем последний символ
	if(cs == 8) {
		if(pos != 0) --pos;
		return;
	}

	// нажата Pause - переводим
	if(ks == 0xff13) {
		// Если нечего переводить, то показываем сообщение на месте ввода
		if(pos == 0) { return; }
		// отправляем бекспейсы, чтобы удалить ввод
		for(int i=0; i<pos; i++) backspace[i] = 8;
		backspace[pos] = 0;
		type(backspace);
		// меняем раскладку
		set_active_group(? : );
		// вводим ввод, но в альтернативной раскладке
		type(word);
		return;
	}

	//if(cs == 0) { pos = 0; return; } // разные управляющие клавиши
	//if(iswspace(cs)) { pos = 0;	return;	}
	// Если символ не печатный, то начинаем ввод с начала
	if(!iswprint(cs)) {	pos = 0; return; }

	// Записываем символ в буфер
	if(pos >= SIZE) pos = 0;
	word[pos++] = cs;
	word[pos] = 0;
	printf("len=%i s=%S\n", pos, word);	fflush(stdout);
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

	XSynchronize(d, TRUE);	

	// Начальные установки
	current_win = get_current_window();
	pos = 0;

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

      	usleep(delay);
   	}
}

// Переключение раскладки
// group - номер раскладки (обычно порядковый номер 
//         в списке включенных раскладок, счет с 0
void set_active_group(int group) {
    // Отправка запроса на переключение раскладки
    XkbLockGroup(d, XkbUseCoreKbd, group);

    // Вызов `XkbGetState()` (получение состояния клавиатуры) для выполнения запроса, без этого вызова переключение раскладки не сработает
    XkbStateRec state;
    XkbGetState(d, XkbUseCoreKbd, &state);
}

// возвращает текущее окно
Window get_current_window() {
	Window w = 0;
  	int revert_to = 0;
	XGetInputFocus(d, &w, &revert_to);
	return w;
}

// транслитерирует текст с русской раскладки в английскую
static wint_t *ru = L"ё1234567890-="
					 L"йцукенгшщзхъ"
					 L"фывапролджэ\\"
					 L"/ячсмитьбю."
					 ;
static wint_t *en = L"`1234567890-=" 
					 L"qwertyuiop[]"
					 L"asdfghjkl;'\\"
					 L"<zxcvbnm,./"
					 ;
void translation(wint_t *s) {

}

// ввод с клавиатуры строки в юникоде
void type(wint_t *s) {

}

// отправляет символ
void send_key(wint_t cs) {
	
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