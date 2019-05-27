#ifndef __HYSCAN_OFFSET_PROFILE_H__
#define __HYSCAN_OFFSET_PROFILE_H__

#include <hyscan-api.h>
#include <hyscan-types.h>
#include <hyscan-control.h>
#include "hyscan-profile.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_OFFSET_PROFILE             (hyscan_offset_profile_get_type ())
#define HYSCAN_OFFSET_PROFILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_OFFSET_PROFILE, HyScanOffsetProfile))
#define HYSCAN_IS_OFFSET_PROFILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_OFFSET_PROFILE))
#define HYSCAN_OFFSET_PROFILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_OFFSET_PROFILE, HyScanOffsetProfileClass))
#define HYSCAN_IS_OFFSET_PROFILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_OFFSET_PROFILE))
#define HYSCAN_OFFSET_PROFILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_OFFSET_PROFILE, HyScanOffsetProfileClass))

typedef struct _HyScanOffsetProfile HyScanOffsetProfile;
typedef struct _HyScanOffsetProfilePrivate HyScanOffsetProfilePrivate;
typedef struct _HyScanOffsetProfileClass HyScanOffsetProfileClass;

struct _HyScanOffsetProfile
{
  HyScanProfile parent_instance;

  HyScanOffsetProfilePrivate *priv;
};

struct _HyScanOffsetProfileClass
{
  HyScanProfileClass parent_class;
};

HYSCAN_API
GType                  hyscan_offset_profile_get_type         (void);

HYSCAN_API
HyScanOffsetProfile *  hyscan_offset_profile_new              (const gchar         *file);

HYSCAN_API
GHashTable *           hyscan_offset_profile_get_sonars       (HyScanOffsetProfile *profile);

HYSCAN_API
GHashTable *           hyscan_offset_profile_get_sensors      (HyScanOffsetProfile *profile);

HYSCAN_API
gboolean               hyscan_offset_profile_apply            (HyScanOffsetProfile *profile,
                                                               HyScanControl       *control);

G_END_DECLS

#endif /* __HYSCAN_OFFSET_PROFILE_H__ */
