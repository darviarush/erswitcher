# Архитектура erswitcher

## 1. Цель и ограничения

Задача — разобрать монолит `erswitcher.c` на модули по принципам чистой
архитектуры, **не меняя поведение приложения**.

Принятые ограничения:

1. **Вариант А: сохраняем текущую семантику.** Транслитерация остаётся
   операцией «переключить раскладку и повторить сканкоды», а не чистой
   функцией `ghbdtn -> привет`. Таблицы посимвольной транслитерации не
   вводим.
2. X11 остаётся единственной инфраструктурой на этом этапе. Wayland —
   отдельная будущая задача, но порты должны позволить его добавить.
3. Формат `erswitcher.conf`, пути к файлам, поведение трея и горячих
   клавиш не меняются.
4. Перенос идёт постепенно (strangler): после каждого шага собирается
   проект, зелёные юнит- и функциональные тесты, работает ручной smoke.
5. Код остаётся на C. Черновики `src/` с C++-инициализаторами в
   структурах не являются контрактом и на шаге 0 приводятся к C.

## 2. Слои и правило зависимостей

Направление стрелки — «зависит от»:

```
main.c / app (composition root)
        │
        ├──────────────► presentation
        │
        └──────────────► infrastructure ──► ports (интерфейсы)
                                                   ▲
                                                   │
                                             use-cases
                                                   │
                                                   ▼
                                               domain
```

Правила:

- `domain` не знает ни о X11, ни о файлах, ни о процессах, ни о времени.
- `use-cases` знает `domain` и **интерфейсы** из `ports`, но не их
  реализации.
- `infrastructure` реализует `ports`; здесь живут X11, `fork/exec`,
  файловая система и системные часы.
- `presentation` — трей и консольный вывод; использует `use-cases`/`ports`.
- `main.c` и `app/` — единственное место, где всё связывается вместе.
- В `domain/` и `use-cases/` запрещены `#include <X11/...>`, `fork`,
  `exec*`, `open`, `system`, `pthread` и т.п.

## 3. Семантика транслитерации (вариант А)

Важно для разбиения: сейчас `print_translate_buffer()` **не перекодирует
строку**. Он:

1. сохраняет активные модификаторы;
2. переключает группу раскладки на противоположную;
3. отправляет Backspace;
4. повторно нажимает накопленные `unikey_t` в другой группе;
5. восстанавливает модификаторы и раскладку.

`print_invertcase_buffer()` так же работает с `unikey_t.mods`, инвертируя
`ShiftMask`, а не с регистром строки.

Следствия для архитектуры:

- доменная модель — `unikey_t` (сканкод + модификаторы + группа), а не
  `char*`/`wchar_t*`;
- «чистыми» в домене остаются: границы слова, накопление буфера,
  инверсия модификаторов, поиск `[compose]`, разбор значений конфига,
  подстановка `%i`/`%o`;
- `InputBuffer` хранит `unikey_t` вместе с Unicode-символом: границы
  слова (`from_space`) и `[compose]` работают с `wint_t`, а повторное
  нажатие — со сканкодом/группой. Символ получается через порт
  клавиатуры при добавлении в буфер;
- преобразования `unikey_t ↔ KeySym ↔ Unicode` — это порт клавиатуры и
  его X11-адаптер;
- `[sendkeys]` по-прежнему печатает UTF-8-строку через эмуляцию клавиш.

## 4. Целевая структура файлов

Это ориентир. Допускается объединять мелкие модули, но границы слоёв и
ответственности должны сохраняться.

