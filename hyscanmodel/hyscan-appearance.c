/**
 * \file hyscan-appearance.c
 *
 * \brief Модель внешнего вида приложения.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 */

#include "hyscan-appearance.h"
#include <hyscan-tile-color.h>
#include <memory.h>

typedef struct
{
  const gchar *name;
  guint32      background;
  guint32     *colormap;
  gsize        length;
} HyScanColormapInfo;

typedef struct
{
  gdouble black;
  gdouble gamma;
  gdouble white;
} HyScanAppearanceLevels;

struct _HyScanAppearancePrivate
{
  GHashTable             *default_colormaps;
  gchar                  *colormap_id;

  guint32                 substrate;
  GHashTable             *levels;
};

/* Индексы сигналов в массиве идентификаторов сигналов. */
enum
{
  SIGNAL_COLORMAP_CHANGED,
  SIGNAL_SUBSTRATE_CHANGED,
  SIGNAL_LEVELS_CHANGED,
  SIGNAL_LAST
};

/* Массив идентификаторов сигналов. */
static guint hyscan_appearance_signals[SIGNAL_LAST] = { 0 };

static void hyscan_appearance_constructed    (GObject          *object);
static void hyscan_appearance_finalize       (GObject          *object);
static void hyscan_appearance_colormaps_init (HyScanAppearance *appearance);
static void hyscan_appearance_levels_init    (HyScanAppearance *appearance);
static void hyscan_appearance_substrate_init (HyScanAppearance *appearance);

static void hyscan_colormap_info_free        (gpointer          data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanAppearance, hyscan_appearance, G_TYPE_OBJECT)

static void
hyscan_appearance_class_init (HyScanAppearanceClass *klass)
{
  GObjectClass *oc = G_OBJECT_CLASS (klass);

  oc->constructed = hyscan_appearance_constructed;
  oc->finalize = hyscan_appearance_finalize;

  hyscan_appearance_signals[SIGNAL_COLORMAP_CHANGED] = g_signal_new (
    "colormap-changed", HYSCAN_TYPE_APPEARANCE,
    G_SIGNAL_RUN_LAST, 0, NULL, NULL,
    g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0
  );

  hyscan_appearance_signals[SIGNAL_SUBSTRATE_CHANGED] = g_signal_new (
    "substrate-changed", HYSCAN_TYPE_APPEARANCE,
    G_SIGNAL_RUN_LAST, 0, NULL, NULL,
    g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0
  );

  hyscan_appearance_signals[SIGNAL_LEVELS_CHANGED] = g_signal_new (
    "levels-changed", HYSCAN_TYPE_APPEARANCE,
    G_SIGNAL_RUN_LAST, 0, NULL, NULL,
    g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0
  );
}

static void
hyscan_appearance_init (HyScanAppearance *self)
{
  self->priv = hyscan_appearance_get_instance_private (self);
}

static void
hyscan_appearance_constructed (GObject *object)
{
  HyScanAppearance *appearance = HYSCAN_APPEARANCE (object);

  hyscan_appearance_colormaps_init (appearance);
  hyscan_appearance_levels_init (appearance);
  hyscan_appearance_substrate_init (appearance);
}

static void
hyscan_appearance_finalize (GObject *object)
{
  HyScanAppearance *self = HYSCAN_APPEARANCE (object);
  HyScanAppearancePrivate *priv = self->priv;

  g_hash_table_unref (priv->default_colormaps);
  g_hash_table_unref (priv->levels);
  g_free (priv->colormap_id);

  G_OBJECT_CLASS (hyscan_appearance_parent_class)->finalize (object);
}

