#ifndef __HYSCAN_CONNECTOR_H__
#define __HYSCAN_CONNECTOR_H__

#include <glib-object.h>
#include <hyscan-api.h>
#include <hyscan-db.h>
#include <hyscan-sonar-control.h>
#include "hyscan-async.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_CONNECTOR            \
        (hyscan_connector_get_type ())

#define HYSCAN_CONNECTOR(obj)            \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_CONNECTOR, HyScanConnector))

#define HYSCAN_IS_CONNECTOR(obj)         \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_CONNECTOR))

#define HYSCAN_CONNECTOR_CLASS(klass)    \
        (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_CONNECTOR, HyScanConnectorClass))

#define HYSCAN_IS_CONNECTOR_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_CONNECTOR))

#define HYSCAN_CONNECTOR_GET_CLASS(obj)  \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_CONNECTOR, HyScanConnectorClass))

typedef struct _HyScanConnector HyScanConnector;
typedef struct _HyScanConnectorPrivate HyScanConnectorPrivate;
typedef struct _HyScanConnectorClass HyScanConnectorClass;

struct _HyScanConnector
{
  HyScanAsync             parent_instance;
  HyScanConnectorPrivate *priv;
};

struct _HyScanConnectorClass
{
  HyScanAsyncClass parent_class;
};

HYSCAN_API
GType                hyscan_connector_get_type           (void);

/* Создаёт HyScanConnector. */
HYSCAN_API
HyScanConnector*     hyscan_connector_new                (void);

/* Запускает асинхронное создание подключение к БД и/или ГЛ. */
HYSCAN_API
gboolean             hyscan_connector_execute            (HyScanConnector  *connector,
                                                          const gchar      *db_profile,
                                                          const gchar      *sonar_profile);

/* Получает HyScanDB. */
HYSCAN_API
HyScanDB*            hyscan_connector_get_db             (HyScanConnector  *connector);

/* Получает HyScanSonarControl. */
HYSCAN_API
HyScanSonarControl*  hyscan_connector_get_sonar_control  (HyScanConnector  *connector);

G_END_DECLS

#endif /* __HYSCAN_CONNECTOR_H__ */
