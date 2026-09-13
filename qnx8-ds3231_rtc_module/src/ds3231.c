#include "ds3231.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <devctl.h>
#include <hw/i2c.h>


/* ============================================================
 * Internal Definitions
 * ============================================================ */

#define DS3231_ADDRESS_MIN  0x03
#define DS3231_ADDRESS_MAX  0x77

#define DS3231_CONTROL_INTCN    (1 << 2)
#define DS3231_CONTROL_RS1      (1 << 3)
#define DS3231_CONTROL_RS2      (1 << 4)
#define DS3231_CONTROL_A1IE     (1 << 0)
#define DS3231_CONTROL_A2IE     (1 << 1)

#define DS3231_STATUS_A1F       (1 << 0)
#define DS3231_STATUS_A2F       (1 << 1)
#define DS3231_STATUS_BSY       (1 << 2)
#define DS3231_STATUS_EN32KHZ   (1 << 3)
#define DS3231_STATUS_OSF       (1 << 7)

#define DS3231_HOUR_12H_BIT     (1 << 6)
#define DS3231_HOUR_PM_BIT      (1 << 5)


/* ============================================================
 * Utility
 * ============================================================ */

static uint8_t dec_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4) |
                     (value % 10U));
}


static uint8_t bcd_to_dec(uint8_t value)
{
    return (uint8_t)(((value >> 4) * 10U) +
                     (value & 0x0FU));
}


static bool valid_address(uint16_t address)
{
    return (address >= DS3231_ADDRESS_MIN &&
            address <= DS3231_ADDRESS_MAX);
}


static bool valid_datetime(const ds3231_datetime_t *dt)
{
    if (dt == NULL)
        return false;

    if (dt->seconds > 59)
        return false;

    if (dt->minutes > 59)
        return false;

    if (dt->hours > 23)
        return false;

    if (dt->day < DS3231_SUNDAY ||
        dt->day > DS3231_SATURDAY)
        return false;

    if (dt->date < 1 || dt->date > 31)
        return false;

    if (dt->month < 1 || dt->month > 12)
        return false;

    if (dt->year < 2000 || dt->year > 2199)
        return false;

    return true;
}


/* ============================================================
 * Mutex Helpers
 * ============================================================ */

static pthread_mutex_t *get_mutex(ds3231_t *dev)
{
    return (pthread_mutex_t *)dev->mutex;
}


static int lock_device(ds3231_t *dev)
{
    if (dev == NULL || dev->mutex == NULL)
        return EINVAL;

    return pthread_mutex_lock(get_mutex(dev));
}


static int unlock_device(ds3231_t *dev)
{
    if (dev == NULL || dev->mutex == NULL)
        return EINVAL;

    return pthread_mutex_unlock(get_mutex(dev));
}


/* ============================================================
 * I2C Read
 * ============================================================ */

static int i2c_read_registers(ds3231_t *dev,
                              uint8_t reg,
                              uint8_t *data,
                              uint16_t length)
{
    if (dev == NULL ||
        data == NULL ||
        length == 0)
        return EINVAL;

    struct
    {
        i2c_sendrecv_t x;
        uint8_t data[32];

    } packet;

    if (length > sizeof(packet.data))
        return EINVAL;

    memset(&packet, 0, sizeof(packet));

    packet.x.slave.addr = dev->address;
    packet.x.slave.fmt = I2C_ADDRFMT_7BIT;

    packet.x.send_len = 1;
    packet.x.recv_len = length;
    packet.x.stop = 1;

    packet.data[0] = reg;

    int status = devctl(dev->fd,
                        DCMD_I2C_SENDRECV,
                        &packet,
                        sizeof(packet),
                        NULL);

    if (status != EOK)
        return status;

    memcpy(data,
           packet.data,
           length);

    return EOK;
}


/* ============================================================
 * I2C Write
 * ============================================================ */

