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
                                             use_cases
                                                   │
                                                   ▼
                                               domain
```

Правила:

- `domain` не знает ни о X11, ни о файлах, ни о процессах, ни о времени.
- `use_cases` знает `domain` и **интерфейсы** из `ports`, но не их
  реализации.
- `infrastructure` реализует `ports`; здесь живут X11, `fork/exec`,
  файловая система и системные часы.
- `presentation` — трей и консольный вывод; использует `use_cases`/`ports`.
- `main.c` и `app/` — единственное место, где всё связывается вместе.
- В `domain/` и `use_cases/` запрещены `#include <X11/...>`, `fork`,
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
│   ├── input_buffer.h/.c           # word[], pos, keys_pressed: add/reset/from_space/backspace
│   ├── case_invert.h/.c            # инверсия ShiftMask у unikey_t
│   ├── compose.h/.c                # compose_t, поиск мнемоники в буфере
│   ├── config.h/.c                 # Config, Binding, ComposeEntry, Snippet, Command, Options
│   ├── command_template.h/.c       # чистая подстановка %i/%o/%%
│   └── text.h/.c                   # utf8_strlen, обход UTF-8 (перенос из монолита)
│
├── use_cases/                      # сценарии приложения
│   ├── input.h/.c                  # приём клавиши, Backspace, сброс при смене окна/мыши
│   ├── translate.h/.c              # word/text/selected translate и invertcase
│   ├── compose_input.h/.c          # применение compose и возврат курсора
│   ├── sendkeys.h/.c               # ввод строки через клавиатуру
│   ├── snippets.h/.c               # вставка сниппета через clipboard
│   ├── commands.h/.c               # координация [commands]
│   └── config_reload.h/.c          # загрузка/перезагрузка конфига, привязка функций
│
├── ports/                          # только объявления контрактов
│   ├── keyboard.h                  # состояние, раскладки, инъекция, модификаторы
│   ├── clipboard.h                 # асинхронные get/set, INCR
│   ├── scheduler.h                 # after/tick/таймауты
│   ├── event_loop.h                # ожидание и доставка X11-событий
│   ├── config_store.h              # чтение/запись/путь конфига
│   ├── command_runner.h            # запуск шелл-команд, timeout, %i/%o
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
│       ├── config_store_fs.h/.c    # ~/.config/erswitcher.conf
│       ├── command_runner_fork.h/.c # fork/system, временные файлы, timeout
│       ├── scheduler_time.h/.c     # gettimeofday/select
│       └── desktop.h/.c            # autostart/desktop, goto_home
│
├── presentation/
│   └── tray_indicator.h/.c         # отрисовка/состояние иконки, поверх tray-порта
│
└── app/
    ├── context.h/.c                # AppContext: состояние + порты
    └── app_run.h/.c                # главный цикл, composition root

