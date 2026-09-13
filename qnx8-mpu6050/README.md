<div align="center">

# 🧭 MPU6050 IMU Driver Library
### Software Datasheet & API Reference

[![Platform](https://img.shields.io/badge/Platform-QNX_Neutrino_RTOS-blue?style=for-the-badge&logo=qnx)](#)
[![Protocol](https://img.shields.io/badge/Interface-I%C2%B2C_(devctl)-8A2BE2?style=for-the-badge)](#)
[![Language](https://img.shields.io/badge/Language-C_(C11)-00599C?style=for-the-badge&logo=c)](#)
[![Maintained by](https://img.shields.io/badge/Maintained_by-Madeti_Pavan_Kumar-success?style=for-the-badge)](https://github.com/Pavan-1522)

*A lightweight, thread-safe C driver for the InvenSense MPU6050 6-axis I²C motion sensor, designed for the QNX Neutrino RTOS.*

</div>

---

**Scope:** Built for students and hobbyists learning QNX driver development and I²C sensor programming.  
**Files:** `mpu6050.h`, `mpu6050.c`, `test.c`  
**Dependencies:** QNX Neutrino RTOS (`<devctl.h>`, `<hw/i2c.h>`), `pthread`, `-lm` (math library for magnitude calculation)

---

## 🎯 1. About the MPU6050

The **MPU6050** is a low-cost 6-axis Motion Tracking device combining a 3-axis MEMS gyroscope and a 3-axis MEMS accelerometer on a single I²C-addressable chip, plus an on-die temperature sensor. It's one of the most common IMUs in hobbyist robotics, drones, and balance-bot projects.

| Property | Value |
|---|---|
| **Interface** | I²C (up to 400 kHz Fast Mode) |
| **I²C address** | `0x68` (AD0 pin low) or `0x69` (AD0 pin high) |
| **Supply voltage** | 2.375 V – 3.46 V |
| **Accelerometer ranges** | ±2g / ±4g / ±8g / ±16g (selectable) |
| **Gyroscope ranges** | ±250 / ±500 / ±1000 / ±2000 °/s (selectable) |
| **Temperature sensor** | On-die, °C output |
| **WHO_AM_I identity** | `0x68` |

---

## ✨ 2. Library Features

*   **Clean API:** Opaque handle-based API (`mpu6050_t`) — no global state.
*   **Thread Safety:** Internal mutex-protected register access (safe for multi-threaded QNX applications).
*   **Quick Start:** Config-struct-based initialization, with a one-call sane-defaults path (`mpu6050_init_default()`).
*   **Data Handling:** All scaling math (raw LSB → physical units) is handled internally.
*   **Burst Reading:** Separate read functions, plus a single-transaction combined read (`mpu6050_read_all()`) that pulls all 14 bytes in one I²C burst for time-consistent samples.
*   **Configuration:** Runtime range reconfiguration, sample-rate divider, and digital low-pass filter (DLPF).
*   **Diagnostics:** Automatic device-identity check (`WHO_AM_I`) at startup.
*   **Power:** Sleep/wake power management.

---

## 🛠️ 3. Requirements & Building

### Requirements
*   **QNX Neutrino RTOS:** Uses `<devctl.h>` and `<hw/i2c.h>` (QNX-specific).
*   **I²C Resource Manager:** Configured instance on target (e.g. `/dev/i2c1`).
*   **pthread Support:** For the internal embedded mutex.
*   **Math Library:** `-lm` if replicating the `sqrtf()` magnitude calculation.

### Building
**Example manual compile:**
```bash
qcc -Vgcc_ntoaarch64le -o test test.c mpu6050.c -lpthread -lm
```

**Or with plain `gcc`-style QNX toolchain:**
```bash
qcc -o test test.c mpu6050.c -lpthread -lm
```

**Run on target:**
```bash
./test
```

---

## ⚙️ 4. Register Map & Hardware Model

### Essential Registers
| Register | Address | Purpose |
|---|:---:|---|
| `REG_SMPLRT_DIV` | `0x19` | Sample rate divider |
| `REG_CONFIG` | `0x1A` | Digital low-pass filter (DLPF) configuration |
| `REG_GYRO_CONFIG` | `0x1B` | Gyroscope full-scale range (`FS_SEL`, bits 4:3) |
| `REG_ACCEL_CONFIG` | `0x1C` | Accelerometer full-scale range (`AFS_SEL`, bits 4:3) |
| `REG_ACCEL_XOUT_H` | `0x3B` | First of 6 contiguous accel output bytes (X/Y/Z) |
| `REG_TEMP_OUT_H` | `0x41` | First of 2 temperature output bytes |
| `REG_GYRO_XOUT_H` | `0x43` | First of 6 contiguous gyro output bytes (X/Y/Z) |
| `REG_PWR_MGMT_1` | `0x6B` | Power management |
| `REG_WHO_AM_I` | `0x75` | Device identity register (`0x68`) |

> Registers `0x3B`–`0x48` are contiguous (accel → temp → gyro), allowing `mpu6050_read_all()` to pull 14 bytes in a single burst.

### Accelerometer & Gyroscope Config Ranges
*   **Accel:** ±2g, ±4g, ±8g, ±16g (Sensitivities handled internally)
*   **Gyro:** ±250, ±500, ±1000, ±2000 °/s (Sensitivities handled internally)
*   **Temp:** `temperature_C = (raw_int16 / 340.0) + 36.53`

---

## 📦 5. Data Types & Structures

### 5.1 Main Data Structures

```c
typedef struct { float x; float y; float z; } mpu6050_accel_t; // units: g
typedef struct { float x; float y; float z; } mpu6050_gyro_t;  // units: degrees/sec

typedef struct
{
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    float temperature;   // Celsius
} mpu6050_data_t;
```

### 5.2 Configuration Struct

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

### 5.3 Driver Handle

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
> **Note:** Treat this as an opaque handle. Do not directly modify members after initialization.

---

## 🛠️ 6. API Reference

All functions return an `int` status code. `EOK` (`0`) means success. 

### 6.1 Initialization & Configuration
*   `int mpu6050_config_default(mpu6050_config_t *config);`
    Fills `*config` with library defaults (e.g. `"/dev/i2c1"`, `0x68`, `±2g`, `±250DPS`).
*   `int mpu6050_set_address(mpu6050_config_t *config, uint8_t address);`
*   `int mpu6050_init(mpu6050_t *dev, const mpu6050_config_t *config);`
*   `int mpu6050_init_default(mpu6050_t *dev);`
    Convenience wrapper for common defaults.
*   `int mpu6050_deinit(mpu6050_t *dev);`

### 6.2 Reading Data
*   `int mpu6050_read_all(mpu6050_t *dev, mpu6050_data_t *data);` **(Recommended)**
    Single 14-byte burst read ensuring time-coherent data.
*   `int mpu6050_read_accel(mpu6050_t *dev, mpu6050_accel_t *accel);`
*   `int mpu6050_read_gyro(mpu6050_t *dev, mpu6050_gyro_t *gyro);`
*   `int mpu6050_read_temperature(mpu6050_t *dev, float *temperature);`

### 6.3 Settings & Power Management
*   `int mpu6050_set_accel_range(mpu6050_t *dev, mpu6050_accel_range_t range);`
*   `int mpu6050_set_gyro_range(mpu6050_t *dev, mpu6050_gyro_range_t range);`
*   `int mpu6050_set_sample_rate_divider(mpu6050_t *dev, uint8_t divider);`
*   `int mpu6050_set_dlpf(mpu6050_t *dev, uint8_t config);`
*   `int mpu6050_sleep(mpu6050_t *dev);`
*   `int mpu6050_wakeup(mpu6050_t *dev);`
*   `int mpu6050_check_device(mpu6050_t *dev);` (Checks `WHO_AM_I` registry)

---

## ⚠️ 7. Error Codes

| Code | Meaning in this driver |
|---|---|
| `EOK` | Success |
| `EINVAL` | Invalid argument — `NULL` pointer, out-of-range field, or bad address/range |
| `ENODEV` | Device not initialized, or `WHO_AM_I` didn't match `0x68` |
| `(errno from open())` | Returned verbatim if the I²C device node fails to open (e.g. `ENOENT`) |
| `(errno from devctl())` | Returned verbatim on I²C bus/transaction failure (e.g. `ETIMEDOUT`, `EIO`) |
| `(errno from pthread)` | Propagated directly if mutex operations fail |

---

## 💻 8. Example Programs

### Minimal default init + single combined read
```c
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpu6050.h"

int main(void)
{
    mpu6050_t imu;

    int status = mpu6050_init_default(&imu);
    if (status != EOK)
    {
        printf("MPU6050 initialization failed: %d\n", status);
        return EXIT_FAILURE;
    }

    mpu6050_data_t data;
    status = mpu6050_read_all(&imu, &data);
    if (status == EOK)
    {
        printf("ACC X=%+.2f Y=%+.2f Z=%+.2f | TEMP=%.2f C | GYRO X=%+.2f Y=%+.2f Z=%+.2f dps\n",
               data.accel.x, data.accel.y, data.accel.z,
               data.temperature,
               data.gyro.x, data.gyro.y, data.gyro.z);
    }

    mpu6050_deinit(&imu);
    return EXIT_SUCCESS;
}
```

### Continuous read loop with simple shake detection
```c
int shake_detected = 0;

while (1)
{
    mpu6050_data_t data;

    if (mpu6050_read_all(&imu, &data) != EOK)
        break;

    float magnitude = sqrtf(data.accel.x * data.accel.x +
                             data.accel.y * data.accel.y +
                             data.accel.z * data.accel.z);

    printf("ACC X=%+.2f Y=%+.2f Z=%+.2f | MAG=%.2f g | TEMP=%.2f C | GYRO X=%+.2f Y=%+.2f Z=%+.2f dps\n",
           data.accel.x, data.accel.y, data.accel.z,
           magnitude, data.temperature,
           data.gyro.x, data.gyro.y, data.gyro.z);

    /* Application-level logic — not part of the driver */
    if (!shake_detected && magnitude > 2.0f)
    {
        printf("SHAKE DETECTED!\n");
        shake_detected = 1;
    }
    if (shake_detected && magnitude < 1.3f)
        shake_detected = 0;

    usleep(100000);
}
```

---

## 📝 9. Design Notes & Known Limitations

*   **QNX-only:** Built directly on QNX's `devctl` interface.
*   **7-bit addressing only:** `I2C_ADDRFMT_7BIT`.
*   **No runtime address change:** `mpu6050_set_address()` only edits a `mpu6050_config_t`, not a live handle. 
*   **`mpu6050_check_device()` bypasses driver locks:** It must work *during* `mpu6050_init()`.
*   **Prefer `mpu6050_read_all()`** for faster, time-coherent data reading.
*   **Shake detection is application logic:** The library only gives you calibrated physical units.

---

## 👨‍💻 10. Credits

<div align="center">
  <p>Designed and developed by <b>Madeti Pavan Kumar</b>.</p>
  
  <a href="https://github.com/Pavan-1522"><img src="https://img.shields.io/badge/GitHub-100000?style=for-the-badge&logo=github&logoColor=white" alt="GitHub"></a>
  <a href="https://www.linkedin.com/in/pavankumarmadeti/"><img src="https://img.shields.io/badge/LinkedIn-0077B5?style=for-the-badge&logo=linkedin&logoColor=white" alt="LinkedIn"></a>
  <a href="https://pavankumarmadeti.elegets.in/"><img src="https://img.shields.io/badge/Website-4285F4?style=for-the-badge&logo=google-chrome&logoColor=white" alt="Website"></a>
</div>