static int i2c_write_registers(ds3231_t *dev,
                                uint8_t reg,
                                const uint8_t *data,
                                uint16_t length)
{
    if (dev == NULL ||
        data == NULL ||
        length == 0)
        return EINVAL;

    struct
    {
        i2c_send_t send;
        uint8_t data[32];

    } packet;

    if (length + 1 > sizeof(packet.data))
        return EINVAL;

    memset(&packet, 0, sizeof(packet));

    packet.send.slave.addr = dev->address;
    packet.send.slave.fmt = I2C_ADDRFMT_7BIT;

    packet.send.len = length + 1;
    packet.send.stop = 1;

    packet.data[0] = reg;

    memcpy(&packet.data[1],
           data,
           length);

    int status = devctl(dev->fd,
                        DCMD_I2C_SEND,
                        &packet,
                        sizeof(packet),
                        NULL);

    if (status != EOK)
        return status;

    return EOK;
}


/* ============================================================
 * Raw Register Access
 * ============================================================ */

int ds3231_read_register(ds3231_t *dev,
                         uint8_t reg,
                         uint8_t *value)
{
    if (dev == NULL || value == NULL)
        return EINVAL;

    int status = lock_device(dev);

    if (status != EOK)
        return status;

    status = i2c_read_registers(dev,
                                reg,
                                value,
                                1);

    unlock_device(dev);

    return status;
}


int ds3231_write_register(ds3231_t *dev,
                          uint8_t reg,
                          uint8_t value)
{
    if (dev == NULL)
        return EINVAL;

    int status = lock_device(dev);

    if (status != EOK)
        return status;

    status = i2c_write_registers(dev,
                                 reg,
                                 &value,
                                 1);

    unlock_device(dev);

    return status;
}


/* ============================================================
 * Initialization
 * ============================================================ */

int ds3231_init(ds3231_t *dev,
                const ds3231_config_t *config)
{
    if (dev == NULL)
        return EINVAL;

    memset(dev, 0, sizeof(*dev));

    const char *device =
        DS3231_DEFAULT_DEVICE;

    uint16_t address =
        DS3231_DEFAULT_ADDRESS;

    if (config != NULL)
    {
        if (config->device != NULL)
            device = config->device;

        if (config->address != 0)
            address = config->address;
    }

    if (!valid_address(address))
        return EINVAL;

    dev->fd = open(device, O_RDWR);

    if (dev->fd == -1)
        return errno;

    dev->address = address;

    pthread_mutex_t *mutex =
        malloc(sizeof(pthread_mutex_t));

    if (mutex == NULL)
    {
        close(dev->fd);
        return ENOMEM;
    }

    pthread_mutex_init(mutex, NULL);

    dev->mutex = mutex;
    dev->initialized = true;

    return EOK;
}


/* ============================================================
 * Deinitialization
 * ============================================================ */

int ds3231_deinit(ds3231_t *dev)
{
    if (dev == NULL ||
        !dev->initialized)
        return EINVAL;

    pthread_mutex_t *mutex =
        get_mutex(dev);

    pthread_mutex_lock(mutex);

    close(dev->fd);

    dev->initialized = false;

    pthread_mutex_unlock(mutex);

    pthread_mutex_destroy(mutex);

    free(mutex);

    dev->mutex = NULL;
    dev->fd = -1;

    return EOK;
}


/* ============================================================
 * Address
 * ============================================================ */

int ds3231_set_address(ds3231_t *dev,
                       uint16_t address)
{
    if (dev == NULL ||
        !dev->initialized)
        return EINVAL;

    if (!valid_address(address))
        return EINVAL;

    int status = lock_device(dev);

    if (status != EOK)
        return status;

    dev->address = address;

    unlock_device(dev);

    return EOK;
}


uint16_t ds3231_get_address(ds3231_t *dev)
{
    if (dev == NULL ||
        !dev->initialized)
        return 0;

    int status = lock_device(dev);

    if (status != EOK)
        return 0;

    uint16_t address = dev->address;

    unlock_device(dev);

    return address;
}


/* ============================================================
 * Date / Time
 * ============================================================ */

