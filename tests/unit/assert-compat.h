#ifndef __ERSWITCHER_TESTS_ASSERT_COMPAT__H__
#define __ERSWITCHER_TESTS_ASSERT_COMPAT__H__

#include <criterion/criterion.h>

/*
 * Совместимость старых Check-ассертов с Criterion
 *
 * Тела тестов писались с ck_assert_*; чтобы миграция не смешивала
 * изменения логики и тестового фреймворка, имена сохранены как тонкие
 * обёртки над cr_assert_*
 */

#define ck_assert(...) cr_assert(__VA_ARGS__)
#define ck_assert_msg(...) cr_assert(__VA_ARGS__)
#define ck_assert_int_eq(...) cr_assert_eq(__VA_ARGS__)
#define ck_assert_int_ge(...) cr_assert_geq(__VA_ARGS__)
#define ck_assert_int_gt(...) cr_assert_gt(__VA_ARGS__)
#define ck_assert_double_eq_tol(...) cr_assert_float_eq(__VA_ARGS__)
#define ck_assert_ptr_eq(...) cr_assert_eq(__VA_ARGS__)
#define ck_assert_ptr_null(...) cr_assert_null(__VA_ARGS__)
#define ck_assert_ptr_nonnull(...) cr_assert_not_null(__VA_ARGS__)
#define ck_assert_str_eq(...) cr_assert_str_eq(__VA_ARGS__)

#endif /* __ERSWITCHER_TESTS_ASSERT_COMPAT__H__ */
