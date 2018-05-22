/**
 * \file hyscan-db-profile.h
 *
 * \brief Заголовочный файл класса HyScanDBProfile - профиля БД.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanDBProfile HyScanDBProfile - профиль БД.
 *
 */

#ifndef __HYSCAN_DB_PROFILE_H__
#define __HYSCAN_DB_PROFILE_H__

#include <glib-object.h>
#include <hyscan-api.h>
#include <hyscan-serializable.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DB_PROFILE            \
        (hyscan_db_profile_get_type ())

#define HYSCAN_DB_PROFILE(obj)            \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_PROFILE, HyScanDBProfile))

#define HYSCAN_IS_DB_PROFILE(obj)         \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_PROFILE))

#define HYSCAN_DB_PROFILE_CLASS(klass)    \
        (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_PROFILE, HyScanDBProfileClass))

#define HYSCAN_IS_DB_PROFILE_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_PROFILE))

#define HYSCAN_DB_PROFILE_GET_CLASS(obj)  \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_PROFILE, HyScanDBProfileClass))

typedef struct _HyScanDBProfile HyScanDBProfile;
typedef struct _HyScanDBProfilePrivate HyScanDBProfilePrivate;
typedef struct _HyScanDBProfileClass HyScanDBProfileClass;

struct _HyScanDBProfile
{
  GObject                   parent_instance;
  HyScanDBProfilePrivate   *priv;
};

struct _HyScanDBProfileClass
{
  GObjectClass   parent_class;
};

HYSCAN_API
GType              hyscan_db_profile_get_type            (void);

/* Создаёт объект HyScanDBProfile. */
HYSCAN_API
HyScanDBProfile*   hyscan_db_profile_new                 (void);

/* Создаёт объект HyScanDBProfile. */
HYSCAN_API
HyScanDBProfile*   hyscan_db_profile_new_full            (const gchar       *name,
                                                          const gchar       *uri);

/* Получает имя системы хранения. */
HYSCAN_API
const gchar*       hyscan_db_profile_get_name            (HyScanDBProfile   *profile);

/* Получает URI системы хранения. */
HYSCAN_API
const gchar*       hyscan_db_profile_get_uri             (HyScanDBProfile   *profile);

/* Задаёт имя системы хранения. */
HYSCAN_API
void               hyscan_db_profile_set_name            (HyScanDBProfile   *profile,
                                                          const gchar       *name);

/* Задаёт URI системы хранения. */
HYSCAN_API
void               hyscan_db_profile_set_uri             (HyScanDBProfile   *profile,
                                                          const gchar       *uri);

G_END_DECLS

#endif /* __HYSCAN_DB_PROFILE_H__ */
