# NAME

**erswitcher** _(EN—RU Switcher)_ — транслитератор текста и переключатель клавиатуры

# VERSION

0.0.3—beta

# SYNOPSIS

**erswitcher** — приложение Иксов _(X11)_.

Он реагирует на следующие комбинации клавиш:

* `Pause` — перевести последнее введённое с клавиатуры слово.
* `Control+Pause` — перевести последний введённый с клавиатуры текст.
* `Shift+Pause` — перевести выделенный текст.
* `Alt+Pause` — инвертировать регистр символов в выделенном тексте.
* `Alt+Shift+Pause` — инвертировать регистр символов в последнем введённом слове.
* `Alt+Control+Pause` — инвертировать регистр символов в последнем введённом тексте.

# DESCIPTION

_EN—RU Switcher_ прослушивает клавиатуру и запоминает последний ввод.

Если была нажата `Pause` он нажимает на клавишу `Backspace` и стирает последнее введённое слово, а затем — вводит с клавиатуры его транслитерацию.

При нажатии `Shift+Pause` _EN—RU Switcher_ копирует выделенный фрагмент текста в буфер обмена, транслитерирует его и вводит с клавиатуры.

# INSTALL

С помощью git:

```sh
$ git clone https://github.com/darviarush/erswitcher.git
$ cd erswitcher
$ make && make install && make lauch
```

_EN—RU Switcher_ будет скомпилирован, установлен в `~/.local/bin/erswitcher` и запущен из `~/.local/bin` в фоне.

# AUTOLOAD

При старте _EN—RU Switcher_ переписывает файлы в каталоге пользователя **~/.local/applications/erswitcher.desktop** и **~/.config/autostart/erswitcher.desktop**. Таким образом он сразу же запускается при старте рабочего стола поддерживающего стандарт _freedesktop.org_.

# DEPENDENCIES

Для сборки потребуются:

* gcc
* make
* X11
* xkbcommon
* Xtst

# AUTHOR

_Yaroslav O. Kosmina_ <darviarush@mail.ru>.

# LICENSE

⚖ **GPLv3**