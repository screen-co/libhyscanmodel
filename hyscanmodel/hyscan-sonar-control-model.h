/*
 * \file hyscan-sonar-control-model.h
 * \brief Заголовочный файл класса асинхронного управления гидролокатором.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanSonarControlModel HyScanSonarControlModel - асинхронное
 * управление гидролокатором.
 *
 * Класс предназначен для асинхронного управления гидролокатором, он повторяет
 * все функции синхронного интерфейса управления гидролокатора \link HyScanSonarControl \endlink.
 * Предельные значения параметров методов HyScanSonarControlModel идентичны соответствующим
 * методам \link HyScanSonarControl \endlink.
 *
 * Экземпляр класса можно создать функцией #hyscan_sonar_control_model_new,
 * передав в качестве параметра указатель на синхронный интерфейс
 * управления гидролокатором - \link HyScanSonarControl \endlink.
 *
 * Класс содержит свойство "sonar-control", которое возможно задать только
 * при конструировании объёкта. Свойство получает в качестве параметра указатель
 * на \link HyScanSonarControl \endlink.
 *
 * Если свойство "sonar-control" не установить при конструировании, созданный
 * объект будет нефункционален - все его методы будут возвращать FALSE.
 *
 * \warning Данный класс корректно работает только в паре с GMainLoop, кроме того
 * он не является потокобезопасным.
 */
#ifndef __HYSCAN_SONAR_CONTROL_MODEL_H__
#define __HYSCAN_SONAR_CONTROL_MODEL_H__

#include <hyscan-sonar-control.h>
#include "hyscan-async.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_CONTROL_MODEL            \
        (hyscan_sonar_control_model_get_type ())

#define HYSCAN_SONAR_CONTROL_MODEL(obj)            \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_CONTROL_MODEL, HyScanSonarControlModel))

#define HYSCAN_IS_SONAR_CONTROL_MODEL(obj)         \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_CONTROL_MODEL))

#define HYSCAN_SONAR_CONTROL_MODEL_CLASS(klass)    \
        (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_CONTROL_MODEL, HyScanSonarControlModelClass))

#define HYSCAN_IS_SONAR_CONTROL_MODEL_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_CONTROL_MODEL))

#define HYSCAN_SONAR_CONTROL_MODEL_GET_CLASS(obj)  \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_CONTROL_MODEL, HyScanSonarControlModelClass))

typedef struct _HyScanSonarControlModel HyScanSonarControlModel;
typedef struct _HyScanSonarControlModelPrivate HyScanSonarControlModelPrivate;
typedef struct _HyScanSonarControlModelClass HyScanSonarControlModelClass;

struct _HyScanSonarControlModel
{
  HyScanAsync                       parent_instance;
  HyScanSonarControlModelPrivate   *priv;
};

struct _HyScanSonarControlModelClass
{
  HyScanAsyncClass   parent_class;
};

HYSCAN_API
GType        hyscan_sonar_control_model_get_type                      (void);

/*
 * Создаёт новый класс асинхронного управления гидролокатором.
 *
 * \param sonar_control указатель на класс \link HyScanSonarControlModel \endlink.
 *
 * \return указатель на класс \link HyScanSonarControlModel \endlink.
 */
HYSCAN_API
HyScanSonarControlModel *
            hyscan_sonar_control_model_new                            (HyScanSonarControl        *sonar_control);

