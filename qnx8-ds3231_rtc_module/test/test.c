#include <stdio.h>
#include <stdlib.h>

#include "ds3231.h"

int main(void)
{
    ds3231_t rtc;

    ds3231_config_t config = {
        .device = "/dev/i2c1",
        .address = 0x68
    };

    int status;

    /* Initialize */
    status = ds3231_init(&rtc, &config);

    if (status != EOK)
    {
        printf("DS3231 initialization failed: %d\n",
               status);

        return EXIT_FAILURE;
    }

    printf("DS3231 initialized successfully\n");


    /* --------------------------------------------------------
     * Read date/time
     * -------------------------------------------------------- */

    ds3231_datetime_t datetime;

    status = ds3231_get_datetime(&rtc,
                                 &datetime);

    if (status != EOK)
    {
        printf("Failed to read date/time: %d\n",
               status);

        ds3231_deinit(&rtc);

        return EXIT_FAILURE;
    }

    printf("\nCurrent RTC time:\n");

    printf("%04u-%02u-%02u "
           "%02u:%02u:%02u "
           "Day=%u\n",

           datetime.year,
           datetime.month,
           datetime.date,

           datetime.hours,
           datetime.minutes,
           datetime.seconds,

           datetime.day);


    /* --------------------------------------------------------
     * Temperature
     * -------------------------------------------------------- */

    float temperature;

    status = ds3231_get_temperature(&rtc,
                                    &temperature);

    if (status == EOK)
    {
        printf("Temperature: %.2f C\n",
               temperature);
    }
    else
    {
        printf("Temperature read failed: %d\n",
               status);
    }


    /* --------------------------------------------------------
     * Oscillator status
     * -------------------------------------------------------- */

    bool stopped;

    status = ds3231_is_oscillator_stopped(&rtc,
                                          &stopped);

    if (status == EOK)
    {
        if (stopped)
            printf("WARNING: RTC oscillator stopped\n");
        else
            printf("RTC oscillator running normally\n");
    }


    /* --------------------------------------------------------
     * 32.768 kHz output
     * -------------------------------------------------------- */

    status = ds3231_enable_32khz(&rtc, true);

    if (status == EOK)
        printf("32.768 kHz output enabled\n");


    /* --------------------------------------------------------
     * SQW 1 Hz
     * -------------------------------------------------------- */

    status = ds3231_set_sqw_frequency(
        &rtc,
        DS3231_SQW_1HZ);

    if (status == EOK)
        printf("SQW configured to 1 Hz\n");


    /* --------------------------------------------------------
     * Close driver
     * -------------------------------------------------------- */

    ds3231_deinit(&rtc);

    printf("\nDS3231 driver closed\n");

    return EXIT_SUCCESS;
}