```
src/
├── domain/                         # чистые правила, без I/O
│   ├── unikey.h/.c                 # unikey_t, доменные ERS_MOD_*, операции над ключом
│   ├── input-buffer.h/.c           # word[], symbol[], pos: init/reset/push/backspace/from_space
│   ├── case-invert.h/.c            # инверсия ShiftMask у unikey_t
│   ├── compose.h/.c                # compose_t, поиск мнемоники в буфере
│   ├── config.h/.c                 # Config, Binding, ComposeEntry, Snippet, Command, Options
│   ├── command-template.h/.c       # чистая подстановка %i/%o/%%
│   └── text.h/.c                   # utf8_strlen, обход UTF-8 (перенос из монолита)
│
├── use-cases/                      # сценарии приложения
│   ├── input.h/.c                  # приём клавиши, Backspace, сброс при смене окна/мыши
│   ├── translate.h/.c              # word/text/selected translate и invertcase
│   ├── compose_input.h/.c          # применение compose и возврат курсора
│   ├── sendkeys.h/.c               # ввод строки через клавиатуру
│   ├── snippets.h/.c               # вставка сниппета через clipboard
│   ├── commands.h/.c               # координация [commands]
│   └── config-reload.h/.c          # загрузка/перезагрузка конфига, привязка функций
│
├── ports/                          # только объявления контрактов
│   ├── keyboard.h                  # состояние, раскладки, инъекция, модификаторы
│   ├── clipboard.h                 # асинхронные get/set, INCR
│   ├── scheduler.h                 # after/tick/таймауты
│   ├── event-loop.h                # ожидание и доставка X11-событий
│   ├── config-store.h              # чтение/запись/путь конфига
│   ├── command-runner.h            # запуск шелл-команд, timeout, %i/%o
│   ├── tray.h                      # systray: dock, клик, запуск конфигуратора
│   └── logger.h                    # debug/warn/info
│
├── infrastructure/
│   ├── x11/
│   │   ├── display.h/.c            # Display, атомы, соединение
│   │   ├── keyboard.h/.c           # Xkb/XTest-реализация keyboard.h
│   │   ├── clipboard.h/.c          # Selection/INCR-реализация clipboard.h
│   │   ├── events.h/.c             # XNextEvent/XPending/select
│   │   └── tray.h/.c               # _NET_SYSTEM_TRAY, окно, клики
│   └── system/
│       ├── config-store-fs.h/.c    # ~/.config/erswitcher.conf
│       ├── command-runner-fork.h/.c # fork/system, временные файлы, timeout
│       ├── scheduler-time.h/.c     # gettimeofday/select
│       └── desktop.h/.c            # autostart/desktop, goto_home
│
├── presentation/
│   └── tray-indicator.h/.c         # отрисовка/состояние иконки, поверх tray-порта
│
└── app/
    ├── app.h                       # состояние, обёртки, API для main/тестов
    └── app.c                       # главный цикл и composition root

main.c                              # только вызов app_run()
```

`tests/`:

```
tests/unit/          # domain и use-cases с фейковыми портами
tests/functional/    # существующие Tcl/Tk-тесты конфигуратора
```

## 5. Карта переноса монолита