main.c                              # только вызов app_run()
```

`tests/`:

```
tests/unit/          # domain и use_cases с фейковыми портами
tests/functional/    # существующие Tcl/Tk-тесты конфигуратора
```

## 5. Карта переноса монолита

| Что в `erswitcher.c` | Ответственность | Куда переносится |
|---|---|---|
| `unikey_t`, `KEY_TO_*`, `SYM_TO_*`, `unikey_to_keysym`, `keysym_to_unikey`, `keyboard_state`, `in_sym` | модель ключа и преобразования | `domain/unikey` + `ports/keyboard` + `infrastructure/x11/keyboard` |
| `init_keyboard`, `groups`, `group_ru`, `group_en`, `maybe_group`, `set_group`, `set_locks` | раскладки и локи | `ports/keyboard` + `infrastructure/x11/keyboard` |
| `press`, `send_mods`, `press_mods`, `send_key`, `press_key`, `active_codes`, `active_state`, `clear_active_mods`, `recover_active_mods` | инъекция клавиш и активные модификаторы | `infrastructure/x11/keyboard` |
| `keyboard_buf1/2`, `saved`, `keys`, `pos`, `word`, `keys_pressed`, `add_to_buffer`, `from_space` | кольцевой буфер ввода | `domain/input_buffer` + `use_cases/input` |
| `change_key` | классификация клавиши и диспетчеризация | `use_cases/input` (+ `ports/keyboard`) |
| `print_translate_buffer`, `print_invertcase_buffer`, `word_*`, `text_*`, `selection_*` | сценарии трансляции/регистра | `use_cases/translate` + `domain/case_invert` |
| `sendkeys`, `to_buffer`, `utf8_strlen`, `FOR_UTF8`, `press_key_multi` | ввод UTF-8 строки | `use_cases/sendkeys` + `domain/text` |
| `compose_map`, `compose`, `cursor_back`, `cursor_back__fn` | мнемоники и возврат курсора | `domain/compose` + `use_cases/compose_input` |
| `get_selection`, `set_clipboard`, `clipboard_*`, `selection_*`, `incr_*`, части `event_next` | буфер обмена и INCR | `ports/clipboard` + `infrastructure/x11/clipboard` |
| `shift_leftArrow`, `control_insert`, `shift_insert`, `copy_selection`, `insert_from_clipboard`, `copy_selection_next` | операции через clipboard | `use_cases` (translate/snippets/compose) + `ports/clipboard` |
| `timers`, `timers_size`, `after`, `timers_apply` | отложенные вызовы | `ports/scheduler` + `infrastructure/system/scheduler_time` |
| `event_next`, `event_delay`, `XQueryKeymap`-опрос из `app_run` | событийный цикл | `ports/event_loop` + `infrastructure/x11/events` + `app/app_run` |
| `load_config`, `keyfn*`, `compose_map*`, `signal_load_config` | конфиг и привязки | `domain/config` + `use_cases/config_reload` + `ports/config_store` |
| `run_command`, `command_fmt2ptr`, `on_run_command`, `_interval_run_command`, `curcommand*` | `[commands]` | `use_cases/commands` + `ports/command_runner` + `infrastructure/system/command_runner_fork` |
| `send_systray_message`, окно в `open_display`, `ButtonPress`, `ClientMessage`, `ConfigureNotify` | systray | `ports/tray` + `infrastructure/x11/tray` + `presentation/tray_indicator` |
| `goto_home`, `init_desktop`, `check_any_instance` | системная интеграция | `infrastructure/system/desktop` |
| `print_sym`, `get_event_type`, `DEBUG`, диагностика | логирование | `ports/logger` + `presentation/console` |
| `open_display`, атомы, `xerror` | X11-соединение | `infrastructure/x11/display` |
| `app_run`, `main.c` | запуск и главный цикл | `app/context`, `app/app_run`, `main.c` |
| `mock.c`, `test.h` | тестовая обвязка | `tests/unit` на фейковых портах |

`open_display()` придётся разделить: соединение и атомы — в `display`,
создание окна/докинг — в `infrastructure/x11/tray`.

## 6. Порты: важные контракты

Точные сигнатуры фиксируются при реализации, но контракты должны
учитывать следующее.

### keyboard

Нужны не только «нажать клавишу», но и:

- `state(code)` — текущие `mods`/`group`;
- `from_keysym(ks)` / `to_keysym(key)` — преобразования;
- `symbol(key)` — Unicode-символ для буфера и `[compose]`;
- `group()` / `set_group(group)`;
- `set_locks(mods)`;
- `press(key)` / `press_multi(key, n)`;
- `save_active()` / `restore_active()` — сохранение зажатых модификаторов.

Доменные модификаторы (`ERS_MOD_SHIFT`, `ERS_MOD_LOCK`, …) не должны
называться `ShiftMask`/`LockMask`. На первом этапе их числовые значения
можно оставить совпадающими с X11, чтобы снизить риск.

### clipboard

Контракт асинхронный:

- `get(selection, callback, context)` — результат может прийти позже, в
  том числе через INCR;
- `set(data, len, target)` — владение памятью передаётся порту;
- ошибка/пустой буфер — тоже вызов callback (как сейчас `fn(strdup(""), 0, …)`).

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
  «что делать с изменением» — в `use_cases/input`.

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
    InputBuffer input;             // word[], pos, keys_pressed
    int current_win;
    int need_load_config;

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

- `InputBuffer` владеет `word[]`, `pos`, `keys_pressed`;
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
- `domain/input_buffer`;
- `domain/case_invert`;
- `domain/compose` (поиск, без вставки);
- `domain/text` (UTF-8);
- `domain/command_template`;
- `domain/config` (модель и разбор значений без I/O и X11).

Монолит продолжает работать; новые модули используются через адаптеры
или пока только тестами.

### Шаг 2. Клавиатура

- Ввести `ports/keyboard`.
- Перенести Xkb/XTest-код в `infrastructure/x11/keyboard`.
- Обернуть существующие функции адаптером, не меняя вызовы в монолите.
- Перенести `use_cases/input` и `use_cases/translate` (вариант А).

### Шаг 3. Clipboard

- Ввести асинхронный `ports/clipboard`.
- Перенести Selection/INCR в `infrastructure/x11/clipboard`.
- Перевести `translate(selected)`, snippets, compose на порт.

### Шаг 4. Время и события

- `ports/scheduler` + системная реализация;
- `ports/event_loop` + X11-реализация;
- главный цикл постепенно переезжает в `app/app_run`.

### Шаг 5. Конфиг

- `ports/config_store`;
- `use_cases/config_reload`;
- привязки, compose, snippets, commands — из модели, без function pointer
  и sentinel-значений;
- SIGHUP через флаг.

### Шаг 6. Команды, snippets, sendkeys

- `ports/command_runner` и fork-реализация;
- `use_cases/commands`, `use_cases/snippets`, `use_cases/sendkeys`;
- сохранить таймауты и временные файлы.

### Шаг 7. Трей и системная интеграция

- `ports/tray`, X11-реализация, `presentation/tray_indicator`;
- `infrastructure/system/desktop`;
- composition root в `main.c`.

### Шаг 8. Удаление монолита

- Удалить `erswitcher.c` и include-трюк `main.c`.
- Переписать `tests/unit` на линковку модулей и фейковые порты.
- Обновить Makefile, `doc/arch.md` и `doc/tests.md`.

После каждого шага обязательны:

```sh
make test
```

и ручной smoke: `Pause`, `Control+Pause`, `Shift+Pause`, `Alt+Pause`,
`Alt+Shift+Pause`, compose, snippet, команда, клик по трею.

## 10. Тесты после разделения

- `domain` — чистые тесты без X11 и без файлов.
- `use_cases` — тесты с фейковыми `keyboard`, `clipboard`, `scheduler`,
  `command_runner`, `config_store`.
- `infrastructure/x11` — минимум интеграционных тестов; основная
  проверка — существующие функциональные тесты и ручной smoke.
- `tests/functional` (Tcl/Tk) не зависят от внутренней структуры C-кода
  и должны оставаться зелёными на всех шагах.
- После шага 8 в тестах не должно быть
  `#include "../../erswitcher.c"`; текущий `tests/unit/mock.c`
  распадается на наборы фейковых портов.

## 11. Критерии готовности

1. В `domain/` и `use_cases/` нет X11, файлового I/O, `fork/exec`,
   `system`, работы со временем.
2. `main.c` только собирает зависимости и запускает `app_run`.
3. `erswitcher.c` удалён; сборка идёт из объектных файлов.
4. Поведение и формат конфига не изменились.
5. `make test` зелёный; ручной smoke пройден.
6. Добавление Wayland-адаптера требует только новой реализации портов.

## 12. Вне рамок этой задачи

- Wayland.
- Переключение языков через функцию `switch language`.
- Поиск/замена в конфигураторе.
- Сниппеты в файлах, команды и sendkeys внутри сниппетов.
- Переход на Criterion и ASan (отдельный пункт TASK).
- Изменение формата конфига или поведения горячих клавиш.
