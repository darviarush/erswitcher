#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <xkbcommon/xkbcommon.h>
#include <locale.h>
#include <stdio.h>


int main() {
	char* locale = "ru_RU.UTF-8";
	if(!setlocale(LC_ALL, locale)) {
		fprintf(stderr, "setlocale(LC_ALL, \"%s\") failed!\n", locale);
        return 1;
	}
	
	xkb_keysym_t n = (xkb_keysym_t) -1;
	for(xkb_keysym_t ks=0; ks<n; ks++) {
		char* s = XKeysymToString(ks);
		if(s) {
			uint32_t cs = xkb_keysym_to_utf32(ks);
			//printf("%u\t%s\t%C\t%s\n", ks, s, cs, cs==0? "---": "");
			if(!cs) printf("%u\t%s\n", ks, s);
		}
	}
	return 0;
}