| Что в `erswitcher.c` | Ответственность | Куда переносится |
|---|---|---|
| `unikey_t`, `KEY_TO_*`, `SYM_TO_*`, `unikey_to_keysym`, `keysym_to_unikey`, `keyboard_state`, `in_sym` | модель ключа и преобразования | `domain/unikey` + `ports/keyboard` + `infrastructure/x11/keyboard` |
| `init_keyboard`, `groups`, `group_ru`, `group_en`, `maybe_group`, `set_group`, `set_locks` | раскладки и локи | `ports/keyboard` + `infrastructure/x11/keyboard` |
| `press`, `send_mods`, `press_mods`, `send_key`, `press_key`, `active_codes`, `active_state`, `clear_active_mods`, `recover_active_mods` | инъекция клавиш и активные модификаторы | `infrastructure/x11/keyboard` |
| `keyboard_buf1/2`, `saved`, `keys` | опрос нажатых клавиш | `infrastructure/x11/keyboard` |
| `pos`, `word`, `keys_pressed`, `add_to_buffer`, `from_space` | кольцевой буфер ввода | `domain/input-buffer` + `use-cases/input`; `keys_pressed` остаётся состоянием опроса |
| `change_key` | классификация клавиши и диспетчеризация | `use-cases/input` + привязки конфига |
| `print_translate_buffer`, `print_invertcase_buffer`, `word_*`, `text_*`, `selection_*` | сценарии трансляции/регистра | `use-cases/translate` + `domain/case-invert`; `selection_*` используют clipboard-порт |
| `sendkeys`, `to_buffer`, `utf8_strlen`, `FOR_UTF8`, `press_key_multi` | ввод UTF-8 строки | `use-cases/sendkeys` + `domain/text` |
| `compose_map`, `compose`, `cursor_back`, `cursor_back__fn` | мнемоники и возврат курсора | `domain/compose` + `use-cases/compose_input` |
| `get_selection`, `set_clipboard`, `clipboard_*`, `selection_*`, `incr_*`, части `event_next` | буфер обмена и INCR | `ports/clipboard` + `infrastructure/x11/clipboard` |
| `shift_leftArrow`, `control_insert`, `shift_insert`, `copy_selection`, `insert_from_clipboard`, `copy_selection_next` | операции через clipboard | `use-cases` (translate/snippets/compose) + `ports/clipboard` |
| `timers`, `timers_size`, `after`, `timers_apply` | отложенные вызовы | `ports/scheduler` + `infrastructure/system/scheduler-time` |
| `event_next`, `event_delay`, `XQueryKeymap`-опрос из `app_run` | событийный цикл | `ports/event_loop` + `infrastructure/x11/events` + `app/app_run` |
| `load_config`, `keyfn*`, `compose_map*`, `signal_load_config` | конфиг и привязки | `domain/config` + `use-cases/config-reload` + `ports/config-store` |
| `run_command`, `command_fmt2ptr`, `on_run_command`, `_interval_run_command`, `curcommand*` | `[commands]` | `use-cases/commands` + `ports/command-runner` + `infrastructure/system/command-runner-fork` |
| `send_systray_message`, окно в `open_display`, `ButtonPress`, `ClientMessage`, `ConfigureNotify` | systray | `ports/tray` + `infrastructure/x11/tray` + `presentation/tray-indicator` |
| `goto_home`, `init_desktop`, `check_any_instance` | системная интеграция | `infrastructure/system/desktop` |
| `print_sym`, `get_event_type`, `DEBUG`, диагностика | логирование | `ports/logger` + `presentation/console`; `print_sym` пока в X11-адаптере |
| `open_display`, атомы, `xerror` | X11-соединение | `infrastructure/x11/display` |
| `app_run`, `main.c` | запуск и главный цикл | `app/context`, `app/app_run`, `main.c` |
| `mock.c`, `test.h` | тестовая обвязка | `tests/unit` на фейковых портах |

`open_display()` придётся разделить: соединение и атомы — в `display`,
создание окна/докинг — в `infrastructure/x11/tray`.

## 6. Порты: важные контракты

Точные сигнатуры фиксируются при реализации, но контракты должны
учитывать следующее.

### keyboard

Сейчас контракт выглядит так:

- `init()` — инициализация раскладок;
- `state(code)` — текущие `mods`/`group`;
- `to_keysym(key)` / `from_keysym(ks)` — преобразования;
- `to_symbol(key)` / `from_symbol(symbol)` — Unicode-символ для буфера и
  `[compose]`;
- `kind(key)` — Backspace / управляющая / печатная / прочая клавиша;
- `layout_count()` / `layout_ru()` / `layout_en()` / `layout_hint()`;
- `set_layout_hint(group)` — с какой группы начинать поиск сканкода;
- `set_layout(group)` — переключение раскладки;
- `set_active_group(group)` — какую группу восстановить после ввода;
- `set_delay(us)` — задержка программных нажатий;
- `send_key(key, event)` / `press_key(key)` / `press_key_multi(key, n)`;
- `clear_active_mods()` / `recover_active_mods()` — сохранение и
  восстановление зажатых модификаторов;
- `keymap()` / `keymap_saved()` / `poll()` / `poll_saved()` /
  `swap_keymap()` — текущая и предыдущая карты нажатых клавиш для
  опроса.

Доменные модификаторы (`ERS_MOD_SHIFT`, `ERS_MOD_LOCK`, …) не должны
называться `ShiftMask`/`LockMask`. На первом этапе их числовые значения
можно оставить совпадающими с X11, чтобы снизить риск.

