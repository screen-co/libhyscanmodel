// TODO: this naming sucks
#ifndef __HYSCAN_HW_PROFILE_DEVICE_H__
#define __HYSCAN_HW_PROFILE_DEVICE_H__

#include <hyscan-param-list.h>
#include <hyscan-discover.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_HW_PROFILE_DEVICE             (hyscan_hw_profile_device_get_type ())
#define HYSCAN_HW_PROFILE_DEVICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_HW_PROFILE_DEVICE, HyScanHWProfileDevice))
#define HYSCAN_IS_HW_PROFILE_DEVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_HW_PROFILE_DEVICE))
#define HYSCAN_HW_PROFILE_DEVICE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_HW_PROFILE_DEVICE, HyScanHWProfileDeviceClass))
#define HYSCAN_IS_HW_PROFILE_DEVICE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_HW_PROFILE_DEVICE))
#define HYSCAN_HW_PROFILE_DEVICE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_HW_PROFILE_DEVICE, HyScanHWProfileDeviceClass))

typedef struct _HyScanHWProfileDevice HyScanHWProfileDevice;
typedef struct _HyScanHWProfileDevicePrivate HyScanHWProfileDevicePrivate;
typedef struct _HyScanHWProfileDeviceClass HyScanHWProfileDeviceClass;

/* TODO: Change GObject to type of the base class. */
struct _HyScanHWProfileDevice
{
  GObject parent_instance;

  HyScanHWProfileDevicePrivate *priv;
};

/* TODO: Change GObjectClass to type of the base class. */
struct _HyScanHWProfileDeviceClass
{
  GObjectClass parent_class;
};

GType                   hyscan_hw_profile_device_get_type         (void);

HyScanHWProfileDevice * hyscan_hw_profile_device_new              (GKeyFile              *keyfile);
void                    hyscan_hw_profile_device_set_group        (HyScanHWProfileDevice *hw_device,
                                                                   const gchar           *group);
void                    hyscan_hw_profile_device_set_paths        (HyScanHWProfileDevice *hw_device,
                                                                   gchar                **paths);
void                    hyscan_hw_profile_device_read             (HyScanHWProfileDevice *hw_device,
                                                                   GKeyFile              *kf);
gboolean                hyscan_hw_profile_device_check            (HyScanHWProfileDevice *hw_device);
HyScanDevice *          hyscan_hw_profile_device_connect          (HyScanHWProfileDevice *hw_device);

//gchar *                 hyscan_hw_profile_device_get_uri          (HyScanHWProfileDevice *hw_device);
//gchar *                 hyscan_hw_profile_device_get_driver       (HyScanHWProfileDevice *hw_device);
//HyScanParamList *       hyscan_hw_profile_device_get_params       (HyScanHWProfileDevice *hw_device,
//                                                                   HyScanDataSchema      *schema);

// void                    hyscan_hw_profile_device_set_uri          (HyScanHWProfileDevice *hw_device,
//                                                                    gchar                 *uri);
// void                    hyscan_hw_profile_device_set_driver       (HyScanHWProfileDevice *hw_device,
//                                                                    gchar                 *driver);
// void                    hyscan_hw_profile_device_set_params       (HyScanHWProfileDevice *hw_device,
//                                                                    HyScanParamList       *params);

// void                    hyscan_hw_profile_device_write            (HyScanHWProfileDevice *hw_device);
// void                    hyscan_hw_profile_device_read             (HyScanHWProfileDevice *hw_device);



G_END_DECLS

#endif /* __HYSCAN_HW_PROFILE_DEVICE_H__ */
