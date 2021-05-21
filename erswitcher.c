#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>

#define SWAP(A, B)  { typeof(A) n=A; B=A; A=n; }

Display *d;

static int xerror = 0;
static int *null_X_error (Display *d, XErrorEvent *e) {
	xerror = 1;
	fprintf(stderr, "x error!\n");
}

change_key() {
	Window w = NULL;
  	int revert_to = 0;
	XGetInputFocus(d, &w, &revert_to);

	
}


void main() {
	int delay = 10000;

	char* display = XDisplayName(NULL);

	printf("display: %s\n", display);

	d = XOpenDisplay(display);
	if(!d) { fprintf(stderr, "Not open display!\n"); exit(1); }

	XSetErrorHandler((XErrorHandler) null_X_error);

	XSynchronize(d, TRUE);	

	char buf1[32], buf2[32];
	char *saved=buf1, keys=buf2;
   	XQueryKeymap(disp, saved);

   	while (1) {
   		XQueryKeymap(disp, keys);
      	for (i=0; i<32*8; i++) {
      		if(BIT(keys, i)!=BIT(saved, i)) change_key()
      	}

      	SWAP(saved, keys);
      	usleep(delay);
   	}
}
