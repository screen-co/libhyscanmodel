#ifndef __HYSCAN_HW_PROFILE_H__
#define __HYSCAN_HW_PROFILE_H__

#include <hyscan-control.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_HW_PROFILE             (hyscan_hw_profile_get_type ())
#define HYSCAN_HW_PROFILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_HW_PROFILE, HyScanHWProfile))
#define HYSCAN_IS_HW_PROFILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_HW_PROFILE))
#define HYSCAN_HW_PROFILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_HW_PROFILE, HyScanHWProfileClass))
#define HYSCAN_IS_HW_PROFILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_HW_PROFILE))
#define HYSCAN_HW_PROFILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_HW_PROFILE, HyScanHWProfileClass))

typedef struct _HyScanHWProfile HyScanHWProfile;
typedef struct _HyScanHWProfilePrivate HyScanHWProfilePrivate;
typedef struct _HyScanHWProfileClass HyScanHWProfileClass;

/* TODO: Change GObject to type of the base class. */
struct _HyScanHWProfile
{
  GObject parent_instance;

  HyScanHWProfilePrivate *priv;
};

/* TODO: Change GObjectClass to type of the base class. */
struct _HyScanHWProfileClass
{
  GObjectClass parent_class;
};

GType                  hyscan_hw_profile_get_type         (void);

HYSCAN_API
HyScanHWProfile *      hyscan_hw_profile_new              (const gchar     *file);

HYSCAN_API
void                   hyscan_hw_profile_set_driver_paths (HyScanHWProfile *profile,
                                                           gchar          **driver_paths);
HYSCAN_API
void                   hyscan_hw_profile_read             (HyScanHWProfile *profile);

HYSCAN_API
GList *                hyscan_hw_profile_list             (HyScanHWProfile *profile);

HYSCAN_API
gboolean               hyscan_hw_profile_check            (HyScanHWProfile *profile);

HYSCAN_API
HyScanControl *        hyscan_hw_profile_connect          (HyScanHWProfile *profile);

HYSCAN_API
HyScanControl *        hyscan_hw_profile_connect_simple   (const gchar     *file);

G_END_DECLS

#endif /* __HYSCAN_HW_PROFILE_H__ */
