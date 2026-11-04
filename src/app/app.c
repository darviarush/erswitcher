/**************************************************************
 * Приложение: erswitcher - переключатель клавиатурного ввода *
 * Автор: Ярослав О. Косьмина                                 *
 * Лицензия: GPLv3                                            *
 * Местонахождение: https://github.com/darviarush/erswitcher  *
 **************************************************************/

#define _GNU_SOURCE

#include "app.h"


input_buffer_t input; // буфер ввода: word[], symbol[], pos
int keys_pressed; // нажато клавишь


int loop_delay = DELAY; // задержка между опросами клавиатуры
pid_t config_window_pid = 0; // pid конфигуратора

//@category Иксы

Display* d; // текущий дисплей
Window w; // окно этого приложения
Window current_win; // окно устанавливается при вводе с клавиатуры

void (*on_copy_selection)(int, int) = NULL; // обработчик завершения копирования буфера обмена в буфер ввода

// для шагов ввода через буфер
char* insert_from_clipboard_data; // что нужно ввести (compose)
char* insert_from_clipboard_save_data; // что было в буфере обмена
int insert_from_clipboard_save_len;
Atom insert_from_clipboard_save_target;


//@category Комбинации -> Функции

// Снимок конфигурации. Привязки хранят действие-enum, а не function
// pointer; диспетчеризация — в execute_config_action()
config_t current_config;
int keyfn_active = -1;

static config_store_port_t config_store_state;

static void config_store_ensure(void) {
    if(!config_store_state.read) config_store_fs_port(&config_store_state, &er_config_store);
}

static void execute_config_action(const config_binding_t* binding);


//@category Раскладки

// Обёртки над портом клавиатуры: старые имена остаются для монолита и
// тестов, реализация Xkb/XTest живёт в infrastructure/x11/keyboard.c
keyboard_port_t kbd_port;

void keyboard_port_ensure(void) {
    if(!kbd_port.init) keyboard_x11_port(&kbd_port, &er_kbd);
    er_kbd.display = d;
}

static input_use_case_t input_uc;
static void input_use_case_ensure(void) {
    keyboard_port_ensure();
    if(!input_uc.buffer) input_use_case_init(&input_uc, &input, &kbd_port);
    input_uc.keyboard = &kbd_port;
}

//@category Буфер обмена

static clipboard_port_t clipboard_port_state;

static void clipboard_port_ensure(void) {
    if(!clipboard_port_state.get) clipboard_x11_port(&clipboard_port_state, &er_clipboard);
    er_clipboard.display = d;
    er_clipboard.window = w;
}

static void get_clipboard(clipboard_get_callback_t fn, void* context) {
    clipboard_port_ensure();
    clipboard_port_state.get(&clipboard_port_state,
                             CLIPBOARD_SELECTION_CLIPBOARD,
                             CLIPBOARD_FORMAT_UTF8,
                             fn, context);
}

static int keyboard_current_group(void) {
    keyboard_port_ensure();
    return kbd_port.state(&kbd_port, 0).group;
}

void init_keyboard() {
    keyboard_port_ensure();
    kbd_port.init(&kbd_port);
}

KeySym unikey_to_keysym(unikey_t key) {
    keyboard_port_ensure();
    return (KeySym) kbd_port.to_keysym(&kbd_port, key);
}

unikey_t keyboard_state(int code) {
    keyboard_port_ensure();
    return kbd_port.state(&kbd_port, code);
}

unikey_t keysym_to_unikey(KeySym ks) {
    keyboard_port_ensure();
    return kbd_port.from_keysym(&kbd_port, (uint32_t) ks);
}

void set_group(int group) {
    keyboard_port_ensure();
    kbd_port.set_layout(&kbd_port, group);
}

// маски модификаторов и локов. На некоторых клавиатурах разные модификаторы могут иметь одну и ту же маску. И их нельзя будет различить
// int shift_mask = 1 << 0;
// int lock_mask = 1 << 1;
// int control_mask = 1 << 2;
// int alt_mask = 1 << 3;
// int num_mask = 1 << 4;
// int meta_mask = 1 << 5;
// int meta_mask = 1 << 5;

