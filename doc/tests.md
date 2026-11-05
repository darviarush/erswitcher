# Тесты

## Юнит-тесты

Каталог `tests/unit/`, фреймворк [Criterion](https://criterion.readthedocs.io/).
Тесты собираются с AddressSanitizer и `-g`, Criterion даёт цветной вывод
и стектрейсы падений.

`make test-unit` собирает и запускает два бинарника:

* `tests/unit/test` — тесты приложения и портов. Исходники приложения и
  адаптеров компилируются в этот бинарник напрямую с ASan; `test.h`
  подключает `tests/unit/mock.c` с заглушками X11; монолит
  `erswitcher.c` больше не используется;
* `tests/unit/test-domain` — чистые доменные модули из `src/domain/`
  (`text`, `input_buffer`, `case_invert`, `compose`, `command_template`,
  `config`). Они компилируются в бинарник напрямую и не требуют X11.

```sh
make test-unit
make memcheck
make coverage
```

Доменные модули используются приложением (`unikey`, `input_buffer`,
`text`, `case_invert`, `compose`, `command_template`, `config`). Клавиатурный X11-адаптер
проверяется набором `keyboard port` через тот же мок: преобразования
сканкодов и символов, переключение раскладки, инъекция клавиш и опрос
`keymap`/`keymap_saved`, `poll`/`poll_saved`. Сценарии `input`/`translate`
проверяются набором `use cases` с фейковым портом — без X11 и без
слоя приложения.

Системный scheduler проверяется набором `scheduler port`: просроченный
таймер срабатывает один раз, будущий остаётся в очереди, контекст
передаётся каждому callback'у. Загрузка конфига проверяется набором
`config reload`: модель `[functions]`/`[options]`/`[compose]`/
`[sendkeys]`/`[snippets]`/`[commands]`, сохранение base-опций,
escape-последовательности и предупреждения о битых строках.

Сценарий `commands` проверяется набором `[commands]` на фейковых
`command_runner`, `clipboard` и `scheduler`: `%i`/`%o`, вставка вывода,
таймаут, пустая команда и запрет параллельного запуска. `sendkeys` и
`snippets` проверяются через app-API и X11-мок; их реализация находится
в `src/use-cases/`.

Системная интеграция проверяется наборами `desktop` и `tray`: создание
`.desktop`/autostart-файлов без перезаписи, `desktop_goto_home`, запуск
конфигуратора по fork/exec; раскладка X11-событий трея в `tray_event_t`
и callback клика в `presentation/tray-indicator`.

Буфер обмена проверяется набором `clipboard`: `set`/`clear`, отдача
собственного буфера, вызов callback при отсутствии владельца,
асинхронный `SelectionNotify` и сборка ответа INCR. Мок X11 умеет
возвращать заданного владельца Selection и заданные данные свойства.

Покрытие собирается отдельными объектами с `--coverage`:

```sh
make coverage        # сводка исходников + build/coverage-html/index.html
make coverage-text   # только цветная сводка исходников
make coverage-html   # только HTML по исходникам
make coverage-clean  # удалить артефакты
```

`coverage-text` показывает только `TOTAL` по `src/`: ветви и строки,
с цветовой шкалой и полосой. Тесты и моки в этот отчёт не входят.
Полный отчёт: `make coverage COVERAGE_FILTER='.*'`. Управление цветом:
`COVERAGE_COLOR=always` или `COVERAGE_COLOR=never`.

## Динамический анализ и фаззинг

В тестах используются три санитайзера. Они дополняют друг друга:

### ASan — AddressSanitizer

ASan ищет ошибки работы с памятью:

* выход за границы массива или объекта;
* use-after-free;
* double-free;
* порчу стека и глобальных объектов;
* чтение или запись по некорректному указателю.

При ошибке тест немедленно падает и печатает стек вызовов. ASan включён
в тестовую сборку по умолчанию.

### LSan — LeakSanitizer

LSan ищет утечки памяти при завершении процесса: выделенные блоки, на
которые больше нет ссылок и которые не были освобождены. На Linux LSan,
как правило, работает вместе с ASan; `make memcheck` явно включает
проверку утечек через `ASAN_OPTIONS=detect_leaks=1`.

LSan показывает именно утечки, а не логические ошибки владения. Поэтому
тесты, которые проверяют, кто освобождает строку или буфер, всё равно
нужны.

### UBSan — UndefinedBehaviorSanitizer

UBSan ищет неопределённое поведение, которое не обязательно связано с
памятью:

* переполнение знаковых целых;
* недопустимые сдвиги;
* невыровненные обращения;
* некорректные значения `enum`;
* некоторые ошибки преобразования указателей.

В `make test-sanitize` UBSan включён вместе с ASan и запускается с
`-fno-sanitize-recover=all`: первое неопределённое поведение становится
ошибкой теста, а не предупреждением.

Санитайзеры проверяют только выполненные пути. Они не доказывают
отсутствие ошибок, поэтому дополнительно используются фаззинг, Valgrind
и статический анализ.

`make test-unit` запускает Criterion-тесты с ASan.
`make memcheck` — полный ASan/LSan-прогон с явной проверкой утечек.
`make test-sanitize` добавляет к юнит-тестам UBSan.

Valgrind остаётся отдельной проверкой для подробной диагностики утечек,
неинициализированной памяти и неверных освобождений. Criterion
изолирует каждый тест в отдельном процессе, поэтому полный прогон
Valgrind слишком долгий; используется фильтр:

```sh
make valgrind VALGRIND_FILTER='commands/*'
```

Фаззеры находятся в `tests/fuzz/`:

```sh
make fuzz-build
make fuzz FUZZ_TARGET=text FUZZ_SECONDS=60
make fuzz-all FUZZ_SECONDS=60
make fuzz-clean
```

Цели: `fuzz-text`, `fuzz-config`, `fuzz-command`, `fuzz-input` и
`fuzz-compose`. Начальный корпус лежит в `tests/fuzz/corpus/`, найденные
регрессии — в `tests/fuzz/regressions/`. Артефакты сборки и падения
пишутся в `build/fuzz/`. Как читать вывод и что делать при падении —
в [tests/fuzzing.md](tests/fuzzing.md).

## Функциональные тесты

Каталог `tests/functional/`, фреймворк `tcltest` и настоящие виджеты Tk.
Проверяется окно конфигуратора `erswitcher-configurator.tcl`:

* раскладка окна, скроллбары, опции текстового поля;
* загрузка `erswitcher.conf` и закладки по секциям;
* раскраска секций, комментариев, `=`, ошибок, дубликатов ключей и
  escape-последовательностей `\t`, `\n`, `\^`;
* события X11 через `event generate`: выделение по `Control+KeyRelease+a`,
  сохранение правки по `KeyRelease` и переход по закладке;
* файловые помощники `exists_file`, `read_file`, `write_file`.

Тесты не трогают конфиг пользователя: `run.tcl` создаёт песочницу,
подменяет `HOME` и пишет туда тестовый `erswitcher.conf`. На время проверки
сохранения `exec` подменяется заглушкой, чтобы не посылать `SIGHUP`
настоящему `erswitcher`.

```sh
make test-functional
```

Tk требует X11. По умолчанию `make test-functional` запускает тесты
через `xvfb-run -a`, даже если `DISPLAY` задан: окно не появляется на
рабочем столе и не перехватывает ввод. Для запуска на текущем дисплее:

```sh
make test-functional FUNCTIONAL_HEADLESS=0
```

Если `xvfb-run` не установлен, используется текущий `DISPLAY`; на macOS
Tk работает напрямую. Для headless-запуска на Linux установите
`xorg-server-xvfb`. Если выполнить тесты нельзя, прогон завершается
**ошибкой**, а не пропуском: функциональные тесты не должны молча
исчезать из зелёного результата.

## Все тесты

```sh
make test
```

`make test` запускает `test-unit` и `test-functional`, но не запускает
фаззинг, sanitizer-прогон с UBSan и покрытие.

## Сводная таблица команд

| make-команда | Для чего |
|---|---|
| `make test-unit` | Юнит-тесты Criterion: приложение, порты и домен; ASan включён |
| `make test-sanitize` | Те же юнит-тесты с ASan, LSan и UBSan; UBSan останавливает тест при первом UB |
| `make test-functional` | Функциональные тесты окна конфигуратора на Tcl/Tk; по умолчанию через `xvfb-run` |
| `make test` | Полный обычный набор: `test-unit` + `test-functional` |
| `make memcheck` | Полный ASan/LSan-прогон юнит-тестов с проверкой утечек |
| `make valgrind VALGRIND_FILTER='commands/*'` | Выбранные тесты под Valgrind: утечки, неинициализированная память, неверные освобождения |
| `make coverage` | Прогон с покрытием, цветная сводка по `src/` и HTML в `build/coverage-html/index.html` |
| `make coverage-text` | Только цветная сводка покрытия по `src/` |
| `make coverage-html` | Только HTML-отчёт по `src/` |
| `make coverage-clean` | Удалить артефакты покрытия |
| `make fuzz-build` | Собрать libFuzzer-бинарники для всех целей |
| `make fuzz FUZZ_TARGET=text FUZZ_SECONDS=60` | Запустить одну цель фаззинга на указанное время |
| `make fuzz-all FUZZ_SECONDS=60` | Запустить все пять целей последовательно |
| `make fuzz-clean` | Удалить бинарники, корпус и артефакты фаззинга |
| `make cppcheck` | Статический анализ cppcheck |
| `make format-check` | Проверить форматирование C/H без изменений |
