# Пакетирование и публикация

Документ описывает общие правила для всех пакетных форматов
**erswitcher**. Частные инструкции лежат в `doc/packaging/<format>.md`,
файлы упаковки — в `packaging/<format>/`.

## 1. Текущий статус

| Формат | Статус | Каталог | Команда/артефакт | Документ |
|---|---|---|---|---|
| AppImage | готово | `packaging/appimage/` | `make appimage`, `make release` | [`appimage.md`](packaging/appimage.md) |
| GitHub Releases | готово для AppImage | — | `make release`, `gh release ...` | [`appimage.md`](packaging/appimage.md) |
| AUR | запланировано | `packaging/aur/` (нужно создать) | `PKGBUILD` | пока нет |
| Flatpak | запланировано | `packaging/flatpak/` | `flatpak-builder` | пока нет |
| Snap | запланировано | `packaging/snap/` | `snapcraft` | пока нет |
| ALT Linux | запланировано | `packaging/alt/` | `.spec`/gear | пока нет |
| Astra Linux | запланировано | `packaging/astra/` | `.deb`/репозиторий | пока нет |
| RED OS | запланировано | `packaging/redos/` | `.rpm`/`.spec` | пока нет |
| Установка через `install` | частично | `Makefile` (`install`) | `make install` | пока нет |
| Единая публикация | запланировано | — | `make publish`, `RELEASE.md` | пока нет |

Корневой `PKGBUILD` — исторический файл. Его нужно перенести в
`packaging/aur/`, обновить версию и зависимости; сейчас он не является
готовым AUR-пакетом.

## 2. Общие правила

1. Версия проекта берётся из `README.md` (раздел `# VERSION`).
   Не дублируйте её в каждом пакете без необходимости; в Makefile уже
   есть `VERSION`, которую можно переопределить.
2. Метаданные и шаблоны формата лежат в `packaging/<format>/`.
   Там не должно быть собранных бинарников.
3. Собранные артефакты складываются в `dist/` и не коммитятся.
4. Документация формата лежит в `doc/packaging/<format>.md`.
5. Для каждого формата нужны:
   - desktop-файл с валидными категориями;
   - иконка не меньше 256×256;
   - корректный `Exec`, указывающий на установленный бинарник;
   - зависимости времени выполнения.
6. Перед публикацией проверяются `make test`, `make format-check` и
   ручной smoke-тест.
7. GitHub Releases создаются через `gh`; тег имеет вид `v<version>`.
8. Не храните токены и пароли в репозитории: `gh` использует
   `gh auth login`.

## 3. Зависимости

Полный список программ: [`Dependencies`](../Dependencies).
Проверить окружение: `make check-deps`.

Сборка:

- `gcc`, `make`, `pkg-config`/`pkgconf`;
- заголовки и библиотеки X11, Xkbcommon, XTest;
- Tcl/Tk — для конфигуратора и функциональных тестов.

Разработка и тесты:

- `git`;
- `criterion` — юнит-тесты и стектрейсы;
- `clang`/`clang-format` — форматирование;
- `cppcheck` — статический анализ;
- `xorg-server-xvfb` — `xvfb-run` для функциональных тестов;
- `xdotool` — ручной smoke-тест;
- `desktop-file-utils` — проверка `.desktop`.

Упаковка и релизы:

- `appimagetool` (`appimagetool-bin` из AUR);
- `gh` (GitHub CLI);
- `curl` — запасной способ получить `appimagetool`;
- `librsvg`/`rsvg-convert` — опционально для иконки.

Время выполнения:

- `libX11`, `libxkbcommon`, `libXtst`;
- `wish`/Tk — если нужен конфигуратор;
- системный трей, поддерживающий `_NET_SYSTEM_TRAY_S0`.

Приложение работает через X11. Wayland-пакет должен будет добавить
отдельный адаптер портов, а не просто переупаковать текущий бинарник.

## 4. Общие пути и поведение

- конфиг: `~/.config/erswitcher.conf`;
- autostart: `~/.config/autostart/erswitcher.desktop`;
- пользовательский desktop-файл:
  `~/.local/share/applications/erswitcher.desktop`;
- установка из исходников: `~/.local/bin/erswitcher` и
  `~/.local/bin/erswitcher-configurator.tcl`;
- версия конфига и формат секций не должны меняться при упаковке.

## 5. Общие ресурсы

Сейчас иконка и desktop-файл AppImage лежат в `packaging/appimage/`.
Когда появится второй графический формат, общие ресурсы стоит вынести в
`packaging/common/`, чтобы не дублировать `erswitcher.png`,
`erswitcher.svg` и desktop-файл.

## 6. Релизный процесс

Текущий сценарий для AppImage:

```sh
make appimage          # dist/erswitcher-<version>-<arch>.AppImage
gh auth login          # один раз
git push origin master # сначала отправить коммиты
make release           # создать/обновить GitHub Release и загрузить AppImage
```

`make release` использует тег `v$(VERSION)`. Переопределение:

```sh
make release VERSION=7.3 RELEASE_TAG=v7.3
```

В будущем `make publish` должен:

1. собрать все доступные форматы;
2. опубликовать их в соответствующие репозитории;
3. записать в начало `RELEASE.md`, где и что опубликовано;
4. не считать успехом частично выполненную публикацию.

До реализации `make publish` публикация выполняется пошагово, отдельно
для каждого формата.

## 7. Чек-лист нового пакета

- [ ] создать `packaging/<format>/` с метаданными;
- [ ] не хардкодить версию, использовать общий `VERSION`;
- [ ] проверить desktop-файл и иконку;
- [ ] описать зависимости сборки и времени выполнения;
- [ ] собрать пакет в `dist/`;
- [ ] установить и запустить в чистом окружении/контейнере;
- [ ] проверить конфиг, autostart и конфигуратор;
- [ ] добавить `doc/packaging/<format>.md`;
- [ ] обновить таблицу в этом документе и `TASK.md`;
- [ ] добавить публикацию в `make publish` или отдельную команду.
