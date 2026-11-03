# Тесты

## Юнит-тесты

Каталог `tests/unit/`, фреймворк [Check](https://libcheck.github.io/check/).

`make test-unit` собирает и запускает два бинарника:

* `tests/unit/test` — старые тесты монолита. `erswitcher.c` подключается
  через `#include`, поэтому тесты видят и `static`-функции, и глобальное
  состояние. К нему линкуются `src/use_cases/**/*.o` и
  `src/infrastructure/x11/keyboard.o`, `src/infrastructure/x11/clipboard.o`,
  `src/infrastructure/x11/events.o` и
  `src/infrastructure/system/scheduler_time.o`; X11-вызовы подменены
  `tests/unit/mock.c`;
* `tests/unit/test_domain` — чистые доменные модули из `src/domain/`
  (`text`, `input_buffer`, `case_invert`, `compose`, `command_template`,
  `config`). Они линкуются как обычные объектные файлы и не требуют X11.

```sh
make test-unit
```

Большая часть доменных модулей уже используется монолитом (`unikey`,
`input_buffer`, `text`, `case_invert`, `compose`, `command_template`);
`config` пока покрыт только отдельными тестами. Клавиатурный X11-адаптер
проверяется набором `keyboard port` через тот же мок: преобразования
сканкодов и символов, переключение раскладки, инъекция клавиш и опрос
`keymap`/`keymap_saved`, `poll`/`poll_saved`. Сценарии `input`/`translate`
проверяются набором `use cases` с фейковым портом — без X11 и без
`erswitcher.c`.

Системный scheduler проверяется набором `scheduler port`: просроченный
таймер срабатывает один раз, будущий остаётся в очереди, контекст
передаётся каждому callback'у.

Буфер обмена проверяется набором `clipboard`: `set`/`clear`, отдача
собственного буфера, вызов callback при отсутствии владельца,
асинхронный `SelectionNotify` и сборка ответа INCR. Мок X11 умеет
возвращать заданного владельца Selection и заданные данные свойства.

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

Tk требует X11. Если `DISPLAY` не задан, `make test-functional` сам
попробует `xvfb-run -a`. Если нет ни дисплея, ни `xvfb-run`, прогон
завершается **ошибкой**: функциональные тесты не должны молча
пропускаться, иначе зелёный результат врёт. Явно через виртуальный
дисплей:

```sh
xvfb-run -a ./tests/functional/run.tcl
```

## Все тесты

```sh
make test
```