В монолите пока остались старые имена (`init_keyboard`, `send_key`,
`press_key`, `keyboard_state`, `unikey_to_keysym`, `keysym_to_unikey`,
`set_group`, `clear_active_mods`, `recover_active_mods`) — это тонкие
обёртки над портом. Макросы совместимости над `er_kbd` убраны: раскладки,
`delay`, `saved/keys` и `active_*` доступны только через порт или сам
X11-адаптер. Обёртки исчезнут после переноса `app_run`.

### clipboard

Контракт асинхронный:

- `get(selection, format, callback, context)` — результат может прийти
  позже, в том числе через INCR;
- `set(data, len, format)` — владение памятью передаётся порту;
- `clear()` — освободить данные и перестать быть владельцем;
- данные в callback принадлежат порту и живут только до возврата из
  callback; ошибка/пустой буфер — тоже вызов callback с `""`.

Нельзя делать вид, что это синхронные `char* get()`/`set()`: текущий код
живёт на `SelectionNotify`/`SelectionRequest` и колбэках.

### scheduler

- `after(seconds, fn, context)`;
- `tick()` — выполнить созревшие таймеры;
- отмена/таймауты команд.

### event_loop

- ожидание события с таймаутом (`select` по `ConnectionNumber`);
- доставка X11-событий;
- опрос клавиатуры (`XQueryKeymap`) остаётся в X11-адаптере, а решение
  «что делать с изменением» — в `use-cases/input`.

### config_store

- путь `~/.config/erswitcher.conf`;
- чтение, запись, создание конфига по умолчанию;
- сигнал/флаг перезагрузки (SIGHUP → `volatile sig_atomic_t`, не прямая
  загрузка из обработчика).

### command_runner

- запуск `sh`-команды в отдельном процессе;
- таймаут и периодический опрос;
- временные файлы для `%i`/`%o`;
- запрет параллельного запуска (как сейчас).

### tray

- докинг окна в `_NET_SYSTEM_TRAY_S0`;
- клик → запуск/перезапуск конфигуратора;
- сообщения systray.

## 7. Владение состоянием

Глобальные переменные монолита переходят в `AppContext` и доменные
структуры:

```c
typedef struct {
    Config config;                 // options, bindings, compose, snippets, commands
    InputBuffer input;             // word[], symbol[], pos
    int keys_pressed;              // опрос клавиатуры, не содержимое буфера
    int current_win;
    volatile sig_atomic_t load_config_requested;

    keyboard_port_t  keyboard;
    clipboard_port_t clipboard;
    scheduler_port_t scheduler;
    event_loop_port_t events;
    config_store_port_t store;
    command_runner_port_t commands;
    tray_port_t tray;
    logger_port_t log;
} AppContext;
```

Правила:

- `InputBuffer` владеет `word[]`, `symbol[]`, `pos`; `keys_pressed`
  относится к опросу клавиатуры и остаётся в `use-cases/input`/
  X11-адаптере;
- `Config` — неизменяемый снимок; при перезагрузке новый конфиг
  собирается целиком и заменяет старый, старый освобождается;
- X11 `Display`, атомы, активные модификаторы и буферы clipboard живут в
  адаптерах;
- обработчик сигнала меняет только `volatile sig_atomic_t`-флаг;
- `void (*)(char*)` + sentinel `-1/-2` заменяются на enum действий и
  типизированные аргументы;
- `keyboard_buf1/2` и `saved/keys` — состояние X11-адаптера, а не домена.

## 8. Совместимость

На время переноса и после него сохраняем:

- секции `[functions]`, `[options]`, `[compose]`, `[snippets]`,
  `[sendkeys]`, `[commands]` и их синтаксис;
- `\=`, `\n`, `\t`, `\\`, `\^`;
- значения по умолчанию (`delay=0,01`, `loop_delay=0,01`,
  `command_timeout`, `command_interval`);
