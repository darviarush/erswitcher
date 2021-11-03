/*****************************************************************
 * Приложение: keyboard - печатает сканкоды раскладок клавиатуры *
 * Автор: Ярослав О. Косьмина                                    *
 * Лицензия: GPLv3                                               *
 * Местонахождение: https://github.com/darviarush/erswitcher     *
 *****************************************************************/

#include <X11/X.h>
#include <X11/XKBlib.h>
#include <xkbcommon/xkbcommon.h>
#include <locale.h>
#include <stdio.h>
#include <wchar.h>

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

	char* locale = "ru_RU.UTF-8";
	if(!setlocale(LC_ALL, locale)) {
		fprintf(stderr, "setlocale(LC_ALL, \"%s\") failed!\n", locale);
        return 1;
	}

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
	// group < XkbNumKbdGroups &&
		
	for(int k=0; k<KEYBOARD_SIZE; k++) {
		
		printf("%i", k);
		
		for(int group = 0; gs[group] != 0; group++) {
			char* kb_name = XGetAtomName(d, gs[group]);

			printf(";%i;%s", group, kb_name);

			for(int shift = 0; shift <= 1; shift++) {
				KeySym ks = XkbKeycodeToKeysym(d, k, group, shift);
		
				wint_t cs = xkb_keysym_to_utf32(ks);
				printf(";%c;%s;%i;%C",
					shift? '^': '_',
					XKeysymToString(ks), 
					cs,
					cs == L'\n' || cs == L'\r' || cs == L';' || cs == L'"'? L'-': cs);
			}
			XFree(kb_name);
		}
		
		printf("\n");
	}
	
	return 0;
}
