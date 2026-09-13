#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "mpu6050.h"


int main(void)
{
    mpu6050_t imu;

    /*
     * Use default configuration:
     *
     * /dev/i2c1
     * 0x68
     * +/-2g
     * +/-250 dps
     */
    int status =
        mpu6050_init_default(&imu);

    if (status != EOK)
    {
        printf("MPU6050 initialization failed: %d\n",
               status);

        return EXIT_FAILURE;
    }


    printf("MPU6050 initialized successfully\n");


    /*
     * Configure ranges if required.
     */
    status =
        mpu6050_set_accel_range(
            &imu,
            MPU6050_ACCEL_RANGE_2G);

    if (status != EOK)
    {
        printf("Accelerometer configuration failed: %d\n",
               status);

        mpu6050_deinit(&imu);

        return EXIT_FAILURE;
    }


    status =
        mpu6050_set_gyro_range(
            &imu,
            MPU6050_GYRO_RANGE_250DPS);

    if (status != EOK)
    {
        printf("Gyroscope configuration failed: %d\n",
               status);

        mpu6050_deinit(&imu);

        return EXIT_FAILURE;
    }


    int shake_detected = 0;


    while (1)
    {
        mpu6050_data_t data;


        /*
         * One I2C transaction obtains:
         *
         * Accel
         * Temperature
         * Gyro
         */
        status =
            mpu6050_read_all(&imu, &data);


        if (status != EOK)
        {
            printf("MPU6050 read failed: %d\n",
                   status);

            break;
        }


        float magnitude =
            sqrtf(
                data.accel.x * data.accel.x +
                data.accel.y * data.accel.y +
                data.accel.z * data.accel.z
            );


        printf(
            "ACC "
            "X=%+.2f "
            "Y=%+.2f "
            "Z=%+.2f "
            "| MAG=%.2f g "
            "| TEMP=%.2f C "
            "| GYRO "
            "X=%+.2f "
            "Y=%+.2f "
            "Z=%+.2f dps\n",

            data.accel.x,
            data.accel.y,
            data.accel.z,

            magnitude,

            data.temperature,

            data.gyro.x,
            data.gyro.y,
            data.gyro.z
        );


        /*
         * Application-level shake detection.
         *
         * NOT part of the driver.
         */
        if (!shake_detected &&
            magnitude > 2.0f)
        {
            printf("SHAKE DETECTED!\n");

            shake_detected = 1;
        }


        if (shake_detected &&
            magnitude < 1.3f)
        {
            shake_detected = 0;
        }


        usleep(100000);
    }


    mpu6050_deinit(&imu);

    return EXIT_SUCCESS;
}
