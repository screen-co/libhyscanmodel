#ifndef __HYSCAN_SONAR_RECORDER_H__
#define __HYSCAN_SONAR_RECORDER_H__

#include <hyscan-sonar.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_RECORDER             (hyscan_sonar_recorder_get_type ())
#define HYSCAN_SONAR_RECORDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_RECORDER, HyScanSonarRecorder))
#define HYSCAN_IS_SONAR_RECORDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_RECORDER))
#define HYSCAN_SONAR_RECORDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_RECORDER, HyScanSonarRecorderClass))
#define HYSCAN_IS_SONAR_RECORDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_RECORDER))
#define HYSCAN_SONAR_RECORDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_RECORDER, HyScanSonarRecorderClass))

typedef struct _HyScanSonarRecorder HyScanSonarRecorder;
typedef struct _HyScanSonarRecorderPrivate HyScanSonarRecorderPrivate;
typedef struct _HyScanSonarRecorderClass HyScanSonarRecorderClass;

struct _HyScanSonarRecorder
{
  GObject parent_instance;

  HyScanSonarRecorderPrivate *priv;
};

struct _HyScanSonarRecorderClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_sonar_recorder_get_type       (void);

HYSCAN_API
HyScanSonarRecorder *  hyscan_sonar_recorder_new            (HyScanSonar         *sonar,
                                                             HyScanDB            *db);

HYSCAN_API
HyScanSonar *          hyscan_sonar_recorder_get_sonar      (HyScanSonarRecorder *recorder);

HYSCAN_API
void                   hyscan_sonar_recorder_set_suffix     (HyScanSonarRecorder *recorder,
                                                             const gchar         *suffix,
                                                             gboolean             one_time);

HYSCAN_API
void                   hyscan_sonar_recorder_set_plan       (HyScanSonarRecorder *recorder,
                                                             HyScanTrackPlan     *plan);

HYSCAN_API
void                   hyscan_sonar_recorder_set_project    (HyScanSonarRecorder *recorder,
                                                             const gchar         *project_name);

HYSCAN_API
void                   hyscan_sonar_recorder_set_track_name (HyScanSonarRecorder *recorder,
                                                             const gchar         *track_name);

HYSCAN_API
void                   hyscan_sonar_recorder_set_track_type (HyScanSonarRecorder *recorder,
                                                             HyScanTrackType      track_type);

HYSCAN_API
gboolean               hyscan_sonar_recorder_start          (HyScanSonarRecorder *recorder);

HYSCAN_API
gboolean               hyscan_sonar_recorder_stop           (HyScanSonarRecorder *recorder);


G_END_DECLS

#endif /* __HYSCAN_SONAR_RECORDER_H__ */
