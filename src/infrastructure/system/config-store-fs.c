#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config-store-fs.h"

#define CONFIG_STORE_PATH ".config/erswitcher.conf"
#define CONFIG_STORE_ETC "/usr/share/erswitcher.conf.simple"

static const char default_config_text[] =
    "#####################################################################\n"
    "#              Конфигурацонный файл erswitcher-а                    #\n"
    "#                                                                   #\n"
    "# Автор: Ярослав О. Косьмина                                        #\n"
    "# Сайт:  https://github.com/darviarush/erswitcher                   #\n"
    "#                                                                   #\n"
    "# Комбинации клавишь указываются в английской раскладке             #\n"
    "#   и действуют во всех раскладках                                  #\n"
    "# Названия символов взяты из Иксов. Посмотреть их можно запустив    #\n"
    "#   ~/local/bin/erswitcher в консоли и введя комбинацию на          #\n"
    "#   клавиатуре                                                      #\n"
    "#####################################################################\n"
    "\n"
    "[functions]\n"
    "# Транслитерация последнего введённого слова\n"
    "Pause=word translate\n"
    "# Транслитерация последнего ввода\n"
    "Control+Pause=text translate\n"
    "# Транслитерация выделенного участка или буфера обмена\n"
    "Shift+Break=selected translate\n"
    "# Изменение регистра последнего введённого слова\n"
    "Alt+Shift+Break=word invertcase\n"
    "# Изменение регистра последнего ввода\n"
    "Alt+Control+Pause=text invertcase\n"
    "# Изменение регистра последнего введённого слова\n"
    "Alt+Pause=selected invertcase\n"
    "# Транслитерация последнего введённого символа в дополнительные символы\n"
    "KP_Begin=compose\n"
    "Scroll_Lock=compose\n"
    "\n"
    "[options]\n"
    "# Задержка между вызовами клавиатуры в секундах\n"
    "loop_delay=0,01\n"
    "# Задержка между нажатиями клавишь в секундах\n"
    "delay=0,01\n"
    "# Переключение языка: OSSwitchLanguage — через X11/XKB,\n"
    "# Caps_Lock — эмуляция CapsLock (для VM и систем с таким shortcut)\n"
    "switch_language=Caps_Lock\n"
    "\n"
    "[compose]\n"
    "# Замены (мнемоники) - при нажатии на ScrollLock или меню (центральная клавиша на расширенной клавиатуре) заменяет последний введённый символ на соответствующий\n"
    "\n"
    "# Дополнительные символы кириллицы для украинского и белорусского алфавитов\n"
    "ы=і\n"
    "Ы=І\n"
    "ъ=ї\n"
    "Ъ=Ї\n"
    "щ=ў\n"
    "Щ=Ў\n"
    "э=є\n"
    "Э=Є\n"
    "г=ґ\n"
    "Г=Ґ\n"
    "ё=’\n"
    "Ё=₴\n"
    "\n"
    "# Кавычки\n"
    "//=«\n"
    "\\\\=»\n"
    ",,=„\n"
    "..=”\n"
    "``=“\n"
    ",=‘\n"
    ".=’\n"
    "# Тире - длинное, среднее и цифровое\n"
    "-=—\n"
    "-,=–\n"
    "-.=‒\n"
    "# Пунктуация\n"
    "...=…\n"
    "p=§\n"
    "P=§§\n"
    ":-=÷\n"
    "-//-=―〃―\n"
    "***=⁂\n"
    "x=⌘\n"
    "?\?=؟\n"
    "?=¿\n"
    "!=¡\n"
    "n=¶\n"
    "# Неразрывный пробел (\u00A0, &nbsp;)\n"
    " = \n"
    "# Утопающий и кричащий о помощи смайлик\n"
    "хелп=‿︵‿ヽ(°□° )ノ︵‿︵\n"
    "# Лежащий на боку и наблюдающий за чем-то смайлик\n"
    "вау=∠( ᐛ 」∠)＿\n"
    "# Ветер унёс зонтик бедняги во время дождя\n"
    "нет=｀、ヽ(ノ＞＜)ノ ｀、ヽ｀☂ヽ\n"
    "# Медвед\n"
    "миш=ʕ ᵔᴥᵔ ʔ\n"
    "# Пожималкин\n"
    "хз=¯\\_(ツ)_/¯\n"
    "# Смайлы\n"
    ")=ヅ\n"
    ";)=ゾ\n"
    ":)=ジ\n"
    "# Математические символы\n"
    "+-=±\n"
    "++=∑\n"
    "**=∏\n"
    "*=×\n"
    "q=√\n"
    "<\\==≤\n"
    ">\\==≥\n"
    "2=½\n"
    "3=⅓\n"
    "4=¼\n"
    "5=⅕\n"
    "6=⅙\n"
    "7=⅐\n"
    "# Cтрелочки\n"
    "->=→\n"
    "-->=⟶\n"
    "<-=←\n"
    "<--=⟵\n"
    "t=// TODO: \n"
    "T=# TODO: \n"
    // ▄︻┻┳═ㅤㅤㅤ’’\̵͇ \з=(• - ̪●)=ε/̵͇ /’ ’ ㅤㅤㅤ═┳┻︻▄
    // 0:47 ━━●━━━━━━━━━━━ 3:30
    // ⇆ㅤㅤㅤㅤ◁ㅤㅤ❚❚ㅤㅤ▷ㅤㅤㅤㅤ↻
    "## SQL\n"
    "sele=SELECT *\\nFROM \\^ p\\nWHERE \\nORDER BY p.id DESC\\nLIMIT 100\\n\n"
    "\n"
    "[sendkeys]\n"
    "# Шаблоны - вводятся с клавиатуры, их символы должны быть в одной из действующих раскладок\n"
    "Super+Pause=Готово.\n"
    "\n"
    "[snippets]\n"
    "# Сниппеты - вводятся через буфер обмена (clipboard), могут содержать любые символы\n"
    "Super+Shift+Break=Готово.\n"
    "\n"
    "[commands]\n"
    "# Команды операционной системы. Выполняются в оболочке шелла\n"
    "Alt+Control+Shift+Break=kate ~/.config/erswitcher.conf && killall -HUP erswitcher\n"
    "Alt+Control+d=date > %o\n"
    "Alt+Control+t=trans ru:en -b < %i > %o\n"
    "Alt+Control+y=trans en:ru -b < %i > %o\n";