- создание `~/.config/erswitcher.conf`, если его нет;
- autostart/desktop-файлы и путь к конфигуратору;
- запуск по клику на иконке в трее;
- перезагрузку по `SIGHUP`;
- `setlocale(LC_ALL, "ru_RU.UTF-8")`: от него зависит разбор `delay=0,01`
  через `atof()` с запятой;
- текст конфига по умолчанию переносится без изменений.

Временные файлы `%i`/`%o` пока остаются прежними
(`/tmp/erswitcher-input`, `/tmp/erswitcher-output`), чтобы не менять
поведение.

## 9. Порядок миграции

### Шаг 0. База и уборка

- Зафиксировать baseline: `make test` (юнит + функциональные) и ручной
  smoke-чеклист.
- Привести `src/` к C: убрать C++-инициализаторы из `keyboard.h`,
  `buffer.c` → корректный `.h`/`.c`, добавить недостающие include.
- Настроить сборку по объектным файлам; монолит пока остаётся.
- Решить, какие черновики `src/` удалить, а какие станут основой.

### Шаг 1. Чистый домен

Вынести и покрыть тестами:

- `domain/unikey`;
- `domain/input-buffer`;
- `domain/case-invert`;
- `domain/compose` (поиск, без вставки);
- `domain/text` (UTF-8);
- `domain/command-template`;
- `domain/config` (модель и разбор значений без I/O и X11).

Монолит продолжает работать; новые модули используются через адаптеры
или пока только тестами.

### Шаг 2. Клавиатура

Сделано:

- введён `ports/keyboard` с типизированными операциями вместо прямых
  вызовов X11;
- Xkb/XTest-код, раскладки, инъекция клавиш, активные модификаторы и
  отладочная печать клавиш перенесены в
  `infrastructure/x11/keyboard.c`;
- старые функции монолита заменены тонкими обёртками, поэтому вызовы в
  `erswitcher.c` и `tests/unit` не пришлось переписывать;
- состояние `groups`, `group_ru`, `group_en`, `maybe_group`, `delay`,
  `saved/keys`, `active_*` живёт в `keyboard_x11_t` (`er_kbd`); макросы
  совместимости убраны, доступ идёт через `ports/keyboard`;
- добавлен отдельный набор `keyboard port` в юнит-тестах: контракт
  проверяется через мок X11;
- `add_to_buffer`/`from_space` и обработка Backspace/управляющих клавиш
  перенесены в `use-cases/input`; классификация клавиши идёт через
  `keyboard->kind()`, без X11 в сценарии;
- `print_translate_buffer`/`print_invertcase_buffer` и `word_*`/`text_*`
  перенесены в `use-cases/translate` (вариант А);
- глобальные `pos/word` заменены на `domain/input-buffer` (`input`):
  `use-cases/input` пополняет `word[]` и `symbol[]`, `from_space` читает
  сохранённые символы; `to_buffer` тоже заполняет оба массива;
- добавлен набор `use cases` с фейковым портом клавиатуры;
- `delay` из `[options]` устанавливается через `keyboard->set_delay()`,
  опрос клавиатуры в `app_run` использует `keymap()/keymap_saved()/
  poll()/poll_saved()/swap_keymap()`; макросы совместимости над
  `er_kbd` удалены.

Осталось в этом шаге:

- убрать старые обёртки после переноса `app_run`.

### Шаг 3. Clipboard

- Введён асинхронный `ports/clipboard`: `get(selection, format, callback,
  context)`, `set(data, len, format)` с передачей владения памятью и
  `clear()`. Callback получает данные только на время вызова; пустой
  буфер/ошибка приходят как `""`, а не как молчание.
- Selection/INCR, атомы, `selection_chunk_size`, буферы отправки/приёма и
  ожидающий callback перенесены в `infrastructure/x11/clipboard.c`.
- `open_display()` создаёт атомы через `clipboard_x11_init()`;
  `event_next()` передаёт SelectionNotify/SelectionClear/SelectionRequest/
  PropertyNotify в `clipboard_x11_handle_event()`.
- `selection translate`/`selection invertcase`, snippets и compose работают
  через порт: `copy_selection()` и `insert_from_clipboard()` больше не
  вызывают X11 напрямую.
