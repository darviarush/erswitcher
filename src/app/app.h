#ifndef __ERSWITCHER_APP__H__
#define __ERSWITCHER_APP__H__

/*
 * Слой приложения: состояние, обёртки над портами и главный цикл
 *
 * Заголовок держит только объявления. Он используется тестами, чтобы
 * подключать модули, а не монолит через #include. Реализация — app.c
 */

#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XKB.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <xkbcommon/xkbcommon.h>

#include "../domain/unikey/unikey.h"
#include "../domain/input-buffer/input-buffer.h"
#include "../domain/text/text.h"
#include "../domain/case-invert/case-invert.h"
#include "../domain/compose/compose.h"
#include "../domain/command-template/command-template.h"
#include "../use-cases/input/input.h"
#include "../use-cases/translate/translate.h"
#include "../use-cases/config-reload/config-reload.h"
#include "../use-cases/sendkeys/sendkeys.h"
#include "../use-cases/snippets/snippets.h"
#include "../use-cases/commands/commands.h"
#include "../ports/keyboard.h"
#include "../ports/clipboard.h"
#include "../ports/scheduler.h"
#include "../ports/event-loop.h"
#include "../ports/config-store.h"
#include "../ports/command-runner.h"
#include "../ports/tray.h"
#include "../ports/logger.h"
#include "../infrastructure/x11/keyboard.h"
#include "../infrastructure/x11/clipboard.h"
#include "../infrastructure/x11/events.h"
#include "../infrastructure/x11/tray.h"
#include "../infrastructure/system/scheduler-time.h"
#include "../infrastructure/system/config-store-fs.h"
#include "../infrastructure/system/command-runner-fork.h"
#include "../infrastructure/system/desktop.h"
#include "../presentation/tray-indicator.h"

#define DELAY 10000
#define USEC_TO_DBL(U) (U / 1000000.0)
#define BUF_SIZE INPUT_BUFFER_CAPACITY

#define BIT(VECTOR, BIT_IDX) (((char*) VECTOR)[BIT_IDX / 8] & (1 << (BIT_IDX % 8)))

#define KEY_TO_SYM(K) unikey_to_keysym(K)
#define KEY_TO_INT(K) xkb_keysym_to_utf32(unikey_to_keysym(K))
#define SYM_TO_KEY(K) keysym_to_unikey(K)
#define SYM_TO_INT(S) xkb_keysym_to_utf32(S)
#define INT_TO_SYM(S) xkb_utf32_to_keysym(S)
#define INT_TO_KEY(K) keysym_to_unikey(xkb_utf32_to_keysym(K))

#define SYM_TO_STR(K) XKeysymToString(K)
#define KEY_TO_STR(K) XKeysymToString(KEY_TO_SYM(K))
#define INT_TO_STR(K) XKeysymToString(INT_TO_SYM(K))

#define STR_TO_SYM(K) XStringToKeysym(K)
#define STR_TO_INT(K) SYM_TO_INT(XStringToKeysym(K))
#define STR_TO_KEY(K) SYM_TO_KEY(XStringToKeysym(K))

#define EQ(S1, S2) (strcmp(S1, S2) == 0)

#define AltMask Mod1Mask
#define NumMask Mod2Mask
#define MetaMask Mod3Mask
#define SuperMask Mod4Mask
#define HyperMask Mod5Mask

extern input_buffer_t input;
extern int keys_pressed;
extern int loop_delay;
extern pid_t config_window_pid;

extern Display* d;
extern Window w;
extern Window current_win;

extern void (*on_copy_selection)(int, int);
extern char* insert_from_clipboard_data;
extern char* insert_from_clipboard_save_data;
extern int insert_from_clipboard_save_len;
extern Atom insert_from_clipboard_save_target;

extern config_t current_config;
extern int keyfn_active;

#define keyfn current_config.bindings
#define keyfn_size current_config.bindings_size
#define compose_map current_config.compose
#define compose_map_size current_config.compose_size

extern keyboard_port_t kbd_port;
extern int xerror;

extern const char* curcommand_infile;
extern const char* curcommand_outfile;
extern float command_timeout;
extern float command_interval;
extern int cursor_back;
extern volatile sig_atomic_t load_config_requested;

void keyboard_port_ensure(void);
void init_keyboard(void);
KeySym unikey_to_keysym(unikey_t key);
unikey_t keyboard_state(int code);
unikey_t keysym_to_unikey(KeySym ks);
void set_group(int group);
void print_sym(int mods, KeySym ks);
int in_sym(KeySym symbol, KeySym* symbols);

void open_display(void);
Window get_current_window(void);
unsigned int get_input_state(void);

void send_key(unikey_t key, int is_press);
void press_key(unikey_t key);
void clear_active_mods(void);
void recover_active_mods(void);
void add_to_buffer(unikey_t key);
int from_space(void);
void press_key_multi(unikey_t key, int n);
void sendkeys(char* s);
void print_translate_buffer(int from, int backspace);
void print_invertcase_buffer(int from, int backspace);
char* get_event_type(int n);
void to_buffer(const char* s);
void shift_leftArrow(void);
void control_insert(void);
void copy_selection_next(void* context, const char* s, int len,
                         clipboard_format_t format);
void copy_selection(void (*fn)(int, int));
void clipboard_free(void);
void event_delay(double seconds);
void change_key(int code);
void insert_from_clipboard(const char* s);
int is_blank(char* s);
char* command_fmt2ptr(const char* clipboard, const char* command);
void run_command(char* s);
void cursor_back__fn(void* context);
void compose(char*);
void word_translate(char*);
void text_translate(char*);
void selection_translate(char*);
void word_invertcase(char*);
void text_invertcase(char*);
void selection_invertcase(char*);
void signal_load_config(int);
void load_config(void);
void app_run(int ac, char** av);

/* Сброс состояния сценариев команд между тестами */
void app_reset_runtime(void);

#endif /* __ERSWITCHER_APP__H__ */
