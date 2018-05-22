/**
 * \file hyscan-profile-common.h
 *
 * \brief Заголовочный файл модуля ф-й общего назначения, для работы с профилями.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#ifndef __HYSCAN_PROFILE_COMMON_H__
#define __HYSCAN_PROFILE_COMMON_H__

#include <glib.h>
#include "hyscan-api.h"

G_BEGIN_DECLS

/* Получает NULL-терминированный массив названий профилей. */
HYSCAN_API
gchar**   hyscan_profile_common_list_profiles       (const gchar   *profiles_path);

G_END_DECLS

#endif /* __HYSCAN_PROFILE_TYPES_H__ */