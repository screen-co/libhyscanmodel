/**
 * \file hyscan-sonar-model.h
 * \brief Заголовочный файл класса HyScanSonarModel - модель гидролокатора.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanSonarModel HyScanSonarModel - модель гидролокатора.
 *
 * \link HyScanSonarModel \endlink - модель, позволяющая асинхронно управлять
 * гидролокатором и получать информацию о его текущем состоянии.
 *
 * Любые изменения параметров сперва буферизируются, а затем отправляются
 * в гидролокатор. Изменения, накопленные в режиме ожидания отправляются в гидролокатор
 * после перехода гидролокатора в рабочее состояние. Изменения, накопленные в
 * рабочем режиме, отправляются сразу после истечения времени буферизации (полсекунды).
 *
 * В системе буферизации имеются исключения, так называемые "форсированные обновления":
 * команды #hyscan_sonar_model_sonar_start, #hyscan_sonar_model_sonar_ping и
 * #hyscan_sonar_model_sonar_stop выполняются практически немедленно - максимальная
 * задержка составляет 0.05сек.
 *
 * \link HyScanSonarModel \endlink позволяет осуществлять наиболее точное управление
 * гидролокатором и не содержит связей между источниками данных. Для создания подобных
 * связей рекомендуется применять наследование или композицию с данным классом.
 *
 * Объект \link HyScanSonarModel \endlink  можно создать функцией #hyscan_sonar_model_new.
 *
 * В случае успешного запуска процесса применения изменений, испускается сигнал
 * "sonar-control-state-changed", извещающий об изменении состояния системы управления
 * гидролокатором. Система управления гидролокатором может быть занята или свободна,
 * её текущее состояние можно проверить функцией #hyscan_sonar_model_get_sonar_control_state.
 * Прототип обработчика сигнала "sonar-control-state-changed":
 *
 * \code
 * void sonar_control_state_chaged_cb (HyScanSonarModel *sonar_model,
 *                                     gpointer          user_data);
 * \endcode
 *
 * После применения изменений, испускается сигнал "sonar-params-updated".
 * Прототип обработчика сигнала "sonar-params-updated":
 *
 * \code
 * void sonar_params_updated_cb (HyScanSonarModel *sonar_model,
 *                               gboolean          result,
 *                               gpointer          user_data);
 * \endcode
 *
 * где:
 * - result - результат применения изменений.
 *
 * Сигнал "sonar-params-updated" вернет в результате TRUE только в случае, если изменения
 * успешно применены.
 *
 * При переводе гидролокатора в рабочее состояние функцией #hyscan_sonar_model_sonar_start, создаётся
 * новый галс - испускается исгнал "active-track-changed":
 *
 * \code
 * void active_track_changed_cb (HyScanSonarModel *sonar_model,
 *                               gchar            *track_name,
 *                               gpointer          user_data);
 * \endcode
 *
 * \warning Данный класс корректно работает только с GMainLoop, кроме того
 * он не является потокобезопасным.
 */
#ifndef __HYSCAN_SONAR_MODEL_H__
#define __HYSCAN_SONAR_MODEL_H__

#include <hyscan-sonar-control.h>
#include "hyscan-db-info.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_MODEL            \
        (hyscan_sonar_model_get_type ())

#define HYSCAN_SONAR_MODEL(obj)            \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_MODEL, HyScanSonarModel))

#define HYSCAN_IS_SONAR_MODEL(obj)         \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_MODEL))

#define HYSCAN_SONAR_MODEL_CLASS(klass)    \
        (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_MODEL, HyScanSonarModelClass))

#define HYSCAN_IS_SONAR_MODEL_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_MODEL))

#define HYSCAN_SONAR_MODEL_GET_CLASS(obj)  \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_MODEL, HyScanSonarModelClass))

typedef struct _HyScanSonarModel HyScanSonarModel;
typedef struct _HyScanSonarModelPrivate HyScanSonarModelPrivate;
typedef struct _HyScanSonarModelClass HyScanSonarModelClass;

struct _HyScanSonarModel
{
  GObject parent_instance;

  HyScanSonarModelPrivate *priv;
};

struct _HyScanSonarModelClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                    hyscan_sonar_model_get_type                    (void);