int ds3231_set_datetime(ds3231_t *dev,
                        const ds3231_datetime_t *datetime)
{
    if (dev == NULL ||
        datetime == NULL ||
        !dev->initialized)
        return EINVAL;

    if (!valid_datetime(datetime))
        return EINVAL;

    uint8_t data[7];

    data[0] = dec_to_bcd(datetime->seconds);
    data[1] = dec_to_bcd(datetime->minutes);

    /* 24-hour mode */
    data[2] = dec_to_bcd(datetime->hours);

    data[3] = dec_to_bcd(datetime->day);
    data[4] = dec_to_bcd(datetime->date);
    data[5] = dec_to_bcd(datetime->month);

    /*
     * DS3231 stores years as 00-99.
     * Driver supports 2000-2099 here.
     */
    if (datetime->year < 2000 ||
        datetime->year > 2099)
        return EINVAL;

    data[6] = dec_to_bcd(
        (uint8_t)(datetime->year - 2000));

    int status = lock_device(dev);

    if (status != EOK)
        return status;

    status = i2c_write_registers(dev,
                                 DS3231_REG_SECONDS,
                                 data,
                                 7);

    unlock_device(dev);

    return status;
}


int ds3231_get_datetime(ds3231_t *dev,
                        ds3231_datetime_t *datetime)
{
    if (dev == NULL ||
        datetime == NULL ||
        !dev->initialized)
        return EINVAL;

    uint8_t data[7];

    int status = lock_device(dev);

    if (status != EOK)
        return status;

    status = i2c_read_registers(dev,
                                DS3231_REG_SECONDS,
                                data,
                                7);

    unlock_device(dev);

    if (status != EOK)
        return status;

    datetime->seconds = bcd_to_dec(data[0]);
    datetime->minutes = bcd_to_dec(data[1]);

    /*
     * Make sure returned hour is interpreted
     * as 24-hour mode.
     */
    if (data[2] & DS3231_HOUR_12H_BIT)
    {
        uint8_t hour = bcd_to_dec(data[2] & 0x1F);

        if (data[2] & DS3231_HOUR_PM_BIT)
        {
            if (hour != 12)
                hour += 12;
        }
        else
        {
            if (hour == 12)
                hour = 0;
        }

        datetime->hours = hour;
    }
    else
    {
        datetime->hours =
            bcd_to_dec(data[2] & 0x3F);
    }

    datetime->day =
        bcd_to_dec(data[3]);

    datetime->date =
        bcd_to_dec(data[4]);

    datetime->month =
        bcd_to_dec(data[5] & 0x1F);

    datetime->year =
        2000 + bcd_to_dec(data[6]);

    return EOK;
}


/* ============================================================
 * Time
 * ============================================================ */

int ds3231_set_time(ds3231_t *dev,
                     uint8_t hours,
                     uint8_t minutes,
                     uint8_t seconds)
{
    if (hours > 23 ||
        minutes > 59 ||
        seconds > 59)
        return EINVAL;

    uint8_t data[3];

    data[0] = dec_to_bcd(seconds);
    data[1] = dec_to_bcd(minutes);
    data[2] = dec_to_bcd(hours);

    int status = lock_device(dev);

    if (status != EOK)
        return status;

    status = i2c_write_registers(dev,
                                 DS3231_REG_SECONDS,
                                 data,
                                 3);

    unlock_device(dev);

    return status;
}


int ds3231_get_time(ds3231_t *dev,
                     uint8_t *hours,
                     uint8_t *minutes,
                     uint8_t *seconds)
{
    if (dev == NULL ||
        hours == NULL ||
        minutes == NULL ||
        seconds == NULL)
        return EINVAL;

    uint8_t data[3];

    int status = lock_device(dev);

    if (status != EOK)
        return status;

    status = i2c_read_registers(dev,
                                DS3231_REG_SECONDS,
                                data,
                                3);

    unlock_device(dev);

    if (status != EOK)
        return status;

    *seconds = bcd_to_dec(data[0]);
    *minutes = bcd_to_dec(data[1]);

    if (data[2] & DS3231_HOUR_12H_BIT)
    {
        uint8_t hour =
            bcd_to_dec(data[2] & 0x1F);

        if (data[2] & DS3231_HOUR_PM_BIT)
        {
            if (hour != 12)
                hour += 12;
        }
        else
        {
            if (hour == 12)
                hour = 0;
        }

        *hours = hour;
    }
    else
    {
        *hours =
            bcd_to_dec(data[2] & 0x3F);
    }

    return EOK;
}


/* ============================================================
 * Date
 * ============================================================ */