void print_sym(int mods, KeySym ks) {
    char sym[256];
    keyboard_x11_format_sym(sym, sizeof sym, mods, (uint32_t) ks);
    DEBUG_LOG("%s", sym);
}

// входит в массив
int in_sym(KeySym symbol, KeySym* symbols) {
    while(*symbols)
        if(symbol == *symbols++) return 1;
    return 0;
}

//@category Задержки

static scheduler_port_t scheduler_state;

static void scheduler_port_ensure(void) {
    if(!scheduler_state.after) scheduler_system_port(&scheduler_state, &er_scheduler);
}

// Единая точка постановки таймера: лениво инициализирует порт
static int after(double interval, scheduler_callback_t fn, void* context) {
    scheduler_port_ensure();
    return scheduler_state.after(&scheduler_state, interval, fn, context);
}

static void scheduler_tick(void) {
    scheduler_port_ensure();
    scheduler_state.tick(&scheduler_state);
}

//@category Иксы

static tray_x11_t tray_state;
static tray_port_t tray_port_state;
static tray_indicator_t tray_indicator_state;

static void on_tray_click(void* context) {
    (void) context;
    config_window_pid = desktop_launch_configurator(config_window_pid);
}

static void tray_indicator_ensure(void) {
    if(tray_indicator_state.tray) return;

    tray_x11_port(&tray_port_state, &tray_state);
    tray_indicator_init(&tray_indicator_state, &tray_port_state,
                        on_tray_click, NULL);
}

// Обработка ошибок
int xerror = 0;
static int null_X_error(Display* d, XErrorEvent* err) {
    xerror = True;
#define BUFLEN (1024 * 64)
    char buf[BUFLEN];
    XGetErrorText(d, err->error_code, buf, BUFLEN);
    printf("!!!!!!!!!!!!! X ERROR !!!!!!!!!!!!! %s\n", buf);
    return False;
}

// подключается к дисплею
void open_display() {
    d = XOpenDisplay(NULL);
    if(!d) {
        fprintf(stderr, "Not open display!\n");
        exit(1);
    }

    XSetErrorHandler(null_X_error);

    // Не включаем отладочный синхронный режим Xlib: он делает каждый
    // XQueryKeymap/XGetInputFocus/XQueryPointer полноценным round-trip'ом
    // При опросе клавиатуры каждые loop_delay это заметно увеличивает
    // задержку ввода, особенно под композитором/на медленном X-сервере
    XSynchronize(d, False);

    // Создаём окно
    w = XCreateSimpleWindow(d, XDefaultRootWindow(d), -10, -10, 1, 1, 0, 0, 0x00DC143C);

    // Атомы и размер куска для Selection/INCR принадлежат clipboard-адаптеру
    clipboard_x11_init(&er_clipboard, d, w);
    // подписываемся на события окна
    XSelectInput(d, w,
                 PropertyChangeMask // Работа с буфером
                     | ButtonPressMask // Слушаем клики мышкой на окно
                     | ExposureMask // разрешаем запрос на перерисовку
                     | StructureNotifyMask // сработает, когда станут видны ширина и высота окна
    );

    // стыкуем окно с системным лотком
    tray_indicator_ensure();
    tray_indicator_attach(&tray_indicator_state, d, w);
}

// возвращает текущее окно
Window get_current_window() {
    Window w = 0;
    int revert_to = 0;
    XGetInputFocus(d, &w, &revert_to);
    return w;
}


//@category клавиатура и мышь

// возвращает модификатры с кнопками мыши
unsigned int get_input_state() {
    Window root, dummy;
    int root_x, root_y, w_x, w_y;
    unsigned int mask;
    root = DefaultRootWindow(d);
    XQueryPointer(d, root, &dummy, &dummy,
                  &root_x, &root_y, &w_x, &w_y, &mask);
    return mask;
}