/**
 * Создаёт объект \link HyScanSonarModel \endlink.
 *
 * \param sonar_control указатель на объект \link HyScanSonarControl \endlink;
 * \param db_info указатель на объект \link HyScanDBInfo \endlink.
 *
 * \return Объект \link HyScanSonarModel \endlink.
 */
HYSCAN_API
HyScanSonarModel*        hyscan_sonar_model_new                         (HyScanSonarControl         *sonar_control,
                                                                         HyScanDBInfo               *db_info);

/**
 * Вызывает отправку изменений вне зависимости от режима работы гидролокатора.
 * Изменения применяются в режима "форсированного обновления".
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink.
 */
HYSCAN_API
void                     hyscan_sonar_model_flush                       (HyScanSonarModel           *model);

/**
 * Проверяет состояние системы управления гидролокатором.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink.
 *
 * \return TRUE - если система управления свободна, FALSE - в остальных случаях.
 */
HYSCAN_API
gboolean                 hyscan_sonar_model_get_sonar_control_state     (HyScanSonarModel           *model);

/**
 * Возвращает копию объекта \link HyScanSonarControl \endlink, связанную с этой моделью.
 * После использования объекта, необходимо его освободить (см. g_object_unref).
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink.
 *
 * \return указатель на объект \link HyScanSonarControl \endlink.
 */
HYSCAN_API
HyScanSonarControl*      hyscan_sonar_model_get_sonar_control           (HyScanSonarModel           *model);

/**
 * Включает или выключает приём данных на указанном порту.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param port_name название порта;
 * \param enabled включить или выключить.
 */
HYSCAN_API
void                     hyscan_sonar_model_sensor_set_enable           (HyScanSonarModel           *model,
                                                                         const gchar                *port_name,
                                                                         gboolean                    enabled);

/**
 * Задаёт режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_VIRTUAL.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param port_name название порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс.
 */
HYSCAN_API
void                     hyscan_sonar_model_sensor_set_virtual_params   (HyScanSonarModel           *model,
                                                                         const gchar                *port_name,
                                                                         guint                       channel,
                                                                         gint64                      time_offset);

/**
 * Задаёт режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UART.
 *
 * В эту функцию передаются идентификаторы устойства UART и режима его работы
 * из списка допустимых значений (см. #hyscan_sensor_control_list_uart_devices и
 * #hyscan_sensor_control_list_uart_modes).
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param port_name название порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс;
 * \param protocol протокол обмена данными с датчиком;
 * \param device идентификатор устройства UART;
 * \param mode идентификатор режима работы устройства UART.
 */
HYSCAN_API
void                     hyscan_sonar_model_sensor_set_uart_params      (HyScanSonarModel           *model,
                                                                         const gchar                *port_name,
                                                                         guint                       channel,
                                                                         gint64                      time_offset,
                                                                         HyScanSensorProtocolType    protocol,
                                                                         guint                       device,
                                                                         guint                       mode);

/**
 * Задаёт режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UDP_IP.
 *
 * В эту функцию передаётся идентфикатор IP адреса из списка допустимых значений
 * (см. #hyscan_sensor_control_list_ip_addresses).
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param port_name название порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс;
 * \param protocol протокол обмена данными с датчиком;
 * \param addr идентификатор IP адреса, по которому принимать данные;
 * \param port номер UDP порта, по которому принимать данные.
 */
HYSCAN_API
void                     hyscan_sonar_model_sensor_set_udp_ip_params    (HyScanSonarModel           *model,
                                                                         const gchar                *port_name,
                                                                         guint                       channel,
                                                                         gint64                      time_offset,
                                                                         HyScanSensorProtocolType    protocol,
                                                                         guint                       addr,
                                                                         guint16                     port);

/**
 * Задаёт информацию о местоположении датчика относительно центра масс судна.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param port_name название порта;
 * \param position параметры местоположения приёмной антенны.
 */
HYSCAN_API
void                     hyscan_sonar_model_sensor_set_position         (HyScanSonarModel           *model,
                                                                         const gchar                *port_name,
                                                                         HyScanAntennaPosition       position);

/**
 * Проверяет, включен ли приём данных по указанному порту.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param port_name название порта.
 *
 * \return TRUE - включен, FALSE - выключен или произошла ошибка.
 */
