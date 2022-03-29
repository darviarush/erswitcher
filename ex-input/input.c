#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>



Display *d;
Window w;

char* get_event_type(int n) {
    if(n < 2 || n > LASTEvent) return NULL;

    char* names[] = {NULL, NULL,
                     "KeyPress",
                     "KeyRelease",
                     "ButtonPress",
                     "ButtonRelease",
                     "MotionNotify",
                     "EnterNotify",
                     "LeaveNotify",
                     "FocusIn",
                     "FocusOut",
                     "KeymapNotify",
                     "Expose",
                     "GraphicsExpose",
                     "NoExpose",
                     "VisibilityNotify",
                     "CreateNotify",
                     "DestroyNotify",
                     "UnmapNotify",
                     "MapNotify",
                     "MapRequest",
                     "ReparentNotify",
                     "ConfigureNotify",
                     "ConfigureRequest",
                     "GravityNotify",
                     "ResizeRequest",
                     "CirculateNotify",
                     "CirculateRequest",
                     "PropertyNotify",
                     "SelectionClear",
                     "SelectionRequest",
                     "SelectionNotify",
                     "ColormapNotify",
                     "ClientMessage",
                     "MappingNotify",
                     "GenericEvent",
                     "LASTEvent",
                    };

    return names[n];
}

void event_next() {
    XEvent event;
    XNextEvent(d, &event);

    fprintf(stderr, "[%i %s], ", event.type, get_event_type(event.type));
	
	if(event.type == Expose) {
		
		fprintf(stderr, "[%li ]",
			ev.xexpose.send_event,
			ev.xexpose.display,
			ev.xexpose.window,
			ev.xexpose.x,
			ev.xexpose.y,
			ev.xexpose.width,
			ev.xexpose.height,
			ev.xexpose.count
		);
	}

}

int main() {

	
	int black, white;
	GC gc;

	d = XOpenDisplay(NULL);
	/* 
	 * Аргументом XOpenDisplay является имя дисплея, при
	 * использовании NULL в качестве аргумента значение берется из
	 * $DISPLAY
	 */

	 black = BlackPixel(d, DefaultScreen(d));
	 white = WhitePixel(d, DefaultScreen(d));

	 w = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0,
	 				640, 480, 0, black, white);
	/* man XCreateSimpleWindow */

	XSelectInput(d, w, 
	KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask | PointerMotionMask | PointerMotionHintMask | Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask | ButtonMotionMask | KeymapStateMask | ExposureMask | VisibilityChangeMask | StructureNotifyMask | ResizeRedirectMask | SubstructureNotifyMask | SubstructureRedirectMask | FocusChangeMask | PropertyChangeMask | ColormapChangeMask | OwnerGrabButtonMask
	);
// [2 KeyPress]
// [10 FocusOut], [15 VisibilityNotify], [28 PropertyNotify], [32 ColormapNotify], [12 Expose],
// [32 ColormapNotify], [15 VisibilityNotify], [12 Expose], [9 FocusIn], [11 KeymapNotify], [32 ColormapNotify], [28 PropertyNotify],

	XMapWindow(d, w);
	/* Окошко нужно не только создать, но и вывести на экран */

	gc = XCreateGC(d, w, 0, NULL);
	/* Создадим графический контекст. man XCreateGC */

	XSetForeground(d, gc, black);

	/* Ошибка 1. Попробуйте понять, что пропущено? */

	XDrawLine(d, w, gc, 1, 1, 199, 199);

	/* Ошибка 2. Попробуйте понять, что пропущено? */

	while (1) {
		usleep(10000);
		while(XPending(d)) event_next();
	}

	return 0; /* unreachable */
}