// Эмулирует нажатие или отжатие клавиши
void send_key(unikey_t key, int is_press) {
    keyboard_port_ensure();
    kbd_port.send_key(&kbd_port, key, (keyboard_event_t) is_press);
}

// Эмулирует нажатие и отжатие клавиши
void press_key(unikey_t key) {
    keyboard_port_ensure();
    kbd_port.press_key(&kbd_port, key);
}

// Очищает активные модификаторы клавиатуры
void clear_active_mods() {
    keyboard_port_ensure();
    kbd_port.clear_active_mods(&kbd_port);
}

// восстанавливаем модификаторы
void recover_active_mods() {
    keyboard_port_ensure();
    kbd_port.recover_active_mods(&kbd_port);
}

void add_to_buffer(unikey_t key) {
    input_use_case_ensure();
    input_use_case_handle_key(&input_uc, key);
}

// Начало последнего слова в буфере: после последнего пробела,
// а если пробелов нет — с нуля
int from_space() {
    input_use_case_ensure();
    return input_use_case_from_space(&input_uc);
}

void press_key_multi(unikey_t key, int n) {
    keyboard_port_ensure();
    kbd_port.press_key_multi(&kbd_port, key, n);
}

#define FOR_UTF8(S)                                                                          \
    mbstate_t mbs_UTF8 = {0};                                                                \
    for(size_t charlen_UTF8, i_UTF8 = 0;                                                     \
        (charlen_UTF8 = mbrlen(S + i_UTF8, MB_CUR_MAX, &mbs_UTF8)) != 0 && charlen_UTF8 > 0; \
        i_UTF8 += charlen_UTF8)

#define STEP_UTF8(S, W)                              \
    wchar_t ws_UTF8[4];                              \
    int res_UTF8 = mbstowcs(ws_UTF8, S + i_UTF8, 1); \
    if(res_UTF8 != 1) break;                         \
    wint_t W = ws_UTF8[0];

// TODO: зажатие управляющих клавишь {\Control+Alt}abc{/Control} и {Ctrl+Alt+a}
void sendkeys(char* s) { // печатает с клавиатуры строку в utf8
    keyboard_port_ensure();
    sendkeys_use_case_run(&kbd_port, s);
}

void print_translate_buffer(int from, int backspace) {
    keyboard_port_ensure();
    translate_use_case_buffer(&kbd_port, input.word, input.pos, from, backspace);
}

void print_invertcase_buffer(int from, int backspace) {
    keyboard_port_ensure();
    translate_use_case_invertcase(&kbd_port, input.word, input.pos, from, backspace);
}