HYSCAN_API
gboolean                 hyscan_sonar_model_sensor_is_enabled           (HyScanSonarModel           *model,
                                                                         const gchar                *port_name);

/**
 * Получает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_VIRTUAL.
 *
 * Выходные параметры channel, time_offset, не могут быть NULL.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param port_name название порта;
 * \param channel возвращает номер канала;
 * \param time_offset возвращает коррекцию времени приёма данных, мкс.
 */
HYSCAN_API
void                     hyscan_sonar_model_sensor_get_virtual_params   (HyScanSonarModel           *model,
                                                                         const gchar                *port_name,
                                                                         guint                      *channel,
                                                                         gint64                     *time_offset);

/**
 * Получает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UART.
 *
 * Выходные параметры channel, time_offset, protocol, device, mode, не могут быть NULL.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param port_name название порта;
 * \param channel возвращает номер канала;
 * \param time_offset возвращает коррекцию времени приёма данных, мкс;
 * \param protocol возвращает протокол обмена данными с датчиком;
 * \param device возвращает идентификатор устройства UART;
 * \param mode возвращает идентификатор режима работы устройства UART.
 */
HYSCAN_API
void                     hyscan_sonar_model_sensor_get_uart_params      (HyScanSonarModel           *model,
                                                                         const gchar                *port_name,
                                                                         guint                      *channel,
                                                                         gint64                     *time_offset,
                                                                         HyScanSensorProtocolType   *protocol,
                                                                         guint                      *device,
                                                                         guint                      *mode);

/**
 * Получает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UDP_IP.
 *
 * Выходные параметры channel, time_offset, protocol, addr, port, не могут быть NULL.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param port_name название порта;
 * \param channel возвращает номер канала;
 * \param time_offset возвращает коррекцию времени приёма данных, мкс;
 * \param protocol возвращает протокол обмена данными с датчиком;
 * \param addr возвращает идентификатор IP адреса, по которому принимаются данные;
 * \param port возвращает номер UDP порта, по которому принимаются данные.
 */
HYSCAN_API
void                     hyscan_sonar_model_sensor_get_udp_ip_params    (HyScanSonarModel           *model,
                                                                         const gchar                *port_name,
                                                                         guint                      *channel,
                                                                         gint64                     *time_offset,
                                                                         HyScanSensorProtocolType   *protocol,
                                                                         guint                      *addr,
                                                                         guint16                    *port);

/**
 * Получает информацию о местоположении приёмных антенн относительно центра масс судна.
 * Возвращаемое значение должно быть освобождено после использования (см. #g_free).
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param port_name название порта.
 *
 * \return указатель на HyScanAntennaPosition, либо NULL, в случае ошибки.
 */
HYSCAN_API
HyScanAntennaPosition*   hyscan_sonar_model_sensor_get_position         (HyScanSonarModel           *model,
                                                                         const gchar                *port_name);

/**
 * Включает или выключает формирование сигнала генератором.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param enabled включён или выключен.
 */
HYSCAN_API
void                     hyscan_sonar_model_gen_set_enable              (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         gboolean                    enabled);

/**
 * Задаёт преднастроенный режим работы генератора.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param preset идентификатор преднастройки.
 */
HYSCAN_API
void                     hyscan_sonar_model_gen_set_preset              (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         guint                       preset);

/**
 * Задаёт автоматический режим работы генератора.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param signal_type тип сигнала.
 */
HYSCAN_API
void                     hyscan_sonar_model_gen_set_auto                (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         HyScanGeneratorSignalType   signal_type);

/**
 * Задаёт упрощённый режим работы генератора.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param signal_type тип сигнала;
 * \param power энергия сигнала, проценты.
 */
HYSCAN_API
void                     hyscan_sonar_model_gen_set_simple              (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         HyScanGeneratorSignalType   signal_type,
                                                                         gdouble                     power);

/**
 * Задаёт расширенный режим работы генератора.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param signal_type тип сигнала;
 * \param duration длительность сигнала, с;
 * \param power энергия сигнала, проценты.
 */
HYSCAN_API
void                     hyscan_sonar_model_gen_set_extended            (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         HyScanGeneratorSignalType   signal_type,
                                                                         gdouble                     duration,
                                                                         gdouble                     power);