int ds3231_set_date(ds3231_t *dev,
                    uint16_t year,
                    uint8_t month,
                    uint8_t date,
                    uint8_t day)
{
    if (year < 2000 || year > 2099)
        return EINVAL;

    if (month < 1 || month > 12)
        return EINVAL;

    if (date < 1 || date > 31)
        return EINVAL;

    if (day < 1 || day > 7)
        return EINVAL;

    uint8_t data[5];

    data[0] = dec_to_bcd(0);
    data[1] = dec_to_bcd(0);
    data[2] = dec_to_bcd(0);
    data[3] = dec_to_bcd(date);
    data[4] = dec_to_bcd(month);

    /*
     * Write date registers separately.
     */
    uint8_t date_data[4];

    date_data[0] = dec_to_bcd(day);
    date_data[1] = dec_to_bcd(date);
    date_data[2] = dec_to_bcd(month);
    date_data[3] =
        dec_to_bcd((uint8_t)(year - 2000));

    int status = lock_device(dev);

    if (status != EOK)
        return status;

    status = i2c_write_registers(dev,
                                 DS3231_REG_DAY,
                                 date_data,
                                 4);

    unlock_device(dev);

    return status;
}


int ds3231_get_date(ds3231_t *dev,
                    uint16_t *year,
                    uint8_t *month,
                    uint8_t *date,
                    uint8_t *day)
{
    if (dev == NULL ||
        year == NULL ||
        month == NULL ||
        date == NULL ||
        day == NULL)
        return EINVAL;

    uint8_t data[4];

    int status = lock_device(dev);

    if (status != EOK)
        return status;

    status = i2c_read_registers(dev,
                                DS3231_REG_DAY,
                                data,
                                4);

    unlock_device(dev);

    if (status != EOK)
        return status;

    *day = bcd_to_dec(data[0]);
    *date = bcd_to_dec(data[1]);
    *month = bcd_to_dec(data[2] & 0x1F);
    *year = 2000 + bcd_to_dec(data[3]);

    return EOK;
}


/* ============================================================
 * Temperature
 * ============================================================ */

int ds3231_get_temperature(ds3231_t *dev,
                           float *temperature)
{
    if (dev == NULL ||
        temperature == NULL ||
        !dev->initialized)
        return EINVAL;

    uint8_t data[2];

    int status = lock_device(dev);

    if (status != EOK)
        return status;

    status = i2c_read_registers(dev,
                                DS3231_REG_TEMP_MSB,
                                data,
                                2);

    unlock_device(dev);

    if (status != EOK)
        return status;

    int16_t raw =
        (int16_t)(((uint16_t)data[0] << 8) |
                  data[1]);

    /*
     * Temperature resolution = 0.25 °C.
     */
    *temperature =
        (float)raw / 256.0f;

    return EOK;
}


/* ============================================================
 * Oscillator Stop Flag
 * ============================================================ */

int ds3231_is_oscillator_stopped(ds3231_t *dev,
                                 bool *stopped)
{
    if (dev == NULL ||
        stopped == NULL)
        return EINVAL;

    uint8_t status;

    int ret =
        ds3231_read_register(dev,
                             DS3231_REG_STATUS,
                             &status);

    if (ret != EOK)
        return ret;

    *stopped =
        (status & DS3231_STATUS_OSF) != 0;

    return EOK;
}


int ds3231_clear_oscillator_stop_flag(ds3231_t *dev)
{
    if (dev == NULL)
        return EINVAL;

    uint8_t status;

    int ret =
        ds3231_read_register(dev,
                             DS3231_REG_STATUS,
                             &status);

    if (ret != EOK)
        return ret;

    status &= ~DS3231_STATUS_OSF;

    return ds3231_write_register(dev,
                                 DS3231_REG_STATUS,
                                 status);
}


/* ============================================================
 * 32.768 kHz
 * ============================================================ */

int ds3231_enable_32khz(ds3231_t *dev,
                        bool enable)
{
    if (dev == NULL)
        return EINVAL;

    uint8_t status;

    int ret =
        ds3231_read_register(dev,
                             DS3231_REG_STATUS,
                             &status);

    if (ret != EOK)
        return ret;

    if (enable)
        status |= DS3231_STATUS_EN32KHZ;
    else
        status &= ~DS3231_STATUS_EN32KHZ;

    return ds3231_write_register(dev,
                                 DS3231_REG_STATUS,
                                 status);
}


