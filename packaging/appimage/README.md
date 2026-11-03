# AppImage packaging

Files used by `make appimage`:

- `AppRun` — entry point, запускает `usr/bin/erswitcher` из `APPDIR`;
- `erswitcher.desktop` — desktop-файл AppImage;
- `erswitcher.svg` — исходник иконки;
- `erswitcher.png` — иконка 256×256 для AppDir и `.DirIcon`.

Сборка:

```sh
make appimage
```

Результат: `dist/erswitcher-<version>-<arch>.AppImage`.

Публикация через GitHub CLI:

```sh
make release
```

Подробности: [`doc/packaging/appimage.md`](../../doc/packaging/appimage.md).
