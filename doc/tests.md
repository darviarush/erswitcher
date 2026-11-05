# Тесты

Стратегия проверок и место каждого инструмента описаны в
[test-metodology.md](test-metodology.md).

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
make coverage        # сводка исходников + coverage.html
make coverage-text   # только цветная сводка исходников
make coverage-html   # только HTML по исходникам
make coverage-clean  # удалить артефакты
```

`coverage-text` показывает только `TOTAL` по `src/`: ветви и строки,
с цветовой шкалой и полосой. Тесты и моки в этот отчёт не входят.
Полный отчёт: `make coverage COVERAGE_FILTER='.*'`. Управление цветом:
`COVERAGE_COLOR=always` или `COVERAGE_COLOR=never`.

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