/**
 * Проверяет, включено или выключено формирование сигнала генератором.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных.
 *
 * \return TRUE - включёно, FALSE - выключено или произошла ошибка.
 */
HYSCAN_API
gboolean                 hyscan_sonar_model_gen_is_enabled              (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type);

/**
 * Получает режим работы генератора.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных.
 *
 * \return режим работы генератора, либо HYSCAN_GENERATOR_MODE_INVALID, в случае ошибки.
 */
HYSCAN_API
HyScanGeneratorModeType  hyscan_sonar_model_gen_get_mode                (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type);

/**
 * Получает параметры преднастроенного режима работы генератора.
 *
 * Выходной параметр preset не может быть NULL.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param preset возвращает идентификатор преднастройки.
 */
HYSCAN_API
void                     hyscan_sonar_model_gen_get_preset_params       (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         guint                      *preset);

/**
 * Получает параметры автоматического режима работы генератора.
 *
 * Выходной параметр signal_type не может быть NULL.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param signal_type возвращает тип сигнала.
 */
HYSCAN_API
void                     hyscan_sonar_model_gen_get_auto_params         (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         HyScanGeneratorSignalType  *signal_type);

/**
 * Получает параметры упрощённого режима работы генератора.
 *
 * Выходные параметры signal_type, power не могут быть NULL.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param signal_type возвращает тип сигнала;
 * \param power возвращает энергию сигнала, проценты.
 */
HYSCAN_API
void                     hyscan_sonar_model_gen_get_simple_params       (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         HyScanGeneratorSignalType  *signal_type,
                                                                         gdouble                    *power);

/**
 * Получает параметры расширенного режима работы генератора.
 *
 * Выходные параметры signal_type, duration, power не могут быть NULL.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param signal_type возвращает тип сигнала;
 * \param duration возвращает длительность сигнала, с;
 * \param power возвращает энергию сигнала, проценты.
 */
HYSCAN_API
void                     hyscan_sonar_model_gen_get_extended_params     (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         HyScanGeneratorSignalType  *signal_type,
                                                                         gdouble                    *duration,
                                                                         gdouble                    *power);

/**
 * Функция включает или выключает систему ВАРУ.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param enabled включёна или выключена.
 */
HYSCAN_API
void                     hyscan_sonar_model_tvg_set_enable              (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         gboolean                    enabled);

/**
 * Задаёт автоматический режим управления системой ВАРУ.
 *
 * Если в качестве значений параметров уровня сигнала и (или) чувствительности
 * передать отрицательное число, будут установлены значения по умолчанию.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param level целевой уровень сигнала;
 * \param sensitivity чувствительность автомата регулировки.
 */
HYSCAN_API
void                     hyscan_sonar_model_tvg_set_auto                (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         gdouble                     level,
                                                                         gdouble                     sensitivity);

/**
 * Задаёт постоянный уровень усиления системой ВАРУ.
 *
 * Уровень усиления должен находится в пределах значений, возвращаемых
 * функцией #hyscan_tvg_control_get_gain_range.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param gain коэффициент усиления, дБ;
 */
HYSCAN_API
void                     hyscan_sonar_model_tvg_set_constant            (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         gdouble                     gain);

/**
 * Задаёт линейное увеличение усиления в дБ на 100 метров.
 *
 * Начальный уровень усиления должен находится в пределах значений, возвращаемых
 * функцией #hyscan_tvg_control_get_gain_range. Величина изменения усиления должна
 * находится в пределах от 0 до 100 дБ.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param gain0 начальный уровень усиления, дБ;
 * \param step величина изменения усиления каждые 100 метров, дБ.
 */
HYSCAN_API
void                     hyscan_sonar_model_tvg_set_linear_db           (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         gdouble                     gain0,
                                                                         gdouble                     step);

/**
 * Задаёт логарифмический вид закона усиления системой ВАРУ.
 *
 * Начальный уровень усиления должен находится в пределах значений, возвращаемых
 * функцией #hyscan_tvg_control_get_gain_range. Значение коэффициента отражения
 * цели должно находится в пределах от 0 дБ до 100 дБ. Значение коэффициента
 * затухания должно находится в пределах от 0 дБ/м до 1 дБ/м.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param gain0 начальный уровень усиления, дБ;
 * \param beta коэффициент отражения цели, дБ;
 * \param alpha коэффициент затухания, дБ/м.
 */
