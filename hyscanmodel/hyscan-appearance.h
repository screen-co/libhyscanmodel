/**
 * \file hyscan-appearance.h
 *
 * \brief Модель внешнего вида приложения.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanAppearance HyScanAppearance - модель внешнего вида приложения.
 */

#ifndef __HYSCAN_APPEARANCE_H__
#define __HYSCAN_APPEARANCE_H__

#include <glib-object.h>
#include <hyscan-api.h>
#include <hyscan-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_APPEARANCE \
        (hyscan_appearance_get_type ())

#define HYSCAN_APPEARANCE(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_APPEARANCE, HyScanAppearance))

#define HYSCAN_IS_APPEARANCE(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_APPEARANCE))

#define HYSCAN_APPEARANCE_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_APPEARANCE, HyScanAppearanceClass))

#define HYSCAN_IS_APPEARANCE_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_APPEARANCE))

#define HYSCAN_APPEARANCE_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_APPEARANCE, HyScanAppearanceClass))


typedef struct _HyScanAppearance HyScanAppearance;
typedef struct _HyScanAppearancePrivate HyScanAppearancePrivate;
typedef struct _HyScanAppearanceClass HyScanAppearanceClass;

struct _HyScanAppearance
{
  GObject parent_instance;
  HyScanAppearancePrivate *priv;
};

struct _HyScanAppearanceClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType              hyscan_appearance_get_type          (void);

HYSCAN_API
HyScanAppearance  *hyscan_appearance_new               (void);


HYSCAN_API
void               hyscan_appearance_get_colormap      (HyScanAppearance  *appearance,
                                                        const gchar       *id,
                                                        gchar            **name,
                                                        guint32           *background,
                                                        guint32          **colormap,
                                                        guint             *length);

HYSCAN_API
gchar            **hyscan_appearance_get_colormaps_ids (HyScanAppearance  *appearance);

HYSCAN_API
const gchar       *hyscan_appearance_get_colormap_id   (HyScanAppearance  *appearance);

HYSCAN_API
void               hyscan_appearance_set_colormap_id   (HyScanAppearance  *appearance,
                                                        const gchar       *id);

HYSCAN_API
void               hyscan_appearance_get_substrate     (HyScanAppearance  *appearance,
                                                        guint32           *substrate);

HYSCAN_API
void               hyscan_appearance_get_levels        (HyScanAppearance  *appearance,
                                                        HyScanSourceType   source,
                                                        gdouble           *black,
                                                        gdouble           *gamma,
                                                        gdouble           *white);

HYSCAN_API
void               hyscan_appearance_set_substrate     (HyScanAppearance  *appearance,
                                                        guint32            substrate);

HYSCAN_API
void               hyscan_appearance_set_levels        (HyScanAppearance  *appearance,
                                                        HyScanSourceType   source,
                                                        gdouble            black,
                                                        gdouble            gamma,
                                                        gdouble            white);

G_END_DECLS

#endif /* __HYSCAN_APPEARANCE_H__ */
