#ifndef __ERSWITCHER_DOMAIN_COMMAND_TEMPLATE__H__
#define __ERSWITCHER_DOMAIN_COMMAND_TEMPLATE__H__

/*
 * Чистая подстановка в шаблон команды:
 *   %i — путь к входному файлу (выделенный текст);
 *   %o — путь к выходному файлу;
 *   %% — символ %
 *
 * Возвращает строку в malloc-памяти; освобождает вызывающий
 * NULL — если command == NULL или не хватило памяти
 * invalid (может быть NULL) — счётчик неизвестных подстановок вида %x
 */
char* command_template_expand(const char* command, const char* infile, const char* outfile, int* invalid);

#endif /* __ERSWITCHER_DOMAIN_COMMAND_TEMPLATE__H__ */