/* ============================================================
 * SQW
 * ============================================================ */

int ds3231_set_sqw_frequency(ds3231_t *dev,
                             ds3231_sqw_frequency_t frequency)
{
    if (dev == NULL)
        return EINVAL;

    if (frequency > DS3231_SQW_8192HZ)
        return EINVAL;

    uint8_t control;

    int ret =
        ds3231_read_register(dev,
                             DS3231_REG_CONTROL,
                             &control);

    if (ret != EOK)
        return ret;

    control &= ~(DS3231_CONTROL_RS1 |
                 DS3231_CONTROL_RS2);

    switch (frequency)
    {
        case DS3231_SQW_1HZ:
            break;

        case DS3231_SQW_1024HZ:
            control |= DS3231_CONTROL_RS1;
            break;

        case DS3231_SQW_4096HZ:
            control |= DS3231_CONTROL_RS2;
            break;

        case DS3231_SQW_8192HZ:
            control |= DS3231_CONTROL_RS1 |
                       DS3231_CONTROL_RS2;
            break;

        default:
            return EINVAL;
    }

    /*
     * INTCN = 0 selects SQW output.
     */
    control &= ~DS3231_CONTROL_INTCN;

    return ds3231_write_register(dev,
                                 DS3231_REG_CONTROL,
                                 control);
}


int ds3231_enable_sqw(ds3231_t *dev,
                       bool enable)
{
    if (dev == NULL)
        return EINVAL;

    uint8_t control;

    int ret =
        ds3231_read_register(dev,
                             DS3231_REG_CONTROL,
                             &control);

    if (ret != EOK)
        return ret;

    if (enable)
        control &= ~DS3231_CONTROL_INTCN;
    else
        control |= DS3231_CONTROL_INTCN;

    return ds3231_write_register(dev,
                                 DS3231_REG_CONTROL,
                                 control);
}


/* ============================================================
 * Alarm 1
 * ============================================================ */

int ds3231_set_alarm1(ds3231_t *dev,
                      uint8_t seconds,
                      uint8_t minutes,
                      uint8_t hours,
                      uint8_t date,
                      ds3231_alarm1_mode_t mode)
{
    if (dev == NULL)
        return EINVAL;

    if (seconds > 59 ||
        minutes > 59 ||
        hours > 23 ||
        date < 1 ||
        date > 31)
        return EINVAL;

    if (mode > DS3231_ALARM1_MATCH_DATE_HOUR_MIN_SEC)
        return EINVAL;

    uint8_t data[4];

    data[0] = dec_to_bcd(seconds);
    data[1] = dec_to_bcd(minutes);
    data[2] = dec_to_bcd(hours);
    data[3] = dec_to_bcd(date);

    /*
     * A1M1..A1M4.
     */
    switch (mode)
    {
        case DS3231_ALARM1_MATCH_SECONDS:
            data[0] |= 0x80;
            data[1] |= 0x80;
            data[2] |= 0x80;
            data[3] |= 0x80;
            break;

        case DS3231_ALARM1_MATCH_MIN_SEC:
            data[1] |= 0x80;
            data[2] |= 0x80;
            data[3] |= 0x80;
            break;

        case DS3231_ALARM1_MATCH_HOUR_MIN_SEC:
            data[2] |= 0x80;
            data[3] |= 0x80;
            break;

        case DS3231_ALARM1_MATCH_DATE_HOUR_MIN_SEC:
            break;

        default:
            return EINVAL;
    }

    return i2c_write_registers(dev,
                                DS3231_REG_ALARM1_SEC,
                                data,
                                4);
}


int ds3231_enable_alarm1(ds3231_t *dev,
                         bool enable)
{
    uint8_t control;

    int ret =
        ds3231_read_register(dev,
                             DS3231_REG_CONTROL,
                             &control);

    if (ret != EOK)
        return ret;

    if (enable)
        control |= DS3231_CONTROL_A1IE;
    else
        control &= ~DS3231_CONTROL_A1IE;

    return ds3231_write_register(dev,
                                 DS3231_REG_CONTROL,
                                 control);
}


