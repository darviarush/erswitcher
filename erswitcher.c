#include <locale.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wctype.h>


#define BIT(c, x)   ( c[x/8]&(1<<(x%8)) )
#define TRUE    1
#define FALSE   0

int mod_pressed(char* keys);
void printState(XkbStateRec* state);

static int xerror = 0;
static int *null_X_error (Display *d, XErrorEvent *e) {
	xerror = 1;
	fprintf(stderr, "x error!\n");
}


Display *d;
Window current_win;
#define SIZE	1024
wchar_t word[SIZE];
int pos;


const char* 
key2str(int code) {
	KeySym ks = XkbKeycodeToKeysym(d, code, 0, 0);
	return XKeysymToString(ks);
}

void
change_key(char* keys, int code) {
	printf("change_key! code=%d\n", code); fflush(stdout);


	// нажата функциональная клавиша и это не shift
	// if(mod_pressed(keys)) return;

	XkbStateRec state;
	memset(&state, 0, sizeof(state));
	XkbGetState(d, XkbUseCoreKbd, &state);
	printState(&state);

	int shiftLevel = ((state.mods & (ShiftMask | LockMask)) 
		&& !(state.mods & ShiftMask && state.mods & LockMask))? 1: 0;
	printf("shiftLevel=%i\n", shiftLevel);
	// ks на самом деле - символ юникода (wchar_t)
	KeySym ks = XkbKeycodeToKeysym(d, code, state.group, shiftLevel);

	const char *s1 = XKeysymToString(ks);
	//if(strlen(s1) != 1) return;

	// Если сменилось окно, то начинаем ввод с начала
	Window w = 0;
  	int revert_to = 0;
	XGetInputFocus(d, &w, &revert_to);

	if(w != current_win) {
		pos = 0;
		current_win = w;
	}

	printf ("0x%04x: %s\n", (unsigned int) ks, s1);
	printf ("wide char (%C)\n", (wchar_t) ks);

	wchar_t cs = (wchar_t) ks;

	if(iswspace(cs)) {
		pos = 0;
		return;
	}

	if(!iswprint(cs)) return;

	// Записываем символ в буфер
	if(pos >= SIZE) pos = 0;
	word[pos++] = cs;
	printf("s=%.*S\n", pos, word);	fflush(stdout);
}


void main(int ac, char** av) {
	setlocale(LC_ALL, "ru_RU.UTF-8");
	// wchar_t c = L'Л';
	// printf("test(%C) = %i\n", c, iswprint(c));
	// exit(0);

	int delay = 10000;

	char* display = XDisplayName(NULL);

	printf("display: %s\n", display);

	d = XOpenDisplay(display);
	if(!d) { fprintf(stderr, "Not open display!\n"); exit(1); }

	XSetErrorHandler((XErrorHandler) null_X_error);

	XSynchronize(d, TRUE);	

	int revert_to = 0;
	XGetInputFocus(d, &current_win, &revert_to);
	pos = 0;

	char buf1[32], buf2[32];
	char *saved=buf1, *keys=buf2;
   	XQueryKeymap(d, saved);

   	while (1) {
   		XQueryKeymap(d, keys);
      	for(int i=0; i<32*8; i++) {
      		if(BIT(keys, i)!=0 && BIT(keys, i)!=BIT(saved, i)) change_key(keys, i);
      	}

      	char* char_ptr=saved;
      	saved=keys;
      	keys=char_ptr;

      	usleep(delay);
   	}
}


void
printState(XkbStateRec* state) {
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
}


// Переключение раскладки
// group - номер раскладки (обычно порядковый номер 
//         в списке включенных раскладок, счет с 0
int setActiveGroup(int group) {
    // Отправка запроса на переключение раскладки
    XkbLockGroup(d, XkbUseCoreKbd, group);

    // Вызов XkbGetState() для выполнения запроса,
    // без этого вызова переключение раскладки не сработает
    XkbStateRec state;
    memset(&state, 0, sizeof(state));
    XkbGetState(d, XkbUseCoreKbd, &state);

    return 0;
}
