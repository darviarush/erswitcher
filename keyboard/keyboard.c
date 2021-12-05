/*****************************************************************
 * Приложение: keyboard - печатает сканкоды раскладок клавиатуры *
 * Автор: Ярослав О. Косьмина                                    *
 * Лицензия: GPLv3                                               *
 * Местонахождение: https://github.com/darviarush/erswitcher     *
 *****************************************************************/

#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <xkbcommon/xkbcommon.h>
#include <locale.h>
#include <stdio.h>

// размер клавиатуры в битах
#define KEYBOARD_SIZE   (32*8)

// переводит некоторые символы в символы с их отображением
int sign(xkb_keysym_t ks) {
	uint32_t cs = xkb_keysym_to_utf32(ks);
	if(cs <= 0x1F) cs += 0x2400;
	
	switch(ks) {
		case XK_Escape: cs = L'⎋'; break;
		case XK_BackSpace: cs = L'↤'; break; // ⌫⌧⌨⌬
		case XK_Delete: cs = L'⌦'; break;
		case XK_KP_Delete: cs = L'⌦'; break;
		case XK_Return: cs = L'↩'; break;
		case XK_KP_Enter: cs = L'↵'; break;
		case XK_Tab: cs = L'↹'; break;
        case XK_ISO_Left_Tab: cs = L'⭿'; break;
		case XK_Home: cs = L'⇱'; break;
		case XK_KP_Home: cs = L'⇱'; break;
		case XK_End: cs = L'⇲'; break;
		case XK_KP_End: cs = L'⇲'; break;
		case XK_Menu: cs = L'≣'; break;
        
		case XK_Print: cs = L'⎙'; break;
		case XK_Sys_Req: cs = L'☈'; break; // ⇼⤄⇌↹⍐⇯⇮⇯
		case XK_Scroll_Lock: cs = L'⤓'; break; // ↨⇵⥮⥯⬍⇳⇕
		case XK_Pause: cs = L'⎉'; break;
		case XK_Break: cs = L'⎊'; break;      // ⌤ ⎀⎁⎃⎅⎆⎌
		
		case XK_Multi_key: cs = L'⎄'; break; // клавиша compose

		case XK_Up: cs = L'↑'; break;
		case XK_KP_Up: cs = L'🢕'; break; // 🢔🢖🢕🢗
		
		case XK_Down: cs = L'↓'; break;
		case XK_KP_Down: cs = L'🢗'; break;
		
		case XK_Left: cs = L'←'; break;
		case XK_KP_Left: cs = L'🢔'; break;
		
		case XK_Right: cs = L'→'; break;
		case XK_KP_Right: cs = L'🢖'; break;
		
		case XK_Shift_L: cs = L'⇧'; break;
		case XK_Shift_R: cs = L'⇧'; break;
		case XK_Caps_Lock: cs = L'⇪'; break;// ⇪🄰🄰
		case XK_Shift_Lock: cs = L'🄰'; break;// ⇪🄰🄰
 		case XK_ISO_Next_Group: cs = L'🌍'; break; // переключение раскладки / 🗺🌎🌏🌍 🗾-Япония
        case XK_Mode_switch: cs = L'🗺'; break; // переключение раскладки, пока клавиша нажата
		case XK_Control_L: cs = L'⌃'; break; // ˅
		case XK_Control_R: cs = L'⎈'; break;
		case XK_Alt_L: cs = L'⌥'; break;
		case XK_Alt_R: cs = L'⎇'; break;
		case XK_Num_Lock: cs = L'⇭'; break; // ⓛ🄸
		case XK_Meta_L: cs = L'⌘'; break;
		case XK_Meta_R: cs = L'⌘'; break;
		case XK_Super_L: cs = L'❖'; break; // ⊞
		case XK_Super_R: cs = L'❖'; break;
		case XK_Hyper_L: cs = L'✦'; break;
		case XK_Hyper_R: cs = L'✦'; break;
	}
	
	switch(cs) {
		//case L'"': cs = L'”'; break;
		case L' ': cs = L'␣'; break;
	}
	
	return cs;
}


// обработка ошибок
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
		
	for(int k=0; k<KEYBOARD_SIZE; k++) {
		
		printf("%i", k);
		
		for(int group = 0; gs[group] != 0; group++) {
			char* kb_name = XGetAtomName(d, gs[group]);

			printf("\t%i\t%s", group, kb_name);

			for(int shift = 0; shift <= 1; shift++) {
				KeySym ks = XkbKeycodeToKeysym(d, k, group, shift);
				uint32_t cs = xkb_keysym_to_utf32(ks);
				
				printf("\t%s\t%s\t%C\t%i",
					shift? "SHIFT": "shift",
					XKeysymToString(ks),
					sign(ks),
					cs);
			}
			XFree(kb_name);
		}
		
		printf("\n");
	}
	
	return 0;
}
