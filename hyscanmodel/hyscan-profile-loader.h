/* hyscan-profile-loader.h
 *
 * Copyright 2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanModel.
 *
 * HyScanModel is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanModel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanModel имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanModel на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#ifndef __HYSCAN_PROFILE_LOADER_H__
#define __HYSCAN_PROFILE_LOADER_H__

#include <glib-object.h>
#include <hyscan-api.h>
#include "hyscan-profile.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_PROFILE_LOADER             (hyscan_profile_loader_get_type ())
#define HYSCAN_PROFILE_LOADER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PROFILE_LOADER, HyScanProfileLoader))
#define HYSCAN_IS_PROFILE_LOADER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PROFILE_LOADER))
#define HYSCAN_PROFILE_LOADER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PROFILE_LOADER, HyScanProfileLoaderClass))
#define HYSCAN_IS_PROFILE_LOADER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PROFILE_LOADER))
#define HYSCAN_PROFILE_LOADER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PROFILE_LOADER, HyScanProfileLoaderClass))

typedef struct _HyScanProfileLoader HyScanProfileLoader;
typedef struct _HyScanProfileLoaderPrivate HyScanProfileLoaderPrivate;
typedef struct _HyScanProfileLoaderClass HyScanProfileLoaderClass;

struct _HyScanProfileLoader
{
  GObject parent_instance;

  HyScanProfileLoaderPrivate *priv;
};

struct _HyScanProfileLoaderClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_profile_loader_get_type (void);

HYSCAN_API
HyScanProfileLoader *  hyscan_profile_loader_new      (const gchar *subfolder,
                                                       const gchar *systemfolder,
                                                       GType        profile_type);

HYSCAN_API
GHashTable *           hyscan_profile_loader_list     (HyScanProfileLoader *loader);

G_END_DECLS

#endif /* __HYSCAN_PROFILE_LOADER_H__ */