int ds3231_get_alarm1_flag(ds3231_t *dev,
                           bool *triggered)
{
    if (triggered == NULL)
        return EINVAL;

    uint8_t status;

    int ret =
        ds3231_read_register(dev,
                             DS3231_REG_STATUS,
                             &status);

    if (ret != EOK)
        return ret;

    *triggered =
        (status & DS3231_STATUS_A1F) != 0;

    return EOK;
}


int ds3231_clear_alarm1_flag(ds3231_t *dev)
{
    uint8_t status;

    int ret =
        ds3231_read_register(dev,
                             DS3231_REG_STATUS,
                             &status);

    if (ret != EOK)
        return ret;

    status &= ~DS3231_STATUS_A1F;

    return ds3231_write_register(dev,
                                 DS3231_REG_STATUS,
                                 status);
}


/* ============================================================
 * Alarm 2
 * ============================================================ */

int ds3231_set_alarm2(ds3231_t *dev,
                      uint8_t minutes,
                      uint8_t hours,
                      uint8_t date,
                      ds3231_alarm2_mode_t mode)
{
    if (dev == NULL)
        return EINVAL;

    if (minutes > 59 ||
        hours > 23 ||
        date < 1 ||
        date > 31)
        return EINVAL;

    if (mode > DS3231_ALARM2_MATCH_DATE_HOUR_MIN)
        return EINVAL;

    uint8_t data[3];

    data[0] = dec_to_bcd(minutes);
    data[1] = dec_to_bcd(hours);
    data[2] = dec_to_bcd(date);

    switch (mode)
    {
        case DS3231_ALARM2_MATCH_MINUTES:
            data[0] |= 0x80;
            data[1] |= 0x80;
            data[2] |= 0x80;
            break;

        case DS3231_ALARM2_MATCH_HOUR_MIN:
            data[1] |= 0x80;
            data[2] |= 0x80;
            break;

        case DS3231_ALARM2_MATCH_DATE_HOUR_MIN:
            data[2] |= 0x80;
            break;

        default:
            return EINVAL;
    }

    return i2c_write_registers(dev,
                                DS3231_REG_ALARM2_MIN,
                                data,
                                3);
}


int ds3231_enable_alarm2(ds3231_t *dev,
                         bool enable)
{
    uint8_t control;

    int ret =
        ds3231_read_register(dev,
                             DS3231_REG_CONTROL,
                             &control);

    if (ret != EOK)
        return ret;

    if (enable)
        control |= DS3231_CONTROL_A2IE;
    else
        control &= ~DS3231_CONTROL_A2IE;

    return ds3231_write_register(dev,
                                 DS3231_REG_CONTROL,
                                 control);
}


int ds3231_get_alarm2_flag(ds3231_t *dev,
                           bool *triggered)
{
    if (triggered == NULL)
        return EINVAL;

    uint8_t status;

    int ret =
        ds3231_read_register(dev,
                             DS3231_REG_STATUS,
                             &status);

    if (ret != EOK)
        return ret;

    *triggered =
        (status & DS3231_STATUS_A2F) != 0;

    return EOK;
}


int ds3231_clear_alarm2_flag(ds3231_t *dev)
{
    uint8_t status;

    int ret =
        ds3231_read_register(dev,
                             DS3231_REG_STATUS,
                             &status);

    if (ret != EOK)
        return ret;

    status &= ~DS3231_STATUS_A2F;

    return ds3231_write_register(dev,
                                 DS3231_REG_STATUS,
                                 status);
}


/* ============================================================
 * Aging Offset
 * ============================================================ */

int ds3231_set_aging_offset(ds3231_t *dev,
                            int8_t offset)
{
    if (dev == NULL)
        return EINVAL;

    return ds3231_write_register(
        dev,
        DS3231_REG_AGING,
        (uint8_t)offset);
}


int ds3231_get_aging_offset(ds3231_t *dev,
                            int8_t *offset)
{
    if (dev == NULL ||
        offset == NULL)
        return EINVAL;

    uint8_t value;

    int ret =
        ds3231_read_register(dev,
                             DS3231_REG_AGING,
                             &value);

    if (ret != EOK)
        return ret;

    *offset = (int8_t)value;

    return EOK;
}