- В монолите остались compatibility-макросы над `er_clipboard` и обёртка
  `clipboard_free()`; они исчезнут вместе с `app_run`.
- Добавлен набор `clipboard` в юнит-тестах: set/clear, собственный
  буфер, отсутствие владельца, асинхронный callback и INCR.

### Шаг 4. Время и события

Сделано:

- введён `ports/scheduler`: `after(seconds, fn, context)` и `tick()`;
  системная реализация `infrastructure/system/scheduler-time` хранит
  очередь из 256 одноразовых таймеров и по-прежнему использует
  `gettimeofday`, поэтому задержки команд и snippets не изменились;
- введён `ports/event_loop`: `wait(timeout)` и `drain(handler)`;
  X11-реализация `infrastructure/x11/events` делает `select` по
  `ConnectionNumber`, затем `XPending`/`XNextEvent`;
- обработка конкретных X11-событий (трей, clipboard, configure/expose)
  осталась в слое приложения отдельным callback'ом; сам цикл `app_run`
  позже перенесён в `src/app/app.c`;
- опрос `XQueryKeymap` перенесён в `keyboard->poll()`/`poll_saved()`;
  `app_run` больше не вызывает X11 напрямую для клавиатуры;
- добавлен набор `scheduler port` и проверка опроса карт клавиатуры в
  `keyboard port`;
- ручной smoke: команда по горячей клавише (таймер опроса процесса) и
  snippet (таймер Shift+Insert + асинхронный clipboard) работают.

Осталось:

- перенести `app_run` и контекст приложения в `app/` (шаги 7–8).

### Шаг 5. Конфиг

Сделано:

- введён `ports/config-store` (`path`/`read`/`ensure_default`) и файловая
  реализация `infrastructure/system/config-store-fs`; текст конфига по
  умолчанию перенесён туда без изменений;
- введён `use-cases/config-reload`: текст разбирается в `config_t` с
  `options`, привязками и `[compose]`; файловый ввод-вывод остаётся за
  портом, X11 — за резолвером клавиш;
- `keyfn_t` с `void (*fn)(char*)` и sentinel-значениями удалён: привязки
  хранят `config_action_t` и аргумент, диспетчеризация — в
  `execute_config_action()`;
- `[options]` сохраняют прежнее поведение накопления: при перезагрузке
  новый снимок стартует от текущих значений, а отсутствующая секция их
  не сбрасывает;
- `SIGHUP` теперь только выставляет `volatile sig_atomic_t`, разбор идёт
  в главном цикле;
- добавлен набор `config reload` с фейковым резолвером клавиш;
- ручной smoke: команда и snippet из модели, повторная загрузка по
  `SIGHUP` заменяет привязки.

Осталось:

- команды, snippets и sendkeys перенесены на шаге 6.

### Шаг 6. Команды, snippets, sendkeys

Сделано:

- введён `ports/command-runner` (`prepare`/`start`/`poll`/`cancel`/
  `output`) и fork-реализация
  `infrastructure/system/command-runner-fork`: подстановки `%i`/`%o`/`%%`,
  временные файлы, `system`, `waitpid` и убийство группы процессов;
- `use-cases/commands` хранит только состояние сценария: запрет
  параллельного запуска, ожидание clipboard для `%i`, интервал опроса,
  таймаут и вставку вывода; время идёт через `ports/scheduler`, процессы —
  через `ports/command-runner`;
- `use-cases/sendkeys` вводит UTF-8 строку через `ports/keyboard`, без
  X11 и без старых макросов `FOR_UTF8`;
- `use-cases/snippets` кладёт текст в `ports/clipboard` и ставит
  отложенный Shift+Insert через `ports/scheduler`/`ports/keyboard`;
- монолит оставил только compatibility-обёртки `sendkeys()`,
  `insert_from_clipboard()`, `run_command()`, `command_fmt2ptr()`;
- добавлены тесты сценария `commands` на фейковых портах: `%i`/`%o`,
  таймаут, запрет параллельного запуска и пустая команда;
