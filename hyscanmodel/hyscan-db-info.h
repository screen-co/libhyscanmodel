/* hyscan-db-info.h
 *
 * Copyright 2017-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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

#ifndef __HYSCAN_DB_INFO_H__
#define __HYSCAN_DB_INFO_H__

#include <hyscan-db.h>

G_BEGIN_DECLS

typedef struct _HyScanProjectInfo HyScanProjectInfo;
typedef struct _HyScanTrackInfo HyScanTrackInfo;

/**
 * HyScanProjectInfo:
 * @name: название проекта
 * @ctime: дата и время создания проекта в локальной зоне
 * @mtime: дата и время изменения проекта в локальной зоне
 * @description: описание проекта
 * @error: признак ошибки в проекте
 *
 * Структура содержит информацию о проекте.
 */
struct _HyScanProjectInfo
{
  const gchar         *name;
  GDateTime           *ctime;
  GDateTime           *mtime;
  const gchar         *description;
  gboolean             error;
};

/**
 * HyScanTrackInfo:
 * @id: уникальный идентификатор галса
 * @type: тип галса
 * @name: название галса
 * @ctime: дата и время создания галса в локальной зоне
 * @mtime: дата и время изменения галса в локальной зоне
 * @description: описание галса
 * @operator_name: имя оператора записавшего галс
 * @sonar_info: общая информация о гидролокаторе
 * @plan: информация о запланированных параметрах галса или NULL
 * @sources: список источников данных
 * @record: признак записи данных в галс
 * @error: признак ошибки в галсе
 *
 * Структура содержит информацию о галсе.
 */
struct _HyScanTrackInfo
{
  const gchar         *id;
  HyScanTrackType      type;
  const gchar         *name;
  GDateTime           *ctime;
  GDateTime           *mtime;
  gchar               *description;
  const gchar         *operator_name;
  guint64              labels;
  HyScanDataSchema    *sonar_info;
  HyScanTrackPlan     *plan;
  gboolean             sources[HYSCAN_SOURCE_LAST];
  gboolean             record;
  gboolean             error;
};

#define HYSCAN_TYPE_DB_INFO             (hyscan_db_info_get_type ())
#define HYSCAN_DB_INFO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_INFO, HyScanDBInfo))
#define HYSCAN_IS_DB_INFO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_INFO))
#define HYSCAN_DB_INFO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_INFO, HyScanDBInfoClass))
#define HYSCAN_IS_DB_INFO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_INFO))
#define HYSCAN_DB_INFO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_INFO, HyScanDBInfoClass))

typedef struct _HyScanDBInfo HyScanDBInfo;
typedef struct _HyScanDBInfoPrivate HyScanDBInfoPrivate;
typedef struct _HyScanDBInfoClass HyScanDBInfoClass;

struct _HyScanDBInfo
{
  GObject parent_instance;

  HyScanDBInfoPrivate *priv;
};

struct _HyScanDBInfoClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_db_info_get_type                 (void);

HYSCAN_API
HyScanDBInfo *         hyscan_db_info_new                      (HyScanDB              *db);

HYSCAN_API
void                   hyscan_db_info_set_project              (HyScanDBInfo          *info,
                                                                const gchar           *project_name);

HYSCAN_API
gchar *                hyscan_db_info_get_project              (HyScanDBInfo          *info);

HYSCAN_API
HyScanDB *             hyscan_db_info_get_db                   (HyScanDBInfo          *info);

HYSCAN_API
GHashTable *           hyscan_db_info_get_projects             (HyScanDBInfo          *info);

HYSCAN_API
GHashTable *           hyscan_db_info_get_tracks               (HyScanDBInfo          *info);

HYSCAN_API
void                   hyscan_db_info_refresh                  (HyScanDBInfo          *info);

HYSCAN_API
HyScanProjectInfo *    hyscan_db_info_get_project_info         (HyScanDB              *db,
                                                                const gchar           *project_name);

HYSCAN_API
HyScanTrackInfo *      hyscan_db_info_get_track_info           (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name);

HYSCAN_API
void                   hyscan_db_info_modify_track_info        (HyScanDBInfo          *db_info,
                                                                HyScanTrackInfo       *track_info);

HYSCAN_API
HyScanProjectInfo *    hyscan_db_info_project_info_copy        (HyScanProjectInfo     *info);

HYSCAN_API
void                   hyscan_db_info_project_info_free        (HyScanProjectInfo     *info);

HYSCAN_API
HyScanTrackInfo *      hyscan_db_info_track_info_copy          (HyScanTrackInfo       *info);

HYSCAN_API
void                   hyscan_db_info_track_info_free          (HyScanTrackInfo       *info);

G_END_DECLS

#endif /* __HYSCAN_DB_INFO_H__ */