config_store_fs_t er_config_store = {.path = CONFIG_STORE_PATH};

static config_store_fs_t* state_of(config_store_port_t* port) {
    return (config_store_fs_t*) port->context;
}

static const char* path_impl(config_store_port_t* port) {
    return state_of(port)->path;
}

static char* read_impl(config_store_port_t* port) {
    const char* path = state_of(port)->path;

    FILE* f = fopen(path, "rb");
    if(!f) return NULL;

    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long size = ftell(f);
    if(size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char* data = malloc((size_t) size + 1);
    if(!data) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(data, 1, (size_t) size, f);
    data[read] = '\0';
    fclose(f);
    return data;
}

static int write_default(FILE* f) {
    FILE* etc = fopen(CONFIG_STORE_ETC, "rb");
    if(etc) {
        char buffer[4096];
        size_t n;
        while((n = fread(buffer, 1, sizeof(buffer), etc)) > 0) {
            if(fwrite(buffer, 1, n, f) != n) {
                fclose(etc);
                return 0;
            }
        }
        int ok = !ferror(etc);
        fclose(etc);
        if(!ok) return 0;
    } else {
        if(fputs(default_config_text, f) == EOF) return 0;
    }

    return 1;
}

static int ensure_default_impl(config_store_port_t* port) {
    config_store_fs_t* state = state_of(port);

    if(access(state->path, F_OK) == 0) return 1;

    // Каталог .config может отсутствовать — создаём, как старый load_config()
    mkdir(".config", 0755);

    FILE* f = fopen(state->path, "wb");
    if(!f) return 0;

    int ok = write_default(f);
    if(fclose(f) != 0) ok = 0;
    return ok;
}

void config_store_fs_port(config_store_port_t* port, config_store_fs_t* state) {
    memset(port, 0, sizeof(*port));
    port->context = state;
    port->path = path_impl;
    port->read = read_impl;
    port->ensure_default = ensure_default_impl;
}
