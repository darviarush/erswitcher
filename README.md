## NAME

**erswitcher** - _EN-RU Switcher_ - транслитератор текста и переключатель клавиатуры

## VERSION

0.0.2-beta

## SYNOPSIS

Комбинации клавиш:

* `Pause` - перевести последнее введённое с клавиатуры предложение;
* `Shift+Pause` - перевести выделенный текст.

## DESCIPTION

_EN-RU Switcher_ прослушивает клавиатуру и запоминает последний ввод.

При нажатии `Pause` он нажимает на клавишу `Backspace` и стирает последнее введённое предложение, а затем - вводит с клавиатуры его транслитерацию.

При нажатии `Shift+Pause` _EN-RU Switcher_ транслитерирует выделенный фрагмент текста. Причём выделение может даже быть в другом окне.

## INSTALL

С помощью git:

```sh
$ git clone https://github.com/darviarush/erswitcher.git
$ cd erswitcher
$ make erswitcher && make install
```

_EN-RU Switcher_ будет скомпилирован и установлен в каталог `~/.local/bin`.

Запустите его из командной строки:

```sh
$ ~/.local/bin/erswitcher &
```

## AUTOLOAD

Для того, чтобы erswitcher запускался вместе с иксами `make install` добавляет его в `~/.xinitrc`.

## AUTHOR

Yaroslav O. Kosmina <darviarush@mail.ru>.
