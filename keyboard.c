/**************************************************************
 * Приложение: keyboard - печатает сканкоды                   *
 * Автор: Ярослав О. Косьмина                                 *
 * Лицензия: GPLv3                                            *
 * Местонахождение: https://github.com/darviarush/erswitcher  *
 **************************************************************/

#include <X11/X.h>
#include <X11/XKBlib.h>

// Установлен ли бит
#define BIT(VECTOR, BIT_IDX)   ( ((char*)VECTOR)[BIT_IDX/8]&(1<<(BIT_IDX%8)) )
// размер клавиатуры в битах
#define KEYBOARD_SIZE   (32*8)

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

int main() {

	Display *d = XOpenDisplay(NULL);
	if(!d) { fprintf(stderr, "Not open display!\n"); return 1; }

	XSetErrorHandler(null_X_error);

	XSynchronize(d, True);

	// инициализируем раскладки клавиатуры
	XkbDescRec* kb = XkbAllocKeyboard();
	if(!kb) {
		fprintf(stderr, "XkbAllocKeyboard не выделила память\n");
		return 1;
	}

	kb->dpy = d;
	if(XkbGetNames(d, XkbGroupNamesMask, kb) != Success) {
		fprintf(stderr, "XkbGetNames свалился\n");
		return 1;
	}
	
	

	Atom* gs = kb->names->groups;
	// groups < XkbNumKbdGroups &&
		
	for(int k=0; k<KEYBOARD_SIZE; k++) {
		
		printf("%i;", k);
		
		for(int group = 0; gs[groups] != 0; group++) {
			char* kb_name = XGetAtomName(d, gs[group]);
			
			KeySym ks = XkbKeycodeToKeysym(d, k, group, 0);
			KeySym kss = XkbKeycodeToKeysym(d, k, group, 1);
		
			printf("%s;%s;%s", kb_name, XKeysymToString(ks), XKeysymToString(kss));
			XFree(kb_name);
		}
		
		printf("\n");
	}
		
		
	
	
	
	return 0;
}