- ручной smoke: команда с `%o`, команда с `%i`/`%o`, таймаут, snippet,
  sendkeys, SIGHUP и `word translate`.

Осталось:

- `shift_leftArrow`, `control_insert` и `copy_selection` пока остаются
  обёртками над clipboard/keyboard в монолите;
- удалить compatibility-обёртки при переносе `app_run` (шаг 8).

### Шаг 7. Трей и системная интеграция

Сделано:

- введён `ports/tray` (`dock`/`send`/`event`/`size`) и X11-адаптер
  `infrastructure/x11/tray` (`_NET_SYSTEM_TRAY_S0`,
  `_NET_SYSTEM_TRAY_OPCODE`, ClientMessage/ButtonPress/ConfigureNotify/
  Expose);
- `presentation/tray-indicator` хранит размер иконки и вызывает callback
  клика, не зная про X11; в приложении этот callback запускает
  конфигуратор;
- `infrastructure/system/desktop` владеет `goto_home`, desktop/autostart
  файлами, проверкой экземпляра и fork/exec конфигуратора;
- `main.c` больше не подключается через `#include` из монолита: это
  отдельная точка входа, которая вызывает `app_run`;
- добавлены наборы `desktop` (файлы, `$HOME`, запуск конфигуратора) и
  `tray` (X11-события и presentation-callback);
- ручной smoke: запуск приложения, команда, sendkeys, таймаут команды.

Осталось:

- перенести `app_run` и оставшиеся обёртки в `src/app/` (шаг 8).

### Шаг 8. Удаление монолита

Сделано:

- `erswitcher.c` удалён; его оставшийся код перенесён в
  `src/app/app.c`, объявления — в `src/app/app.h`;
- `main.c` больше не подключается через `#include`: это отдельный
  объект, который вызывает `app_run`;
- `tests/unit` линкуются с `src/app/app.o` и модулями; X11-заглушки
  подключаются из `test.h`, а не из монолита;
- Makefile собирает приложение из объектных файлов, без
  `erswitcher.o`;
- обновлены `doc/arch.md` и `doc/tests.md`;
- `make test` зелёный, ручной smoke: команда, `%i`/`%o`, таймаут,
  snippet, sendkeys, compose, `word translate`, SIGHUP.

Критерии готовности из раздела 11 выполнены: в `domain/` и
`use-cases/` нет X11/файлов/процессов/часов; `main.c` только запускает
`app_run`; сборка идёт из объектных файлов.

После каждого шага обязательны:

```sh
make test
```

и ручной smoke: `Pause`, `Control+Pause`, `Shift+Pause`, `Alt+Pause`,
`Alt+Shift+Pause`, compose, snippet, команда, клик по трею.

## 10. Тесты после разделения

- `domain` — чистые тесты без X11 и без файлов.
- `use-cases` — тесты с фейковыми `keyboard`, `clipboard`, `scheduler`,
  `command_runner`, `config_store`.
- `infrastructure/x11` — минимум интеграционных тестов; основная
  проверка — существующие функциональные тесты и ручной smoke.
- `tests/functional` (Tcl/Tk) не зависят от внутренней структуры C-кода
  и должны оставаться зелёными на всех шагах.
- После шага 8 в тестах нет `#include "../../erswitcher.c"`;
  `tests/unit/mock.c` подключается из `test.h` и содержит заглушки X11.

## 11. Критерии готовности

1. В `domain/` и `use-cases/` нет X11, файлового I/O, `fork/exec`,
   `system`, работы со временем.
2. `main.c` только собирает зависимости и запускает `app_run`.
3. `erswitcher.c` удалён; сборка идёт из объектных файлов.
4. Поведение и формат конфига не изменились.
5. `make test` зелёный; ручной smoke пройден.
6. Добавление Wayland-адаптера требует только новой реализации портов.

## 12. Вне рамок этой задачи

- Wayland.
- Поиск/замена в конфигураторе.
- Сниппеты в файлах, команды и sendkeys внутри сниппетов.
- Изменение формата конфига или поведения горячих клавиш.