HYSCAN_API
void                     hyscan_sonar_model_tvg_set_logarithmic         (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         gdouble                     gain0,
                                                                         gdouble                     beta,
                                                                         gdouble                     alpha);

/**
 * Проверяет, включена или выключена система ВАРУ.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных.
 *
 * \return TRUE - включёна, FALSE - в остальных случаях.
 */
HYSCAN_API
gboolean                 hyscan_sonar_model_tvg_is_enabled              (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type);

/**
 * Получает режим работы системы ВАРУ.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных.
 *
 * \return режим работы системы ВАРУ, либо HYSCAN_TVG_MODE_INVALID, в случае ошибки.
 */
HYSCAN_API
HyScanTVGModeType        hyscan_sonar_model_tvg_get_mode                (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type);

/**
 * Проверяет, работает ли система ВАРУ в полностью автоматическом режиме.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных.
 *
 * \return TRUE - если система ВАРУ работает в полностью автоматическом режиме, FALSE - в остальных случаях.
 */
HYSCAN_API
gboolean                 hyscan_sonar_model_tvg_is_auto                 (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type);

/**
 * Получает параметры автоматического режим управления системой ВАРУ.
 *
 * Выходные параметры level, sensitivity не могут быть NULL.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param level возвращает целевой уровень сигнала;
 * \param sensitivity возвращает чувствительность автомата регулировки.
 */
HYSCAN_API
void                     hyscan_sonar_model_tvg_get_auto_params         (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         gdouble                    *level,
                                                                         gdouble                    *sensitivity);

/**
 * Получает параметры режима управления системой ВАРУ при постоянном уровне усиления.
 *
 * Выходной параметр gain не может быть NULL.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param gain возвращает коэффициент усиления, дБ.
 */
HYSCAN_API
void                     hyscan_sonar_model_tvg_get_const_params        (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         gdouble                    *gain);

/**
 * Получает параметры режима управления усилением системой ВАРУ по линейному закону.
 *
 * Выходные параметры gain, step не могут быть NULL.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param gain возвращает начальный уровень усиления, дБ;
 * \param step возвращает величину изменения усиления каждые 100 метров, дБ.
 */
HYSCAN_API
void                     hyscan_sonar_model_tvg_get_linear_db_params    (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         gdouble                    *gain,
                                                                         gdouble                    *step);

/**
 * Получает параметры режима управления усилением системой ВАРУ по логарифмическому закону.
 *
 * Выходные параметры gain, beta, alpha не могут быть NULL.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param gain возвращает начальный уровень усиления, дБ;
 * \param beta возвращает коэффициент отражения цели, дБ;
 * \param alpha возвращает коэффициент затухания, дБ/м.
 */
HYSCAN_API
void                     hyscan_sonar_model_tvg_get_logarithmic_params  (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         gdouble                    *gain,
                                                                         gdouble                    *beta,
                                                                         gdouble                    *alpha);

/**
 * Задаёт информацию о местоположении приёмных антенн относительно центра масс судна.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param position параметры местоположения приёмной антенны.
 */
HYSCAN_API
void                     hyscan_sonar_model_sonar_set_position          (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         HyScanAntennaPosition       position);

/**
 * Задаёт время приёма эхосигнала источником данных.
 *
 * Если время установленно нулевым, приём данных этим источником отключается. Если время
 * приёма отрицательное (<= -1.0), будет использоваться автоматическое управление.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param receive_time время приёма эхосигнала, секунды.
 */
HYSCAN_API
void                     hyscan_sonar_model_set_receive_time            (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         gdouble                     receive_time);

/**
 * Задаёт дальность работы гидролокатора.
 *
 * Если задать нулевую дальность, приём данных этим источником отключается. Если дальность отрицательная (<= -1.0),
 * будет использоваться автоматическое управление.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных;
 * \param distance дальность, м.
 */
HYSCAN_API
void                     hyscan_sonar_model_set_distance                (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type,
                                                                         gdouble                     distance);