char* get_event_type(int n) {
    if(n < 2 || n > LASTEvent) return NULL;

    char* names[] = {
        NULL,
        NULL,
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

// переписываем строку в буфер ввода
void to_buffer(const char* s) {
    input_buffer_reset(&input);

    // utf8 переводим в символы юникода, затем в символы x11, после - в сканкоды
    FOR_UTF8(s) {
        STEP_UTF8(s, ws);
        if(input.pos >= BUF_SIZE) break;
        input_buffer_push(&input, INT_TO_KEY(ws), ws);
    }
}

// нажимаем комбинацию Shift+LeftArrow (выделить предыдущий от курсора символ)
void shift_leftArrow() {
    clear_active_mods();
    unikey_t shift_left = SYM_TO_KEY(XK_Shift_L);
    send_key(shift_left, 1);
    press_key(SYM_TO_KEY(XK_Left));
    send_key(shift_left, 0);
    recover_active_mods();
    kbd_port.set_layout_hint(&kbd_port, keyboard_current_group());
}

// нажимаем комбинацию Control+Insert (выделение добавить в буфер обмена (clipboard))
void control_insert() {
    clear_active_mods();
    unikey_t control_left = SYM_TO_KEY(XK_Control_L);
    send_key(control_left, 1);
    press_key(SYM_TO_KEY(XK_Insert));
    send_key(control_left, 0);
    recover_active_mods();
    kbd_port.set_layout_hint(&kbd_port, keyboard_current_group());
}

// копируем выделенное и заменяем им буфер ввода
void copy_selection_next(void* context, const char* s, int len, clipboard_format_t format) {
    (void) context;
    (void) len;
    (void) format;

    to_buffer(s);
    on_copy_selection(0, 0);
}
void copy_selection(void (*fn)(int, int)) {
    on_copy_selection = fn;
    control_insert();
    get_clipboard(copy_selection_next, NULL);
}

// очистить буфер раздачи данных клипборда
void clipboard_free() {
    clipboard_port_ensure();
    clipboard_port_state.clear(&clipboard_port_state);
}

static event_loop_port_t event_loop_state;

static void event_loop_port_ensure(void) {
    if(!event_loop_state.wait) event_loop_x11_port(&event_loop_state, &er_event_loop);
    er_event_loop.display = d;
}

static void handle_x_event(void* context, const void* native_event) {
    (void) context;
    XEvent* event = (XEvent*) native_event;

    const char* event_name = get_event_type(event->type);
    DEBUG_LOG("event %i %s\n", event->type, event_name ? event_name : "?");

    if(tray_indicator_handle_event(&tray_indicator_state, event)) return;

    switch(event->type) {
    case SelectionNotify: // получить данные из буфера обмена
    case SelectionClear:
    case SelectionRequest: // запрос данных из буфера обмена
    case PropertyNotify: // протокол INCR: отправка по частям
        clipboard_port_ensure();
        clipboard_x11_handle_event(&er_clipboard, event);
        break;
    }
}

void event_delay(double seconds) {
    // задержка, но коль пришли события иксов - сразу выходим
    event_loop_port_ensure();
    event_loop_state.wait(&event_loop_state, seconds);
    event_loop_state.drain(&event_loop_state, handle_x_event, NULL);
}

void change_key(int code) {
    unikey_t key = keyboard_state(code);
    KeySym ks = KEY_TO_SYM(key);

    if(ERSWITCHER_DEBUG) {
        // XkbKeysymToModifiers()
        unsigned int state = get_input_state();
        char state_buf[128];
        size_t pos = 0;
        for(int i = 0; i < 32; i++) {
            if(state & (1 << i))
                pos += (size_t) snprintf(state_buf + pos, sizeof state_buf - pos, "%i", i);
            else if(pos + 1 < sizeof state_buf)
                state_buf[pos++] = ' ';
        }
        state_buf[pos] = '\0';

        char layout[16];
        if(key.group == kbd_port.layout_en(&kbd_port))
            snprintf(layout, sizeof layout, "en");
        else if(key.group == kbd_port.layout_ru(&kbd_port))
            snprintf(layout, sizeof layout, "ru");
        else
            snprintf(layout, sizeof layout, "%i", key.group);

        char sym[256];
        keyboard_x11_format_sym(sym, sizeof sym, key.mods, (uint32_t) ks);
        DEBUG_LOG("change_key %i (%s) %s: %s\n", code, state_buf, layout, sym);
    }

    // нажата комбинация? выполняем действие
    int mods = key.mods & ~(LockMask | NumMask);

    // ищем соответствие среди функций
    for(int i = 0; i < keyfn_size; i++) {
        if(keyfn[i].key.mods == mods && keyfn[i].key.code == code) {
            keyfn_active = i;
            return;
        }
    }

    input_use_case_ensure();
    input_use_case_handle_key(&input_uc, key);
}

static snippets_use_case_t snippets_uc;

static void snippets_use_case_ensure(void) {
    if(snippets_uc.clipboard) return;

    clipboard_port_ensure();
    scheduler_port_ensure();
    keyboard_port_ensure();

    snippets_use_case_init(&snippets_uc, &clipboard_port_state,
                           &scheduler_state, &kbd_port);
}

void insert_from_clipboard(const char* s) {
    snippets_use_case_ensure();
    snippets_use_case_insert(&snippets_uc, s);
}

/*

-2. У команды есть таймаут
-1. Нельзя запустить несколько команд одновременно
0. Копируем выделенное в буфер, а из него – в файл, если указан %i
1. Запускаем команду в отдельном процессе
2. Опрашиваем по таймеру: завершился ли процесс
3. Когда процесс завершился, то считываем файл %o и вставляем из буфера

*/

const char* curcommand_infile = "/tmp/erswitcher-input";
const char* curcommand_outfile = "/tmp/erswitcher-output";
float command_timeout = 13; // В секундах
float command_interval = 0.1; // Интервал, через который опрашивается команда

static command_runner_port_t command_runner_port_state;
static command_runner_fork_t command_runner_state;
static commands_use_case_t commands_uc;

static void command_runner_ensure(void) {
    if(command_runner_port_state.prepare) return;

    command_runner_state.infile = curcommand_infile;
    command_runner_state.outfile = curcommand_outfile;
    command_runner_fork_port(&command_runner_port_state, &command_runner_state);
}

int is_blank(char* s) {
    return commands_use_case_is_blank(s);
}

// Из формата делает путь
char* command_fmt2ptr(const char* clipboard, const char* command) {
    command_runner_ensure();
    return command_runner_port_state.prepare(
        &command_runner_port_state, command, clipboard ? clipboard : "",
        clipboard ? (int) strlen(clipboard) : 0);
}

static void commands_request_selection(void* context) {
    (void) context;
    control_insert();
}

static void commands_insert_text(void* context, const char* text) {
    (void) context;
    insert_from_clipboard(text);
}

static void commands_use_case_ensure(void) {
    if(commands_uc.runner) return;

    command_runner_ensure();
    clipboard_port_ensure();
    scheduler_port_ensure();

    commands_use_case_init(&commands_uc, &command_runner_port_state,
                           &clipboard_port_state, &scheduler_state,
                           commands_request_selection, NULL,
                           commands_insert_text, NULL,
                           command_timeout, command_interval);
}

void run_command(char* s) {
    commands_use_case_ensure();
    commands_use_case_run(&commands_uc, s);
}

// Нажата клавиша компосе - нужно заменить мнемонику на значение
int cursor_back = 0;
void cursor_back__fn(void* context) {
    (void) context;
    DEBUG_LOG("Курсор влево на %i позиций\n", cursor_back);
    press_key_multi(SYM_TO_KEY(XK_Left), cursor_back);
    cursor_back = 0;
}

void compose(char*) {
    if(input.pos == 0) return;

    wint_t symbols[BUF_SIZE];
    for(int i = 0; i < input.pos; i++)
        symbols[i] = KEY_TO_INT(input.word[i]);

    int match = compose_find(compose_map, compose_map_size, symbols, input.pos);
    if(match < 0) {
        DEBUG_LOG("compose: Ничего не найдено!\n");
        return;
    }

    compose_t* entry = &compose_map[match];
    char* to = entry->to;
    int remove_sym = entry->pos;

    cursor_back = entry->cursor_back;

    DEBUG_LOG("compose -> %s\n", to);

    // удаляем мнемонику введённую ранее
    // Модификаторов тут не нужно: раскладку берём из последнего введённого символа,
    // как при печати — так работает и под тестами, где XkbGetState не настоящий
    unikey_t key = input.pos ? input.word[input.pos - 1] : keyboard_state(0);
    press_key_multi(SYM_TO_KEY(XK_BackSpace), remove_sym);
    set_group(key.group);
    // подменяем буфер собой и вставляем из него композэ
    insert_from_clipboard(to);

    if(cursor_back) after(0.30, cursor_back__fn, NULL);
}

void word_translate(char*) {
    print_translate_buffer(from_space(), 1);
}
void text_translate(char*) {
    print_translate_buffer(0, 1);
}
void selection_translate(char*) {
    copy_selection(print_translate_buffer);
}

void word_invertcase(char*) {
    print_invertcase_buffer(from_space(), 1);
}
void text_invertcase(char*) {
    print_invertcase_buffer(0, 1);
}
void selection_invertcase(char*) {
    copy_selection(print_invertcase_buffer);
}

volatile sig_atomic_t load_config_requested = 0;

void signal_load_config(int) {
    load_config_requested = 1;
}

static void config_log(void* context, const char* message) {
    (void) context;
    fprintf(stderr, "%s\n", message);
}

static int config_resolve_key(const char* name, unikey_t* key, void* context) {
    (void) context;

    KeySym ks = XStringToKeysym(name);
    if(ks == NoSymbol) return 0;

    unikey_t parsed = keysym_to_unikey(ks);
    if(parsed.code == 0) return 0;

    *key = parsed;
    return 1;
}

static void apply_config_options(const config_options_t* options) {
    keyboard_port_ensure();

    int delay_us = (int) (options->delay * 1000000.0);
    kbd_port.set_delay(&kbd_port, delay_us);
    loop_delay = (int) (options->loop_delay * 1000000.0);
    command_timeout = options->command_timeout;
    command_interval = options->command_interval;
    commands_use_case_set_options(&commands_uc, command_timeout, command_interval);

    DEBUG_LOG("delay = %i микросекунд\n", delay_us);
    DEBUG_LOG("loop_delay = %i микросекунд\n", loop_delay);
    DEBUG_LOG("command_timeout = %g секунд\n", command_timeout);
    DEBUG_LOG("command_interval = %g секунд\n", command_interval);
}

static void print_config_debug(void) {
    if(!ERSWITCHER_DEBUG) return;

    for(int i = 0; i < keyfn_size; i++) {
        const config_binding_t* binding = &keyfn[i];
        KeySym ks = KEY_TO_SYM(binding->key);
        char sym[256];
        keyboard_x11_format_sym(sym, sizeof sym, binding->key.mods, (uint32_t) ks);
        DEBUG_LOG("------------------------------\n");
        DEBUG_LOG("%s = %s\n", sym, binding->arg ? binding->arg : "(null)");
    }

    for(int i = 0; i < compose_map_size; i++) {
        DEBUG_LOG("compose[%i]: pos=%i cursor_back=%i -> %s\n", i,
                  compose_map[i].pos, compose_map[i].cursor_back,
                  compose_map[i].to ? compose_map[i].to : "");
    }
}

void load_config() {
    DEBUG_LOG("Конфигурация применяется ...\n");

    config_store_ensure();

    const char* path = config_store_state.path(&config_store_state);

    config_reload_context_t reload = {
        .store = &config_store_state,
        .path = path,
        .parse = {
            .resolve_key = config_resolve_key,
            .resolver_context = NULL,
            .log = config_log,
            .log_context = NULL,
            .base_options = &current_config.options,
        },
    };

    config_t next;
    if(!config_reload(&next, &reload)) {
        // Как старый load_config(): неудачная загрузка оставляет пустые
        // привязки/compose, но прежние [options]
        config_free(&next);
        config_clear_entries(&current_config);
        keyfn_active = -1;
        DEBUG_LOG("Конфигурация применена!\n");
        return;
    }

    config_free(&current_config);
    current_config = next;
    keyfn_active = -1;

    apply_config_options(&current_config.options);
    print_config_debug();

    DEBUG_LOG("Конфигурация применена!\n");
}

static void execute_config_action(const config_binding_t* binding) {
    if(!binding) return;

    switch(binding->action) {
    case CONFIG_ACTION_WORD_TRANSLATE: word_translate(binding->arg); break;
    case CONFIG_ACTION_TEXT_TRANSLATE: text_translate(binding->arg); break;
    case CONFIG_ACTION_SELECTED_TRANSLATE: selection_translate(binding->arg); break;

    case CONFIG_ACTION_WORD_INVERTCASE: word_invertcase(binding->arg); break;
    case CONFIG_ACTION_TEXT_INVERTCASE: text_invertcase(binding->arg); break;
    case CONFIG_ACTION_SELECTED_INVERTCASE: selection_invertcase(binding->arg); break;

    case CONFIG_ACTION_COMPOSE: compose(binding->arg); break;
    case CONFIG_ACTION_SENDKEYS: sendkeys(binding->arg); break;
    case CONFIG_ACTION_SNIPPET: insert_from_clipboard(binding->arg); break;
    case CONFIG_ACTION_COMMAND: run_command(binding->arg); break;

    case CONFIG_ACTION_NONE: break;
    }
}

void app_run(int ac, char** av);

void app_run(int ac, char** av) {
    logger_init();

    if(!ac) fprintf(stderr, "ERROR: а где путь к программе?\n");

    char* locale = "ru_RU.UTF-8";
    if(!setlocale(LC_ALL, locale)) {
        fprintf(stderr, "setlocale(LC_ALL, \"%s\") failed!\n", locale);
        exit(1);
    }

    const char* appimage = getenv("APPIMAGE");
    char* program = (appimage && *appimage) ? strdup(appimage)
                                            : realpath(av[0], NULL);
    if(!program) {
        fprintf(stderr, "WARN: не найден реальный путь к программе %s\n", av[0]);
        exit(1);
    }

    // меняет директорию на дир. пользователя
    if(!desktop_goto_home()) exit(1);

    desktop_init(program); // создаёт десктопный файл, если его нет
    free(program);

    open_display();
    desktop_check_any_instance();
    scheduler_system_reset(&er_scheduler); // инициализируем таймеры

    // Начальные установки
    current_win = get_current_window();
    input_buffer_init(&input);
    init_keyboard();

    // конфигурация должна загружаться после инициализации клавиатуры
    config_init(&current_config);
    load_config();

    signal(SIGHUP, signal_load_config);
    signal(SIGCHLD, SIG_IGN);

    unsigned int state1, state2 = 0;
    kbd_port.poll_saved(&kbd_port);


    while(1) {

        // сбросим ввод, коль нажата мышка
        state1 = get_input_state();
        state1 &= Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask;
        if(state1 != state2) input_buffer_reset(&input);
        state2 = state1;

        // Если сменилось окно, то начинаем ввод с начала
        Window w = get_current_window();
        if(w != current_win) {
            input_buffer_reset(&input);
            current_win = w;
        }

        // опрашиваем клавиатуру
        const char* keys = kbd_port.keymap(&kbd_port);
        const char* saved = kbd_port.keymap_saved(&kbd_port);
        kbd_port.poll(&kbd_port);
        keys_pressed = -1;
        for(int i = 0; i < KEYBOARD_SIZE; i++) {

            if(keys_pressed < 0) {
                keys_pressed = 0;
                for(int i = 0; i < KEYBOARD_SIZE; i++)
                    if(BIT(keys, i) != 0) keys_pressed++;
            }

            if(BIT(keys, i) != BIT(saved, i)) {
                if(BIT(keys, i) != 0) { // клавиша нажата
                    change_key(i);
                } else { // клавиша отжата
                    if(keys_pressed == 0 && keyfn_active != -1) {
                        execute_config_action(&keyfn[keyfn_active]);
                        keyfn_active = -1;
                    }
                }
            }
        }

        kbd_port.swap_keymap(&kbd_port);

        // крошим зомбиков
        int status;
        waitpid(-1, &status, WNOHANG);

        // сработают таймеры, чьё время пришло
        scheduler_tick();

        event_delay(USEC_TO_DBL(loop_delay));

        // обработка сигналов
        if(load_config_requested) {
            load_config_requested = 0;
            load_config();
        }
    }
}

void app_reset_runtime(void) {
    commands_use_case_reset(&commands_uc);
    command_runner_fork_reset(&command_runner_state);
    commands_use_case_set_options(&commands_uc, command_timeout, command_interval);
}
