#ifndef __HYSCAN_SONAR_PROFILE_H__
#define __HYSCAN_SONAR_PROFILE_H__

#include <glib-object.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_PROFILE            \
        (hyscan_sonar_profile_get_type ())

#define HYSCAN_SONAR_PROFILE(obj)            \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_PROFILE, HyScanSonarProfile))

#define HYSCAN_IS_SONAR_PROFILE(obj)         \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_PROFILE))

#define HYSCAN_SONAR_PROFILE_CLASS(klass)    \
        (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_PROFILE, HyScanSonarProfileClass))

#define HYSCAN_IS_SONAR_PROFILE_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_PROFILE))

#define HYSCAN_SONAR_PROFILE_GET_CLASS(obj)  \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_PROFILE, HyScanSonarProfileClass))

typedef struct _HyScanSonarProfile HyScanSonarProfile;
typedef struct _HyScanSonarProfilePrivate HyScanSonarProfilePrivate;
typedef struct _HyScanSonarProfileClass HyScanSonarProfileClass;

struct _HyScanSonarProfile
{
  GObject                      parent_instance;
  HyScanSonarProfilePrivate   *priv;
};

struct _HyScanSonarProfileClass
{
  GObjectClass   parent_class;
};

HYSCAN_API
GType                 hyscan_sonar_profile_get_type            (void);

/* Создаёт объект HyScanSonarProfile. */
HYSCAN_API
HyScanSonarProfile*   hyscan_sonar_profile_new                 (void);

/* Создаёт объект HyScanSonarProfile. */
HYSCAN_API
HyScanSonarProfile*   hyscan_sonar_profile_new_full            (const gchar          *name,
                                                                const gchar          *driver_path,
                                                                const gchar          *driver_name,
                                                                const gchar          *uri,
                                                                const gchar          *config);

/* Десериализация из INI-файла. */
HYSCAN_API
gboolean              hyscan_sonar_profile_read_from_file      (HyScanSonarProfile   *profile,
                                                                const gchar          *filename);

/* Сериализация в INI-файл. */
HYSCAN_API
gboolean              hyscan_sonar_profile_write_to_file       (HyScanSonarProfile   *profile,
                                                                const gchar          *filename);

/* Получает имя локатора. */
HYSCAN_API
const gchar*          hyscan_sonar_profile_get_name            (HyScanSonarProfile   *profile);

HYSCAN_API
const gchar*          hyscan_sonar_profile_get_driver_path     (HyScanSonarProfile   *profile);

HYSCAN_API
const gchar*          hyscan_sonar_profile_get_driver_name     (HyScanSonarProfile   *profile);

HYSCAN_API
const gchar*          hyscan_sonar_profile_get_sonar_uri       (HyScanSonarProfile   *profile);

HYSCAN_API
const gchar*          hyscan_sonar_profile_get_sonar_config    (HyScanSonarProfile   *profile);

/* Задаёт имя локатора. */
HYSCAN_API
void                  hyscan_sonar_profile_set_name            (HyScanSonarProfile   *profile,
                                                                const gchar          *name);

HYSCAN_API
void                  hyscan_sonar_profile_set_driver_path     (HyScanSonarProfile   *profile,
                                                                const gchar          *driver_path);
HYSCAN_API
void                  hyscan_sonar_profile_set_driver_name     (HyScanSonarProfile   *profile,
                                                                const gchar          *driver_name);

HYSCAN_API
void                  hyscan_sonar_profile_set_sonar_uri       (HyScanSonarProfile   *profile,
                                                                const gchar          *uri);

HYSCAN_API
void                  hyscan_sonar_profile_set_sonar_config    (HyScanSonarProfile   *profile,
                                                                const gchar          *config);

G_END_DECLS

#endif /* __HYSCAN_SONAR_PROFILE_H__ */
