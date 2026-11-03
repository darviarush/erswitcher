#ifndef __ERSWITCHER_DOMAIN_UNIKEY__H__
#define __ERSWITCHER_DOMAIN_UNIKEY__H__

/*
 * Доменная модель клавиши.
 *
 * unikey_t описывает физическую клавишу так, как её видит приложение:
 * сканкод, модификаторы и группу раскладки. Здесь нет ни X11, ни KeySym:
 * преобразования в X11-типы живут в infrastructure/x11/keyboard.
 *
 * Числовые значения модификаторов намеренно совпадают с X11-масками
 * (ShiftMask = 1 << 0 и т.д.), чтобы перенос из монолита не менял
 * поведение. Имена доменные: ERS_MOD_*.
 */

typedef struct {
    unsigned code:8;
    unsigned mods:8;
    unsigned group:8;
} unikey_t;

#define ERS_MOD_SHIFT       0x01
#define ERS_MOD_LOCK        0x02
#define ERS_MOD_CONTROL     0x04
#define ERS_MOD_ALT         0x08
#define ERS_MOD_NUM         0x10
#define ERS_MOD_META        0x20
#define ERS_MOD_SUPER       0x40
#define ERS_MOD_HYPER       0x80

// Остальные модификаторы, которые не участвуют в именовании
#define ERS_MOD_NONE        0x00

#endif /* __ERSWITCHER_DOMAIN_UNIKEY__H__ */
