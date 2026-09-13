#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * DS3231 Default Configuration
 * ============================================================ */

#define DS3231_DEFAULT_DEVICE   "/dev/i2c1"
#define DS3231_DEFAULT_ADDRESS  0x68

/* DS3231 register addresses */

#define DS3231_REG_SECONDS      0x00
#define DS3231_REG_MINUTES      0x01
#define DS3231_REG_HOURS        0x02
#define DS3231_REG_DAY          0x03
#define DS3231_REG_DATE         0x04
#define DS3231_REG_MONTH        0x05
#define DS3231_REG_YEAR         0x06

#define DS3231_REG_ALARM1_SEC   0x07
#define DS3231_REG_ALARM1_MIN   0x08
#define DS3231_REG_ALARM1_HOUR  0x09
#define DS3231_REG_ALARM1_DATE  0x0A

#define DS3231_REG_ALARM2_MIN   0x0B
#define DS3231_REG_ALARM2_HOUR  0x0C
#define DS3231_REG_ALARM2_DATE  0x0D

#define DS3231_REG_CONTROL      0x0E
#define DS3231_REG_STATUS       0x0F
#define DS3231_REG_AGING        0x10
#define DS3231_REG_TEMP_MSB     0x11
#define DS3231_REG_TEMP_LSB     0x12


/* ============================================================
 * Days
 * ============================================================ */

typedef enum
{
    DS3231_SUNDAY = 1,
    DS3231_MONDAY,
    DS3231_TUESDAY,
    DS3231_WEDNESDAY,
    DS3231_THURSDAY,
    DS3231_FRIDAY,
    DS3231_SATURDAY

} ds3231_weekday_t;


/* ============================================================
 * Date / Time
 * ============================================================ */

typedef struct
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;

    uint8_t day;
    uint8_t date;
    uint8_t month;

    uint16_t year;

} ds3231_datetime_t;


/* ============================================================
 * SQW Output Frequency
 * ============================================================ */

typedef enum
{
    DS3231_SQW_1HZ = 0,
    DS3231_SQW_1024HZ,
    DS3231_SQW_4096HZ,
    DS3231_SQW_8192HZ

} ds3231_sqw_frequency_t;


/* ============================================================
 * Alarm 1
 * ============================================================ */

typedef enum
{
    DS3231_ALARM1_MATCH_SECONDS = 0,
    DS3231_ALARM1_MATCH_MIN_SEC,
    DS3231_ALARM1_MATCH_HOUR_MIN_SEC,
    DS3231_ALARM1_MATCH_DATE_HOUR_MIN_SEC

} ds3231_alarm1_mode_t;


/* ============================================================
 * Alarm 2
 * ============================================================ */

typedef enum
{
    DS3231_ALARM2_MATCH_MINUTES = 0,
    DS3231_ALARM2_MATCH_HOUR_MIN,
    DS3231_ALARM2_MATCH_DATE_HOUR_MIN

} ds3231_alarm2_mode_t;


/* ============================================================
 * Driver Configuration
 * ============================================================ */

typedef struct
{
    const char *device;
    uint16_t address;

} ds3231_config_t;


/* ============================================================
 * Driver Handle
 * ============================================================ */

typedef struct
{
    int fd;
    uint16_t address;
    bool initialized;

    /* Internal synchronization object */
    void *mutex;

} ds3231_t;


/* ============================================================
 * Initialization
 * ============================================================ */

int ds3231_init(ds3231_t *dev,
                const ds3231_config_t *config);

int ds3231_deinit(ds3231_t *dev);


/* ============================================================
 * Device Configuration
 * ============================================================ */

int ds3231_set_address(ds3231_t *dev,
                       uint16_t address);

uint16_t ds3231_get_address(ds3231_t *dev);


/* ============================================================
 * Date / Time
 * ============================================================ */

int ds3231_set_datetime(ds3231_t *dev,
                        const ds3231_datetime_t *datetime);

int ds3231_get_datetime(ds3231_t *dev,
                        ds3231_datetime_t *datetime);


/* ============================================================
 * Individual Time Functions
 * ============================================================ */

int ds3231_set_time(ds3231_t *dev,
                     uint8_t hours,
                     uint8_t minutes,
                     uint8_t seconds);

int ds3231_get_time(ds3231_t *dev,
                     uint8_t *hours,
                     uint8_t *minutes,
                     uint8_t *seconds);


/* ============================================================
 * Individual Date Functions
 * ============================================================ */

int ds3231_set_date(ds3231_t *dev,
                    uint16_t year,
                    uint8_t month,
                    uint8_t date,
                    uint8_t day);

int ds3231_get_date(ds3231_t *dev,
                    uint16_t *year,
                    uint8_t *month,
                    uint8_t *date,
                    uint8_t *day);


/* ============================================================
 * Temperature
 * ============================================================ */

int ds3231_get_temperature(ds3231_t *dev,
                           float *temperature);


/* ============================================================
 * Oscillator Status
 * ============================================================ */

int ds3231_is_oscillator_stopped(ds3231_t *dev,
                                 bool *stopped);

int ds3231_clear_oscillator_stop_flag(ds3231_t *dev);


/* ============================================================
 * 32.768 kHz Output
 * ============================================================ */

int ds3231_enable_32khz(ds3231_t *dev,
                        bool enable);


/* ============================================================
 * Square Wave Output
 * ============================================================ */

int ds3231_set_sqw_frequency(ds3231_t *dev,
                             ds3231_sqw_frequency_t frequency);

int ds3231_enable_sqw(ds3231_t *dev,
                       bool enable);


/* ============================================================
 * Alarm 1
 * ============================================================ */

int ds3231_set_alarm1(ds3231_t *dev,
                      uint8_t seconds,
                      uint8_t minutes,
                      uint8_t hours,
                      uint8_t date,
                      ds3231_alarm1_mode_t mode);

int ds3231_enable_alarm1(ds3231_t *dev,
                         bool enable);

int ds3231_get_alarm1_flag(ds3231_t *dev,
                           bool *triggered);

int ds3231_clear_alarm1_flag(ds3231_t *dev);


/* ============================================================
 * Alarm 2
 * ============================================================ */

int ds3231_set_alarm2(ds3231_t *dev,
                      uint8_t minutes,
                      uint8_t hours,
                      uint8_t date,
                      ds3231_alarm2_mode_t mode);

int ds3231_enable_alarm2(ds3231_t *dev,
                         bool enable);

int ds3231_get_alarm2_flag(ds3231_t *dev,
                           bool *triggered);

int ds3231_clear_alarm2_flag(ds3231_t *dev);


/* ============================================================
 * Aging Offset
 * ============================================================ */

int ds3231_set_aging_offset(ds3231_t *dev,
                            int8_t offset);

int ds3231_get_aging_offset(ds3231_t *dev,
                            int8_t *offset);


/* ============================================================
 * Raw Register Access
 * ============================================================ */

int ds3231_read_register(ds3231_t *dev,
                         uint8_t reg,
                         uint8_t *value);

int ds3231_write_register(ds3231_t *dev,
                          uint8_t reg,
                          uint8_t value);

#endif
