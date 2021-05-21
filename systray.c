#include <X11/Xutil.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>


#define MIN(A, B)               ((A) < (B) ? (A) : (B))

/* --------- XEMBED and systray stuff */
#define SYSTEM_TRAY_REQUEST_DOCK   0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

static int trapped_error_code = 0;
static int (*old_error_handler) (Display *, XErrorEvent *);

static int
error_handler(Display     *display, XErrorEvent *error) {
    trapped_error_code = error->error_code;
    return 0;
}

void
trap_errors(void) {
    trapped_error_code = 0;
    old_error_handler = XSetErrorHandler(error_handler);
}

int
untrap_errors(void) {
    XSetErrorHandler(old_error_handler);
    return trapped_error_code;
}

void
send_systray_message(Display* display, long message, long data1, long data2, long data3) {
    XEvent ev;

    Atom selection_atom = XInternAtom (display,"_NET_SYSTEM_TRAY_S0", False);
    Window tray = XGetSelectionOwner (display, selection_atom);

    if ( tray != None)
        XSelectInput (display, tray, StructureNotifyMask);

    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = tray;
    ev.xclient.message_type = XInternAtom (display, "_NET_SYSTEM_TRAY_OPCODE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = message;
    ev.xclient.data.l[2] = data1; // <--- your window is only here
    ev.xclient.data.l[3] = data2;
    ev.xclient.data.l[4] = data3;

    trap_errors();
    XSendEvent(display, tray, False, NoEventMask, &ev);
    XSync(display, False);
    usleep(10000);
    if (untrap_errors()) {
        /* Handle errors */
    }
}
/* ------------ Regular X stuff */
int
main(int argc, char **argv) {
    int width, height;
    XWindowAttributes wa;
    XEvent ev;
    Display *display;
    int screen;
    Window root, win;

    /* init */
    if (!(display=XOpenDisplay(NULL)))
        return 1;
    screen = DefaultScreen(display);
    root = RootWindow(display, screen);
    if(!XGetWindowAttributes(display, root, &wa))
        return 1;
    width = height = MIN(wa.width, wa.height);

    /* create window */
    win = XCreateSimpleWindow(display, root, 0, 0, width, height, 0, 0, 0xFFFF5656);

    // устанавливаем окно в системный лоток
    send_systray_message(display, SYSTEM_TRAY_REQUEST_DOCK, win, 0, 0);
    XMapWindow(display, win);

    XSelectInput(display, win, ButtonPressMask|ExposureMask);

    XSync(display, False);

    /* run */
    while(1) {
        while(XPending(display)) {
            XNextEvent(display, &ev); /* just waiting until we error because window closed */
            switch(ev.type)
            {
            case Expose:

            case ButtonPress:
                exit(0);
            }
        }
    }
}