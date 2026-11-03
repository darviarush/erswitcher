# AppImage и GitHub Releases

> Общие правила упаковки: [`../packaging.md`](../packaging.md).

## 1. Что собирается

`make appimage` создаёт переносимый AppImage:

```text
dist/erswitcher-<version>-<arch>.AppImage
```

Версия берётся из `README.md` (раздел `# VERSION`), архитектура — из
`uname -m`. Оба значения можно переопределить:

```sh
make appimage VERSION=7.3 APPIMAGE_ARCH=x86_64
```

## 2. Файлы упаковки

Всё, что нужно для AppDir, лежит в `packaging/appimage/`:

- `AppRun` — точка входа AppImage, запускает `usr/bin/erswitcher` из
  `APPDIR`;
- `erswitcher.desktop` — desktop-файл;
- `erswitcher.svg` — исходник иконки;
- `erswitcher.png` — иконка 256×256 для AppDir и `.DirIcon`;
- `README.md` — краткая памятка по каталогу.

Makefile собирает из них staging-каталог `build/AppImage/AppDir` и
вызывает `appimagetool`.

## 3. Зависимости сборки

Нужен `appimagetool`. В Arch/Manjaro отдельного пакета в официальных
репозиториях может не быть, поэтому проще взять официальный continuous
AppImage:

```sh
mkdir -p ~/.local/bin
curl -L -o ~/.local/bin/appimagetool \
  https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x ~/.local/bin/appimagetool
```

Если в вашем дистрибутиве есть пакет `appimagetool`, можно установить
его штатным пакетным менеджером.

Makefile запускает `appimagetool` с `APPIMAGE_EXTRACT_AND_RUN=1`, чтобы
сборка работала и без FUSE. Первый запуск может скачать runtime AppImage
с GitHub.

Если инструмент лежит не в `PATH`:

```sh
make appimage APPIMAGE_TOOL=/путь/к/appimagetool
```

## 4. Сборка и проверка

```sh
make appimage
```

Проверка запуска:

```sh
chmod +x dist/erswitcher-7.2-x86_64.AppImage
./dist/erswitcher-7.2-x86_64.AppImage
```

Если в системе нет FUSE:

```sh
APPIMAGE_EXTRACT_AND_RUN=1 ./dist/erswitcher-7.2-x86_64.AppImage
```

Staging-каталог можно удалить отдельно:

```sh
make appimage-clean
```

## 5. Особенности AppImage

При запуске AppImage runtime выставляет `APPIMAGE` и `APPDIR`.

- `APPIMAGE` используется при создании autostart/desktop-файлов, чтобы
  `Exec=` указывал на сам AppImage, а не на временный путь внутри
  смонтированного образа.
- `APPDIR` используется для запуска встроенного
  `erswitcher-configurator.tcl` по клику на иконке в трее.
- Конфиг по-прежнему создаётся в `~/.config/erswitcher.conf`; AppImage не
  хранит пользовательские данные внутри образа.

## 6. Публикация в GitHub Releases через gh

Да, GitHub CLI умеет создавать релизы и загружать файлы:

```sh
gh auth login          # один раз
make release
```

`make release`:

1. собирает AppImage;
2. проверяет, что `gh` установлен и авторизован;
3. если релиз с тегом `v<version>` уже есть — загружает AppImage через
   `gh release upload --clobber`;
4. если релиза нет — создаёт его через `gh release create` с
   автоматическими заметками (`--generate-notes`) и прикладывает AppImage.

По умолчанию тег — `v$(VERSION)`, например `v7.2`. Можно задать явно:

```sh
make release VERSION=7.3 RELEASE_TAG=v7.3
```

Перед публикацией убедитесь, что нужные коммиты уже отправлены в GitHub:
`gh release create` создаёт тег на удалённом репозитории. Посмотреть
команды без выполнения:

```sh
make -n release
```

Проверить существующий релиз:

```sh
gh release view v7.2
```

Добавить ещё один файл в уже существующий релиз:

```sh
gh release upload v7.2 путь/к/файлу
```

## 7. Ограничения

- AppImage собирается под текущую архитектуру; для другой нужен
  соответствующий `appimagetool` и `APPIMAGE_ARCH`.
- В образ не включается `erswitcher.conf`: приложение создаёт конфиг из
  встроенного текста или `/usr/share/erswitcher.conf.simple`.
- Иконка в трее и иконка приложения — разные вещи: первая рисуется
  приложением, вторая берётся из `packaging/appimage/erswitcher.png`.