static void
hyscan_appearance_colormaps_init (HyScanAppearance *appearance)
{
  HyScanAppearancePrivate *priv = appearance->priv;
  const gsize length = 256;
  HyScanColormapInfo *white_cm;
  HyScanColormapInfo *yellow_cm;
  HyScanColormapInfo *green_cm;
  gsize i;

  white_cm = g_new (HyScanColormapInfo, 1);
  white_cm->name = "White on Black";
  white_cm->background = hyscan_tile_color_converter_d2i (0, 0, 0, 1);
  white_cm->colormap = g_new0 (guint32, length);
  white_cm->length = length;

  yellow_cm = g_new (HyScanColormapInfo, 1);
  yellow_cm->name = "Yellow on Black";
  yellow_cm->background = hyscan_tile_color_converter_d2i (0, 0, 0, 1);
  yellow_cm->colormap = g_new0 (guint32, length);
  yellow_cm->length = length;

  green_cm = g_new (HyScanColormapInfo, 1);
  green_cm->name = "Green on Black";
  green_cm->background = hyscan_tile_color_converter_d2i (0, 0, 0, 1);
  green_cm->colormap = g_new0 (guint32, length);
  green_cm->length = length;

  for (i = 0; i < length; ++i)
    {
      gdouble luminance = i / (gdouble) (length - 1);
      white_cm->colormap[i] = hyscan_tile_color_converter_d2i  (luminance, luminance, luminance, 1);
      yellow_cm->colormap[i] = hyscan_tile_color_converter_d2i (luminance, luminance, 0, 1);
      green_cm->colormap[i] = hyscan_tile_color_converter_d2i (0, luminance, 0, 1);
    }

  priv->default_colormaps = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, hyscan_colormap_info_free);
  g_hash_table_insert (priv->default_colormaps, "white_on_black", white_cm);
  g_hash_table_insert (priv->default_colormaps, "yellow_on_black", yellow_cm);
  g_hash_table_insert (priv->default_colormaps, "green_on_black", green_cm);

  /* Палитра по умолчанию. */
  priv->colormap_id = g_strdup ("yellow_on_black");
}

static void
hyscan_appearance_levels_init (HyScanAppearance *appearance)
{
  HyScanAppearancePrivate *priv = appearance->priv;
  HyScanAppearanceLevels *fallback = g_new0 (HyScanAppearanceLevels, 1);

  priv->levels = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                        NULL, g_free);

  fallback->black = 0.0;
  fallback->white = 0.2;
  fallback->gamma = 1.0;

  g_hash_table_insert (priv->levels, GINT_TO_POINTER (HYSCAN_SOURCE_INVALID), fallback);
}

static void
hyscan_appearance_substrate_init (HyScanAppearance *appearance)
{
  appearance->priv->substrate = hyscan_tile_color_converter_d2i (0, 0, 0, 1);
}

static void
hyscan_colormap_info_free (gpointer data)
{
  if (data != NULL)
    {
      HyScanColormapInfo *cinf = data;
      g_free (cinf->colormap);
      g_free (cinf);
    }
}

void
hyscan_appearance_get_colormap (HyScanAppearance *appearance,
                                const gchar      *id,
                                gchar           **name,
                                guint32          *background,
                                guint32         **colormap,
                                guint            *length)
{
  HyScanAppearancePrivate *priv;
  HyScanColormapInfo *cinf;

  g_return_if_fail (HYSCAN_IS_APPEARANCE (appearance));

  priv = appearance->priv;

  if (id != NULL && (cinf = g_hash_table_lookup (priv->default_colormaps, id)) != NULL)
    {
      if (name != NULL)
        *name = g_strdup (cinf->name);
      if (background != NULL)
        *background = cinf->background;
      if (colormap != NULL)
        {
          *colormap = g_new (guint32, cinf->length);
          memcpy (*colormap, cinf->colormap, cinf->length * sizeof (guint32));
        }
      if (length != NULL)
        *length = cinf->length;
    }
}

gchar **
hyscan_appearance_get_colormaps_ids (HyScanAppearance *appearance)
{
  HyScanAppearancePrivate *priv;
  GList *keys;
  gchar **ids = NULL;

  g_return_val_if_fail (HYSCAN_IS_APPEARANCE (appearance), NULL);

  priv = appearance->priv;

  if ((keys = g_hash_table_get_keys (priv->default_colormaps)) != NULL)
    {
      GList *k;
      gsize i;

      ids = g_malloc0 (sizeof(gchar *) * (g_list_length (keys) + 1));
      for (k = keys, i = 0; k != NULL; k = g_list_next (k))
        ids[i++] = g_strdup (k->data);

      g_list_free (keys);
    }

  return ids;
}

