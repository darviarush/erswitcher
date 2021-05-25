#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>

#define SWAP(A, B)  { typeof(A) n=A; B=A; A=n; }

static int xerror = 0;
static int *null_X_error (Display *d, XErrorEvent *e) {
	xerror = 1;
	fprintf(stderr, "x error!\n");
}


Display *d;
Window current_win;
#define SIZE	1024
char word[SIZE];
int pos;

change_key(char* keys, int code) {
	// Если нажата контролирующая клавиша, исключая shift, то что же вы хотите?
	XModifierKeymap *mmap = XGetModifierMapping(d);
	// Нажато несколько специальных клавиш
	if(mmap->max_keypermod > 1) return;

	// Нажата одна и не shift
	if(mmap->max_keypermod == 1) {
		int code = mmap->modifiermap[ShiftMapIndex];
		if(code == 0) return;
		if(BIT(keys, code) == 0) return;
	}


	// Если сменилось окно, то начинаем ввод с начала
	Window w = NULL;
  	int revert_to = 0;
	XGetInputFocus(d, &w, &revert_to);

	if(w != current_win) {
		pos = 0;
		current_win = w;
	}

	// Получаем символ
	KeySym keysym = XKeycodeToKeysym(d, code, index);
	if(NoSymbol==keysym) return;

	// Записываем символ в буфер
	word[pos++] = ;
}


void main(int ac, char** av) {
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
	char *saved=buf1, keys=buf2;
   	XQueryKeymap(disp, saved);

   	while (1) {
   		XQueryKeymap(disp, keys);
      	for(int i=0; i<32*8; i++) {
      		if(BIT(keys, i)!=BIT(saved, i)) change_key(keys, i);
      	}

      	SWAP(saved, keys);
      	usleep(delay);
   	}
}