/**
 * Задаёт тип синхронизации излучения.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param sync_type тип синхронизации излучения.
 */
HYSCAN_API
void                     hyscan_sonar_model_set_sync_type               (HyScanSonarModel           *model,
                                                                         HyScanSonarSyncType         sync_type);

/**
 * Задаёт тип следующего записываемого галса.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param track_type тип галса.
 */
HYSCAN_API
void                     hyscan_sonar_model_set_track_type              (HyScanSonarModel           *model,
                                                                         HyScanTrackType             track_type);

/**
 * Переводит гидролокатор в рабочий режим и включает запись данных.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink.
 */
HYSCAN_API
void                     hyscan_sonar_model_sonar_start                 (HyScanSonarModel           *model);

/**
 * Переводит гидролокатор в ждущий режим и отключает запись данных.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink.
 */
HYSCAN_API
void                     hyscan_sonar_model_sonar_stop                  (HyScanSonarModel           *model);

/**
 * Выполняет один цикл зондирования и приёма данных.
 *
 * Для использования этой функции гидролокатор должен поддерживать программное управление
 * синхронизацией излучения (#HYSCAN_SONAR_SYNC_SOFTWARE) и этот тип синхронизации должен
 * быть включён.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink.
 */
HYSCAN_API
void                     hyscan_sonar_model_sonar_ping                  (HyScanSonarModel           *model);

/**
 * Получает информацию о местоположении приёмных антенн относительно центра масс судна.
 *
 * Возвращаемое значение должно быть освобождено после использования (см. #g_free).
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных.
 *
 * \return указатель на HyScanAntennaPosition, либо NULL, в случае ошибки.
 */
HYSCAN_API
HyScanAntennaPosition   *hyscan_sonar_model_sonar_get_position          (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type);

/**
 * Получает время приёма эхосигнала источником данных.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных.
 *
 * \return Время приёма эхосинала, либо G_MINDOUBLE, в случае ошибки.
 */
HYSCAN_API
gdouble                  hyscan_sonar_model_get_receive_time            (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type);

/**
 * Получает дальность источника данных.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных.
 *
 * \return Дальность источника данных, либо G_MINDOUBLE, в случае ошибки.
 */
HYSCAN_API
gdouble                  hyscan_sonar_model_get_distance                (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type);

/**
 * Получает максимальную дальность источника данных.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink;
 * \param source_type идентификатор источника данных.
 *
 * \return Максимальная дальность источника данных, либо G_MINDOUBLE, в случае ошибки.
 */
HYSCAN_API
gdouble                  hyscan_sonar_model_get_max_distance            (HyScanSonarModel           *model,
                                                                         HyScanSourceType            source_type);

/**
 * Получает скорость звука.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink.
 *
 * \return Скорость звука в м/c, либо G_MINDOUBLE, в случае ошибки.
 */
HYSCAN_API
gdouble                  hyscan_sonar_model_get_sound_velocity          (HyScanSonarModel           *model);


/**
 * Получает тип синхронизации.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink.
 *
 * \return Тип синхронизации \link HyScanSonarSyncType \endlink, либо HYSCAN_SONAR_SYNC_INVALID, в случае ошибки.
 */
HYSCAN_API
HyScanSonarSyncType      hyscan_sonar_model_get_sync_type               (HyScanSonarModel           *model);

/**
 * Получает тип галса.
 *
 * Возвращаемое значение может отличаться от типа текущего записываемого галса.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink.
 *
 * \return Тип галса \link HyScanTrackType \endlink, либо HYSCAN_TRACK_UNSPECIFIED, в случае ошибки.
 */
HYSCAN_API
HyScanTrackType          hyscan_sonar_model_get_track_type              (HyScanSonarModel           *model);

/**
 * Получает состояние записи.
 *
 * \param model указатель на объект \link HyScanSonarModel \endlink.
 *
 * \return TRUE - если гидролокатор в рабочем режиме, FALSE - если гидролокатор в режиме
 * ожидания, либо произошла ошибка.
 */
HYSCAN_API
gboolean                 hyscan_sonar_model_get_record_state            (HyScanSonarModel           *model);

G_END_DECLS

#endif /* __HYSCAN_SONAR_MODEL_H__ */
