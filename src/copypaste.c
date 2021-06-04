#include <X11/Xatom.h>
#include <wchar.h>
#include <stdlib.h>

#include "erswitcher.h"
#include "copypaste.h"


// static unsigned char *
// get_selection_text (Atom selection)
// {
//   unsigned char * retval;

//   if ((retval = get_selection (selection, utf8_atom)) == NULL)
//     retval = get_selection (selection, XA_STRING);

//   return retval;
// }

int utf8len(char* s) {
	int len = 0;
	while (*s) len += (*s++ & 0xc0) != 0x80;
	return len;
}

void copypaste() {
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
	Atom selection = XA_PRIMARY;

	XConvertSelection(d, selection,
		request_target, 
		sel_data_atom, 
		w,
	    CurrentTime
	);
	XSync(d, False);
	
	// строка которую получим
	unsigned char* s = NULL;
	int format;	// в этом формате
	unsigned long bytesafter, length;
	Atom target;

	// получаем событие
	while(1) {
		XEvent event;
		XNextEvent(d, &event);

		// пришло какое-то другое событие... ну его
		if(event.type != SelectionNotify) continue;
		if(event.xselection.selection != selection) continue;

		// нет выделения
		if(event.xselection.property == None) break;

		// считываем
		XGetWindowProperty(event.xselection.display,
			    event.xselection.requestor,
			    event.xselection.property, 0L, 1000000,
			    False, (Atom) AnyPropertyType, &target,
			    &format, &length, &bytesafter, &s);

		break;
	}

	XDestroyWindow(d, w);

	if(s == NULL) return;

	printf("len: %i x: %s\n", length, s);

	// utf8 переводим в символы юникода, затем в символы x11, после - в сканкоды
	mbstate_t mbs = {0};
	for(size_t charlen, i = 0; (charlen = mbrlen(s+i, MB_CUR_MAX, &mbs)) != 0 && charlen >= 0; i += charlen) {
        wchar_t ws[4];
        int res = mbstowcs(ws, s+i, 1);
        if(res != 1) break;


    }

	XFree(s);
}