const gchar *
hyscan_appearance_get_colormap_id (HyScanAppearance *appearance)
{
  g_return_val_if_fail (HYSCAN_IS_APPEARANCE (appearance), NULL);

  return appearance->priv->colormap_id;
}

void
hyscan_appearance_set_colormap_id (HyScanAppearance *appearance,
                                   const gchar      *id)
{
  HyScanAppearancePrivate *priv;

  g_return_if_fail (HYSCAN_IS_APPEARANCE (appearance));

  priv = appearance->priv;

  if (id != NULL && g_hash_table_lookup (priv->default_colormaps, id) != NULL)
    {
      g_free (priv->colormap_id);
      priv->colormap_id = g_strdup (id);

      g_signal_emit (appearance, hyscan_appearance_signals[SIGNAL_COLORMAP_CHANGED], 0);
    }
}

void
hyscan_appearance_get_substrate (HyScanAppearance *appearance,
                                 guint32          *substrate)
{
  g_return_if_fail (HYSCAN_IS_APPEARANCE (appearance));

  if (substrate != NULL)
    *substrate = appearance->priv->substrate;
}

void
hyscan_appearance_get_levels (HyScanAppearance *appearance,
                              HyScanSourceType  source,
                              gdouble          *black,
                              gdouble          *gamma,
                              gdouble          *white)
{
  HyScanAppearancePrivate *priv;
  HyScanAppearanceLevels *levels;

  g_return_if_fail (HYSCAN_IS_APPEARANCE (appearance));

  priv = appearance->priv;

  levels = g_hash_table_lookup (priv->levels, GINT_TO_POINTER (source));

  if (levels == NULL)
    {
      levels = g_hash_table_lookup (priv->levels, GINT_TO_POINTER (HYSCAN_SOURCE_INVALID));
      if (levels == NULL)
        return;
    }

  if (black != NULL)
    *black = levels->black;
  if (gamma != NULL)
    *gamma = levels->gamma;
  if (white != NULL)
    *white = levels->white;
}

void
hyscan_appearance_set_substrate (HyScanAppearance *appearance,
                                 guint32           substrate)
{
  HyScanAppearancePrivate *priv;

  g_return_if_fail (HYSCAN_IS_APPEARANCE (appearance));

  priv = appearance->priv;

  if (priv->substrate != substrate)
    {
      priv->substrate = substrate;

      g_signal_emit (appearance, hyscan_appearance_signals[SIGNAL_SUBSTRATE_CHANGED], 0);
    }
}

void
hyscan_appearance_set_levels (HyScanAppearance *appearance,
                              HyScanSourceType  source,
                              gdouble           black,
                              gdouble           gamma,
                              gdouble           white)
{
  HyScanAppearancePrivate *priv;
  HyScanAppearanceLevels *levels;

  g_return_if_fail (HYSCAN_IS_APPEARANCE (appearance));

  priv = appearance->priv;

  white = CLAMP (white, 10e-3, 1.0);
  black = CLAMP (black, 0.0, 1.0 - 10e-3);

  levels = g_hash_table_lookup (priv->levels, GINT_TO_POINTER (source));

  if (levels == NULL)
    {
      levels = g_new0 (HyScanAppearanceLevels, 1);

      g_hash_table_insert (priv->levels, GINT_TO_POINTER (source), levels);
    }

  if (levels->black != black || levels->gamma != gamma || levels->white != white)
    {
      white = black < white ? white : black + 10e-3;

      levels->black = black;
      levels->white = white;
      levels->gamma = gamma > 0.0 ? gamma : 10e-3;

      g_signal_emit (appearance, hyscan_appearance_signals[SIGNAL_LEVELS_CHANGED], 0);
    }
}

HyScanAppearance *
hyscan_appearance_new (void)
{
  return g_object_new (HYSCAN_TYPE_APPEARANCE, NULL);
}
