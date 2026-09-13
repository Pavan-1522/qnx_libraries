<div align="center">

# 🧭 MPU6050 6-Axis IMU Driver
### Software Datasheet

[![Platform](https://img.shields.io/badge/Platform-QNX_Neutrino-blue?style=for-the-badge&logo=qnx)](#)
[![Interface](https://img.shields.io/badge/Interface-I%C2%B2C-C51A4A?style=for-the-badge)](#)
[![Language](https://img.shields.io/badge/Language-C_(C11)-00599C?style=for-the-badge&logo=c)](#)

*A thread-safe C API for the InvenSense MPU6050 6-axis accelerometer + gyroscope, over I²C on QNX.*

</div>

---

**Target platform:** QNX Neutrino (POSIX threads / `pthread`, `devctl()` I²C resource manager)
**Language:** C (C11)
**Files:** `mpu6050.h`, `mpu6050.c`
**Scope:** I²C-level register configuration and calibrated sensor reads (accelerometer, gyroscope, temperature) for a single MPU6050 device

---

## 🎯 1. Overview

This driver provides a thread-safe C API for a single **MPU6050** 6-axis motion sensor (3-axis accelerometer + 3-axis gyroscope + on-die temperature) on a QNX I²C bus. It owns register configuration and raw-to-physical-unit conversion only — it has **no** sensor fusion, filtering/complementary-filter math, orientation estimation, or gesture logic inside it. It is meant to sit underneath a separate sensing/estimation task in a multithreaded QNX application.

| Property | Value |
|---|---|
| **Axes measured** | 6 (accel X/Y/Z, gyro X/Y/Z) + on-die temperature |
| **Accelerometer output** | Signed float, units of **g** |
| **Gyroscope output** | Signed float, units of **degrees/second** |
| **Thread safety** | Yes — mutex-protected on every data/config call |
| **Configuration** | Fully caller-supplied via `mpu6050_config_t`, or one-call defaults via `mpu6050_init_default()` |
| **Identity check** | `WHO_AM_I` register verified automatically at init |
| **Error reporting** | Standard `errno`-style `int` codes (`EOK`, `EINVAL`, `ENODEV`, ...) on every call |
| **Dependencies** | `<stdint.h>`, `<pthread.h>`, `<devctl.h>`, `<hw/i2c.h>` |

---

## ⚙️ 2. Hardware Model

The MPU6050 exposes its data and configuration through I²C registers rather than discrete pins — there's no per-pin wiring table like a motor driver, but the equivalent "contract" is the register map this driver touches:

| Register | Address | Function |
|:---:|:---:|---|
| `REG_PWR_MGMT_1` | `0x6B` | Power management — sleep/wake |
| `REG_SMPLRT_DIV` | `0x19` | Sample-rate divider |
| `REG_CONFIG` | `0x1A` | Digital low-pass filter (DLPF) |
| `REG_GYRO_CONFIG` | `0x1B` | Gyroscope full-scale range |
| `REG_ACCEL_CONFIG` | `0x1C` | Accelerometer full-scale range |
| `REG_ACCEL_XOUT_H` | `0x3B` | First of 6 accel output bytes |
| `REG_TEMP_OUT_H` | `0x41` | First of 2 temperature output bytes |
| `REG_GYRO_XOUT_H` | `0x43` | First of 6 gyro output bytes |
| `REG_WHO_AM_I` | `0x75` | Device identity (reads back `0x68`) |

I²C address, sample-rate divider, DLPF setting, and both full-scale ranges are all supplied by the caller through `mpu6050_config_t` at init time — nothing is hard-coded, so the same driver works for either strapping of the `AD0` pin.

### 2.1 Address convention

> ⚠️ **IMPORTANT:** Confirm which address your `AD0` pin strapping selects before wiring a second sensor on the same bus.

```text
AD0 = LOW   -> address 0x68   (MPU6050_ADDRESS_LOW,  the default)
AD0 = HIGH  -> address 0x69   (MPU6050_ADDRESS_HIGH)
```

### 2.2 Full-scale range → sensitivity

```text
Accelerometer (AFS_SEL):            Gyroscope (FS_SEL):
    +/-2g   -> 16384 LSB/g              +/-250 dps  -> 131.0 LSB/(deg/s)
    +/-4g   -> 8192  LSB/g              +/-500 dps  -> 65.5  LSB/(deg/s)
    +/-8g   -> 4096  LSB/g              +/-1000 dps -> 32.8  LSB/(deg/s)
    +/-16g  -> 2048  LSB/g              +/-2000 dps -> 16.4  LSB/(deg/s)
```

The driver applies the correct sensitivity automatically based on whichever range is currently configured — callers always receive already-converted `float` values, never raw counts.

---

## 📦 3. Data Types

### 3.1 `mpu6050_config_t`

```c
typedef struct
{
    const char *i2c_path;

    uint8_t address;

    mpu6050_accel_range_t accel_range;
    mpu6050_gyro_range_t gyro_range;

    uint8_t sample_rate_divider;
    uint8_t dlpf_config;

} mpu6050_config_t;
```

Passed once to `mpu6050_init()`. `address` must be exactly `0x68` or `0x69`; `dlpf_config` must be `0`–`6`. Build one from scratch, or start from `mpu6050_config_default()` and override only what you need.

### 3.2 `mpu6050_accel_t` / `mpu6050_gyro_t`

```c
typedef struct { float x; float y; float z; } mpu6050_accel_t;  /* units: g       */
typedef struct { float x; float y; float z; } mpu6050_gyro_t;   /* units: deg/s   */
```

### 3.3 `mpu6050_data_t`

```c
typedef struct
{
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    float temperature;    /* Celsius */

} mpu6050_data_t;
```

Populated by `mpu6050_read_all()` from a single 14-byte burst read — accel, temperature, and gyro are guaranteed time-coherent with each other.

### 3.4 `mpu6050_accel_range_t` / `mpu6050_gyro_range_t`

```c
typedef enum
{
    MPU6050_ACCEL_RANGE_2G = 0,
    MPU6050_ACCEL_RANGE_4G,
    MPU6050_ACCEL_RANGE_8G,
    MPU6050_ACCEL_RANGE_16G
} mpu6050_accel_range_t;

typedef enum
{
    MPU6050_GYRO_RANGE_250DPS = 0,
    MPU6050_GYRO_RANGE_500DPS,
    MPU6050_GYRO_RANGE_1000DPS,
    MPU6050_GYRO_RANGE_2000DPS
} mpu6050_gyro_range_t;
```

### 3.5 `mpu6050_t` — driver handle

```c
typedef struct
{
    int fd;
    uint8_t address;

    mpu6050_accel_range_t accel_range;
    mpu6050_gyro_range_t gyro_range;

    uint8_t sample_rate_divider;
    uint8_t dlpf_config;

    int initialized;
    pthread_mutex_t lock;

} mpu6050_t;
```

Treat as an **opaque handle** — do not modify its members directly after initialization. Unlike a heap-allocated mutex, `lock` is embedded by value, so there's no separate mutex-free step on teardown.

### 3.6 Constants

| Constant | Value | Meaning |
|---|:---:|---|
| `MPU6050_DEFAULT_I2C_PATH` | `"/dev/i2c1"` | Default I²C bus node |
| `MPU6050_DEFAULT_ADDRESS` | `0x68` | Default 7-bit address |
| `MPU6050_ADDRESS_LOW` | `0x68` | `AD0` strapped low |
| `MPU6050_ADDRESS_HIGH` | `0x69` | `AD0` strapped high |

---

## 🛠️ 4. API Reference

### 4.1 `mpu6050_config_default`

```c
int mpu6050_config_default(mpu6050_config_t *config);
```

Fills `*config` with baseline defaults: `/dev/i2c1`, `0x68`, ±2g, ±250 dps, sample-rate divider `7`, DLPF `0`.

| | |
|---|---|
| **Parameters** | `config` — output pointer, must not be `NULL` |
| **Returns** | `EOK`, or `EINVAL` |

### 4.2 `mpu6050_set_address`

```c
int mpu6050_set_address(mpu6050_config_t *config, uint8_t address);
```

Overrides the address field of a **not-yet-initialized** config struct (e.g. to select `0x69` for a second sensor). Does not touch a live handle.

| | |
|---|---|
| **Parameters** | `config` — must not be `NULL`. `address` — must be `0x68` or `0x69`. |
| **Returns** | `EOK`, or `EINVAL` |

### 4.3 `mpu6050_init` — primary bring-up entry point

```c
int mpu6050_init(mpu6050_t *dev, const mpu6050_config_t *config);
```

Opens the bus, wakes the sensor, applies sample-rate/DLPF/range configuration, and verifies `WHO_AM_I`.

| | |
|---|---|
| **Parameters** | `dev` — handle to populate. `config` — validated configuration; must not be `NULL`. |
| **Returns** | `EOK`, or `EINVAL` / `errno` from `open()` / `ENODEV` (identity mismatch) / I²C error |
| **Preconditions** | None — `dev` is zeroed and rebuilt from scratch |
| **Guarantees** | On any failure from device-open onward, the fd is closed and the mutex destroyed — no cleanup call needed after a failed init |
| **Concurrency** | Must not be called concurrently with itself, `mpu6050_deinit()`, or any other `mpu6050_*` call on the same handle |

### 4.4 `mpu6050_init_default`

```c
int mpu6050_init_default(mpu6050_t *dev);
```

Convenience wrapper: builds a config via `mpu6050_config_default()` and calls `mpu6050_init()`.

| | |
|---|---|
| **Returns** | Same as §4.3, plus `EINVAL` if `dev` is `NULL` |

### 4.5 `mpu6050_check_device`

```c
int mpu6050_check_device(mpu6050_t *dev);
```

Reads `WHO_AM_I` and confirms it equals `0x68`. Called automatically inside `mpu6050_init()`; exposed publicly to re-verify the link at any time.

| | |
|---|---|
| **Returns** | `EOK`, or `EINVAL` / `ENODEV` (`fd < 0` or identity mismatch) / I²C error |
| **Concurrency** | ⚠️ Does **not** take the driver's lock and does **not** check `initialized` — only checks `fd >= 0`. Calling it yourself concurrently with other operations is not fully serialized against them. |

### 4.6 `mpu6050_read_accel` / `mpu6050_read_gyro` / `mpu6050_read_temperature`

```c
int mpu6050_read_accel(mpu6050_t *dev, mpu6050_accel_t *accel);
int mpu6050_read_gyro(mpu6050_t *dev, mpu6050_gyro_t *gyro);
int mpu6050_read_temperature(mpu6050_t *dev, float *temperature);
```

Single-quantity reads. Each performs its own I²C burst read and converts to physical units using the currently configured range.

| | |
|---|---|
| **Returns** | `EOK`, or `EINVAL` / `ENODEV` (not initialized) / I²C error |

### 4.7 `mpu6050_read_all` — primary read entry point

```c
int mpu6050_read_all(mpu6050_t *dev, mpu6050_data_t *data);
```

**Recommended over the individual reads whenever more than one quantity is needed.** One 14-byte I²C burst covers accel + temperature + gyro in a single transaction, guaranteeing all three come from the same instant — important for anything sensitive to cross-channel timing skew.

| | |
|---|---|
| **Parameters** | `data` — output pointer, must not be `NULL` |
| **Returns** | `EOK`, or `EINVAL` / `ENODEV` / I²C error |

### 4.8 `mpu6050_set_accel_range` / `mpu6050_set_gyro_range`

```c
int mpu6050_set_accel_range(mpu6050_t *dev, mpu6050_accel_range_t range);
int mpu6050_set_gyro_range(mpu6050_t *dev, mpu6050_gyro_range_t range);
```

Reconfigures the full-scale range at runtime and updates the handle's internal sensitivity so subsequent reads convert correctly automatically.

| | |
|---|---|
| **Returns** | `EOK`, or `EINVAL` / `ENODEV` / I²C error |

### 4.9 `mpu6050_set_sample_rate_divider` / `mpu6050_set_dlpf`

```c
int mpu6050_set_sample_rate_divider(mpu6050_t *dev, uint8_t divider);
int mpu6050_set_dlpf(mpu6050_t *dev, uint8_t config);
```

`Sample Rate = Gyro Output Rate / (1 + divider)`, where the gyro output rate is 1 kHz with the DLPF enabled (`dlpf_config` 1–6) or 8 kHz with it disabled (`dlpf_config = 0`).

| | |
|---|---|
| **Parameters** | `config` (DLPF) must be `0`–`6` |
| **Returns** | `EOK`, or `EINVAL` / `ENODEV` / I²C error |

### 4.10 `mpu6050_sleep` / `mpu6050_wakeup`

```c
int mpu6050_sleep(mpu6050_t *dev);
int mpu6050_wakeup(mpu6050_t *dev);
```

Power management — sets/clears the `SLEEP` bit in `REG_PWR_MGMT_1`.

| | |
|---|---|
| **Returns** | `EOK`, or `EINVAL` / `ENODEV` / I²C error |

### 4.11 `mpu6050_deinit`

```c
int mpu6050_deinit(mpu6050_t *dev);
```

Closes the I²C fd and destroys the embedded mutex. Safe to call even after a partially-failed init — it no-ops cleanly if there's nothing to release.

| | |
|---|---|
| **Returns** | `EOK` (including the no-op case), or `EINVAL` |

---

## 🔒 5. Thread Safety Summary

| Function | Blocks on mutex? | Notes |
|---|:---:|---|
| `mpu6050_init` / `mpu6050_init_default` / `mpu6050_deinit` | Yes | Must not overlap any other `mpu6050_*` call on the same handle |
| `mpu6050_read_accel` / `read_gyro` / `read_temperature` / `read_all` | Yes | Safe to call concurrently with other data/config calls (will simply wait its turn) |
| `mpu6050_set_accel_range` / `set_gyro_range` / `set_sample_rate_divider` / `set_dlpf` | Yes | Same as above |
| `mpu6050_sleep` / `mpu6050_wakeup` | Yes | Same as above |
| `mpu6050_get_status`-equivalent (n/a — no status snapshot in this driver) | — | This driver has no cached-status accessor; every read talks to hardware directly |
| **`mpu6050_check_device`** | **No** | Bypasses the lock and `initialized` check — see §4.5 |

---

## 💻 6. Example Code

### 6.1 Basic default init and single combined read

```c
#include <stdio.h>
#include <stdlib.h>
#include "mpu6050.h"

int main(void)
{
    mpu6050_t imu;

    int rc = mpu6050_init_default(&imu);
    if (rc != EOK)
    {
        fprintf(stderr, "mpu6050_init_default failed: %d\n", rc);
        return 1;
    }

    mpu6050_data_t data;
    rc = mpu6050_read_all(&imu, &data);
    if (rc == EOK)
    {
        printf("ACC X=%+.2f Y=%+.2f Z=%+.2f | TEMP=%.2f C | GYRO X=%+.2f Y=%+.2f Z=%+.2f dps\n",
               data.accel.x, data.accel.y, data.accel.z,
               data.temperature,
               data.gyro.x, data.gyro.y, data.gyro.z);
    }

    mpu6050_deinit(&imu);
    return 0;
}
```

### 6.2 Custom configuration (wider range, faster sampling)

```c
mpu6050_config_t config;
mpu6050_config_default(&config);

config.accel_range        = MPU6050_ACCEL_RANGE_8G;
config.gyro_range          = MPU6050_GYRO_RANGE_500DPS;
config.sample_rate_divider = 0;
config.dlpf_config         = 3;

mpu6050_t imu;
int rc = mpu6050_init(&imu, &config);
if (rc != EOK)
{
    /* handle error */
}
```

### 6.3 Second sensor on the alternate address

```c
mpu6050_config_t config;
mpu6050_config_default(&config);
mpu6050_set_address(&config, MPU6050_ADDRESS_HIGH);   /* 0x69 */

mpu6050_t imu2;
mpu6050_init(&imu2, &config);
```

### 6.4 Continuous sampling loop from a sensing task

```c
/* Called periodically by a sensing/estimation task, e.g. every 10 ms. */
void imu_task_tick(mpu6050_t *imu)
{
    mpu6050_data_t data;

    if (mpu6050_read_all(imu, &data) != EOK)
    {
        /* log/escalate — hardware read failure */
        return;
    }

    float accel_mag = sqrtf(data.accel.x * data.accel.x +
                             data.accel.y * data.accel.y +
                             data.accel.z * data.accel.z);

    /* Hand accel_mag / data.gyro.* off to your fusion or event
     * detection layer — that logic does not belong in the driver. */
}
```

### 6.5 Low-power idle, then resume

```c
mpu6050_sleep(&imu);      /* put sensor to sleep when not sampling */
/* ... */
mpu6050_wakeup(&imu);     /* resume before the next read */
```

---

## ⚠️ 7. Error Handling Reference

| Code | When it occurs | Recommended caller response |
|---|---|---|
| `EINVAL` | `NULL` pointer, out-of-range value, bad address/DLPF value | Programming error — fix the caller, don't retry |
| `ENODEV` | A data/config call is made before init succeeds, or `WHO_AM_I` doesn't match `0x68` | Check wiring/address; call `mpu6050_init()` (again) before retrying |
| *(errno from `open()`)* | I²C device node failed to open | Check the configured `i2c_path` and permissions |
| *(errno/status from `devctl()`)* | I²C transaction failure (e.g. `ETIMEDOUT`, `EIO`) | Sensor not ACKing — check wiring, address, and bus health; retry as appropriate for your application |

---

## 📝 8. Design Notes / Out of Scope

- **No feedback beyond raw sensor data.** This driver reads and scales accel/gyro/temperature only — there is no sensor fusion, complementary/Kalman filtering, orientation/quaternion estimation, or calibration/bias-correction logic. Those belong in a task built on top of this driver.
- **No gesture or event detection.** Shake detection, tap detection, free-fall detection, etc. are application-level logic — not part of this driver.
- **No runtime address change on a live handle.** `mpu6050_set_address()` only edits a `mpu6050_config_t` before `mpu6050_init()`. To talk to a different address, configure it before init or run a second, separate `mpu6050_t` handle.
- **`mpu6050_check_device()` bypasses the normal lock/initialized-check convention** because it must also work *during* `mpu6050_init()`, before the handle is marked initialized — see §4.5.
- **No automatic retry/backoff** on I²C transaction failure — callers should handle `EIO`/`ETIMEDOUT` and retry as appropriate.