/*
 * Асинхронный запрос на задание режима работы порта типа
 * HYSCAN_SENSOR_CONTROL_PORT_VIRTUAL.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param name название порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_sensor_set_virtual_port_param   (HyScanSonarControlModel   *model,
                                                                       const gchar               *name,
                                                                       guint                      channel,
                                                                       gint64                     time_offset);

/*
 * Асинхронный запрос на задание режима работы порта типа
 * HYSCAN_SENSOR_CONTROL_PORT_UART.
 *
 * В эту функцию передаются идентификаторы устойства UART и режима его работы
 * из списка допустимых значений (см. #hyscan_sensor_control_list_uart_devices и
 * #hyscan_sensor_control_list_uart_modes).
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param name название порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс;
 * \param protocol протокол обмена данными с датчиком;
 * \param uart_device идентификатор устройства UART;
 * \param uart_mode идентификатор режима работы устройства UART.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_sensor_set_uart_port_param      (HyScanSonarControlModel   *model,
                                                                       const gchar               *name,
                                                                       guint                      channel,
                                                                       gint64                     time_offset,
                                                                       HyScanSensorProtocolType   protocol,
                                                                       guint                      uart_device,
                                                                       guint                      uart_mode);

/*
 * Асинхронный запрос на задание режима работы порта типа
 * HYSCAN_SENSOR_CONTROL_PORT_UDP_IP.
 *
 * В эту функцию передаётся идентфикатор IP адреса из списка допустимых значений
 * (см. #hyscan_sensor_control_list_ip_addresses).
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param name название порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс;
 * \param protocol протокол обмена данными с датчиком;
 * \param ip_address идентификатор IP адреса, по которому принимать данные;
 * \param udp_port номер UDP порта, по которому принимать данные.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_sensor_set_udp_ip_port_param    (HyScanSonarControlModel   *model,
                                                                       const gchar               *name,
                                                                       guint                      channel,
                                                                       gint64                     time_offset,
                                                                       HyScanSensorProtocolType   protocol,
                                                                       guint                      ip_address,
                                                                       guint16                    udp_port);

/*
 * Асинхронный запрос на задание информации о местоположении датчика
 * относительно центра масс судна.
 * Подробное описание параметров приводится в \link HyScanCoreTypes \endlink.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param name название порта;
 * \param position параметры местоположения датчика.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_sensor_set_position             (HyScanSonarControlModel   *model,
                                                                       const gchar               *name,
                                                                       HyScanAntennaPosition     *position);

/*
 * Асинхронный запрос на включение или выключение приёма данных на указанном
 * порту.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param name название порта;
 * \param enable включён или выключен.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_sensor_set_enable               (HyScanSonarControlModel   *model,
                                                                       const gchar               *name,
                                                                       gboolean                   enable);


/*
 * Асинхронный запрос на включение преднастроенного режима работы генератора.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param source идентификатор источника данных;
 * \param preset идентификатор преднастройки.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_generator_set_preset            (HyScanSonarControlModel   *model,
                                                                       HyScanSourceType           source,
                                                                       guint                      preset);

/*
 * Асинхронный запрос на включение автоматического режима работы генератора.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param source идентификатор источника данных;
 * \param signal тип сигнала.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_generator_set_auto              (HyScanSonarControlModel   *model,
                                                                       HyScanSourceType           source,
                                                                       HyScanGeneratorSignalType  signal);

/*
 * Асинхронный запрос на включение упрощённого режима работы генератора.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param source идентификатор источника данных;
 * \param signal тип сигнала;
 * \param power энергия сигнала, проценты.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_generator_set_simple            (HyScanSonarControlModel    *model,
                                                                        HyScanSourceType           source,
                                                                        HyScanGeneratorSignalType  signal,
                                                                        gdouble                    power);

/*
 * Асинхронный запрос на включаение расширенного режима работы генератора.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param source идентификатор источника данных;
 * \param signal тип сигнала;
 * \param duration длительность сигнала, с;
 * \param power энергия сигнала, проценты.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_generator_set_extended          (HyScanSonarControlModel   *model,
                                                                       HyScanSourceType           source,
                                                                       HyScanGeneratorSignalType  signal,
                                                                       gdouble                    duration,
                                                                       gdouble                    power);

/*
 * Асинхронный запрос на включение или выключение формирования сигнала
 * генератором.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param source идентификатор источника данных;
 * \param enable включён или выключен.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_generator_set_enable            (HyScanSonarControlModel   *model,
                                                                       HyScanSourceType           source,
                                                                       gboolean                   enable);

/*
 * Асинхронный запрос на включение автоматического режима управления системой
 * ВАРУ.
 *
 * Если в качестве значений параметров уровня сигнала и (или) чувствительности
 * передать отрицательное число, будут установлены значения по умолчанию.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param source идентификатор источника данных;
 * \param level целевой уровень сигнала;
 * \param sensitivity чувствительность автомата регулировки.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_tvg_set_auto                    (HyScanSonarControlModel   *model,
                                                                       HyScanSourceType           source,
                                                                       gdouble                    level,
                                                                       gdouble                    sensitivity);

/*
 * Асинхронный запрос на задание постоянного уровня усиления системы ВАРУ.
 *
 * Уровень усиления должен находится в пределах значений, возвращаемых
 * функцией #hyscan_tvg_control_get_gain_range.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param source идентификатор источника данных;
 * \param gain коэффициент усиления, дБ;
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_tvg_set_constant                (HyScanSonarControlModel   *model,
                                                                       HyScanSourceType           source,
                                                                       gdouble                    gain);

/*
 * Асинхронный запрос на задание линейного увеличения усиления в дБ на 100
 * метров.
 *
 * Начальный уровень усиления должен находится в пределах в пределах значений,
 * возвращаемых функцией #hyscan_tvg_control_get_gain_range. Величина
 * изменения усиления должна находится в пределах от 0 до 100 дБ.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param source идентификатор источника данных;
 * \param gain0 начальный уровень усиления, дБ;
 * \param step величина изменения усиления каждые 100 метров, дБ.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_tvg_set_linear_db               (HyScanSonarControlModel   *model,
                                                                       HyScanSourceType           source,
                                                                       gdouble                    gain0,
                                                                       gdouble                    step);

/*
 * Асинхронный запрос на задание логарифмического вида закона усиления системы
 * ВАРУ.
 *
 * Начальный уровень усиления должен находится в пределах в пределах значений,
 * возвращаемых функцией #hyscan_tvg_control_get_gain_range. Значение
 * коэффициента отражения цели должно находится в пределах от 0 дБ до 100 дБ.
 * Значение коэффициента затухания должно находится в пределах от 0 дБ/м до
 * 1 дБ/м.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param source идентификатор источника данных;
 * \param gain0 начальный уровень усиления, дБ;
 * \param beta коэффициент отражения цели, дБ;
 * \param alpha коэффициент затухания, дБ/м.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_tvg_set_logarithmic             (HyScanSonarControlModel   *model,
                                                                       HyScanSourceType           source,
                                                                       gdouble                    gain0,
                                                                       gdouble                    beta,
                                                                       gdouble                    alpha);

/*
 * Асинхронный запрос на включение или выключение системы ВАРУ.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param source идентификатор источника данных;
 * \param enable включить или выключить.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_tvg_set_enable                  (HyScanSonarControlModel   *model,
                                                                       HyScanSourceType           source,
                                                                       gboolean                   enable);


/*
 * Асинхронный запрос на задание типа синхронизации излучения.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param sync_type тип синхронизации излучения.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_sonar_set_sync_type             (HyScanSonarControlModel   *model,
                                                                       HyScanSonarSyncType        sync_type);

/*
 * Асинхронный запрос на задание местоположения приёмных антенн относительно
 * центра масс судна.
 * Подробное описание параметров приводится в \link HyScanCoreTypes \endlink.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param source идентификатор источника данных;
 * \param position параметры местоположения приёмной антенны.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_sonar_set_position              (HyScanSonarControlModel   *model,
                                                                       HyScanSourceType           source,
                                                                       HyScanAntennaPosition     *position);

/*
 * Асинхронный запрос на задание времени приёма эхосигнала источником данных.
 * Если задаётся нулевое время, приём данных этим источником отключается.
 * Если время приёма отрицательное (<= -1.0), будет использоваться
 * автоматическое управление.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param source идентификатор источника данных;
 * \param receive_time время приёма эхосигнала, секунды.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_sonar_set_receive_time          (HyScanSonarControlModel   *model,
                                                                       HyScanSourceType           source,
                                                                       gdouble                    receive_time);

/*
 * Асинхронный запрос на перевод гидролокатора в рабочий режим.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink;
 * \param track_name название галса, в который записывать данные;
 * \param track_type тип галса.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_sonar_start                     (HyScanSonarControlModel   *model,
                                                                       const gchar               *track_name,
                                                                       HyScanTrackType            track_type);

/*
 * Асинхронный запрос на перевод гидролокатора в ждущий режим.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_sonar_stop                      (HyScanSonarControlModel   *model);

/*
 * Асинхронный запрос на выполнение одного цикла зондирования и приёма данных.
 * Гидролокатор должен поддерживать программное управление синхронизацией
 * излучения (#HYSCAN_SONAR_SYNC_SOFTWARE) и этот тип синхронизации должен быть
 * включён.
 *
 * \param model указатель на класс \link HyScanSonarControlModel \endlink.
 *
 * \return TRUE, если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean   hyscan_sonar_control_model_sonar_ping                      (HyScanSonarControlModel   *model);

G_END_DECLS

#endif /* __HYSCAN_SONAR_CONTROL_MODEL_H__ */
