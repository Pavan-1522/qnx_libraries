<div align="center">

# 🕰️ DS3231 RTC Driver Library
### Software Datasheet & API Reference

[![Platform](https://img.shields.io/badge/Platform-QNX_Neutrino_RTOS-blue?style=for-the-badge&logo=qnx)](#)
[![Protocol](https://img.shields.io/badge/Interface-I%C2%B2C_(devctl)-8A2BE2?style=for-the-badge)](#)
[![Language](https://img.shields.io/badge/Language-C_(C11)-00599C?style=for-the-badge&logo=c)](#)
[![Maintained by](https://img.shields.io/badge/Maintained_by-Madeti_Pavan_Kumar-success?style=for-the-badge)](https://github.com/Pavan-1522)

*A lightweight, thread-safe C driver for the Maxim/Analog Devices DS3231 I²C Real-Time Clock, designed for the QNX Neutrino RTOS I²C resource manager framework.*

</div>

---

**Scope:** Built for students and hobbyists learning QNX driver development and I²C peripheral programming.  
**Files:** `ds3231.h`, `ds3231.c`, `test.c`  
**Dependencies:** QNX Neutrino RTOS (`<devctl.h>`, `<hw/i2c.h>`), `pthread`

---

## 🎯 1. About the DS3231

The DS3231 is a low-cost, extremely accurate I²C real-time clock (RTC) with an integrated temperature-compensated crystal oscillator (TCXO) and crystal. It provides:

*   **Timekeeping:** Seconds, minutes, hours, day, date, month, year (with automatic leap-year compensation through 2099)
*   **Alarms:** Two programmable time-of-day alarms
*   **Interrupts:** A programmable square-wave/interrupt output pin (SQW/INT)
*   **Sensors:** A digital temperature sensor (±3°C accuracy, 0.25°C resolution)
*   **Reliability:** Battery-backed operation for continued timekeeping without main power
*   **Outputs:** A 32.768 kHz output pin

| Property | Value |
|---|---|
| **Interface** | I²C (up to 400 kHz Fast Mode) |
| **Default 7-bit I²C address** | `0x68` |
| **Supply voltage** | 2.3 V – 5.5 V |
| **Timekeeping range** | 2000 – 2099 (driver-enforced) |
| **Temperature resolution** | 0.25 °C |
| **Register map size** | `0x00` – `0x12` (19 registers) |

---

## ✨ 2. Library Features

*   **Clean API:** Opaque handle-based API (`ds3231_t`) — no global state.
*   **Thread Safety:** Internal mutex-protected register access (safe for multi-threaded QNX applications).
*   **Data Handling:** Automatic BCD ↔ decimal conversion (the API always speaks decimal).
*   **Time Normalization:** 12-hour/PM-bit-aware read path that normalizes the hour value into 24-hour form for the caller.
*   **Alarms:** Alarm 1 (seconds resolution) and Alarm 2 (minute resolution) configuration, enable, flag-read, and flag-clear.
*   **Wave Control:** Square-wave output frequency selection (1 Hz / 1.024 kHz / 4.096 kHz / 8.192 kHz) and SQW/INT pin mode switch, plus 32.768 kHz auxiliary output control.
*   **Diagnostics:** Oscillator-Stop-Flag (OSF) detection — tells you if the RTC lost power and its time is no longer trustworthy. Aging offset get/set for fine crystal calibration.
*   **Raw Access:** Raw single-register read/write escape hatch for advanced use.

---

## 🛠️ 3. Requirements & Building

### Requirements
*   **QNX Neutrino RTOS:** Uses `<devctl.h>` and `<hw/i2c.h>` (QNX-specific; will not compile as-is on Linux/Windows).
*   **I²C Resource Manager:** A configured instance on the target (e.g., `/dev/i2c1`), started by your board's `i2c-*` driver.
*   **pthread Support:** For the internal mutex.
*   **Hardware:** The DS3231 module wired to the corresponding I²C bus with pull-ups on SDA/SCL.

### Building
**Example manual compile** (adjust the I²C bus path for your board's BSP):
```bash
qcc -Vgcc_ntoaarch64le -o test test.c ds3231.c -lpthread
```

**Or with a plain gcc-style QNX toolchain invocation:**
```bash
qcc -o test test.c ds3231.c -lpthread
```

**Run on target:**
```bash
./test
```

---

## ⚙️ 4. DS3231 Register Map

The library talks to the following on-chip registers:

| Register | Address | Purpose |
|---|:---:|---|
| `DS3231_REG_SECONDS` | `0x00` | Seconds (BCD) |
| `DS3231_REG_MINUTES` | `0x01` | Minutes (BCD) |
| `DS3231_REG_HOURS` | `0x02` | Hours (BCD, 12/24-hr format bit) |
| `DS3231_REG_DAY` | `0x03` | Day of week (1–7) |
| `DS3231_REG_DATE` | `0x04` | Day of month (1–31, BCD) |
| `DS3231_REG_MONTH` | `0x05` | Month (1–12, BCD) + century bit |
| `DS3231_REG_YEAR` | `0x06` | Year, 2 digits (BCD, 00–99) |
| `DS3231_REG_ALARM1_SEC` | `0x07` | Alarm 1 seconds |
| `DS3231_REG_ALARM1_MIN` | `0x08` | Alarm 1 minutes |
| `DS3231_REG_ALARM1_HOUR` | `0x09` | Alarm 1 hours |
| `DS3231_REG_ALARM1_DATE` | `0x0A` | Alarm 1 date/day |
| `DS3231_REG_ALARM2_MIN` | `0x0B` | Alarm 2 minutes |
| `DS3231_REG_ALARM2_HOUR` | `0x0C` | Alarm 2 hours |
| `DS3231_REG_ALARM2_DATE` | `0x0D` | Alarm 2 date/day |
| `DS3231_REG_CONTROL` | `0x0E` | Control register (SQW, alarm interrupt enables) |
| `DS3231_REG_STATUS` | `0x0F` | Status register (OSF, alarm flags, 32kHz enable, BSY) |
| `DS3231_REG_AGING` | `0x10` | Aging offset (crystal trim, signed 8-bit) |
| `DS3231_REG_TEMP_MSB` | `0x11` | Temperature, integer part |
| `DS3231_REG_TEMP_LSB` | `0x12` | Temperature, fractional part (top 2 bits = 0.25°C steps) |

---

## 📦 5. Data Types & Structures

### 5.1 `ds3231_config_t` — initialization configuration

```c
typedef struct
{
    const char *device;   // I2C device path, e.g. "/dev/i2c1"
    uint16_t address;     // 7-bit I2C slave address, e.g. 0x68
} ds3231_config_t;
```

*   `device`: Path to the QNX I²C bus node. If `NULL`, defaults to `DS3231_DEFAULT_DEVICE` (`"/dev/i2c1"`).
*   `address`: 7-bit I²C address. If `0`, defaults to `DS3231_DEFAULT_ADDRESS` (`0x68`). Must be between `0x03` and `0x77` or initialization fails with `EINVAL`.

### 5.2 `ds3231_t` — driver handle

```c
typedef struct
{
    int fd;              // Open file descriptor for the I2C bus node
    uint16_t address;    // Active 7-bit I2C slave address
    bool initialized;    // true once ds3231_init() succeeds
    void *mutex;         // Internal pthread_mutex_t*, heap-allocated
} ds3231_t;
```
> **Note:** Treat this as an opaque handle. Do not read or write its fields directly.

### 5.3 `ds3231_datetime_t` — full date/time

```c
typedef struct
{
    uint8_t seconds;    // 0–59
    uint8_t minutes;    // 0–59
    uint8_t hours;      // 0–23 (always 24-hour form to/from the caller)

    uint8_t day;        // 1–7, see ds3231_weekday_t
    uint8_t date;       // 1–31 (day of month)
    uint8_t month;      // 1–12

    uint16_t year;      // 2000–2199 for validation; hardware supports 2000–2099
} ds3231_datetime_t;
```

### 5.4 Support Enums

*   **`ds3231_weekday_t`**: `DS3231_SUNDAY = 1` through `DS3231_SATURDAY = 7`.
*   **`ds3231_sqw_frequency_t`**: Options for 1Hz, 1.024kHz, 4.096kHz, and 8.192kHz square-wave outputs.
*   **`ds3231_alarm1_mode_t`**: Match modes for Alarm 1 (seconds resolution).
*   **`ds3231_alarm2_mode_t`**: Match modes for Alarm 2 (minute resolution).

---

## 🛠️ 6. API Reference

All functions return an `int` status code. `EOK` (`0`) means success; any other value is an errno-style error code. 

### 6.1 Initialization

*   `int ds3231_init(ds3231_t *dev, const ds3231_config_t *config);`
    Opens the I²C device node and validates the slave address. Must be called first.
*   `int ds3231_deinit(ds3231_t *dev);`
    Closes the I²C file descriptor and frees the mutex.

### 6.2 Device Configuration
*   `int ds3231_set_address(ds3231_t *dev, uint16_t address);`
*   `uint16_t ds3231_get_address(ds3231_t *dev);`

### 6.3 Date / Time
*   `int ds3231_set_datetime(ds3231_t *dev, const ds3231_datetime_t *datetime);`
    Writes seconds through year in a single I²C burst write. Time is always written in 24-hour mode.
*   `int ds3231_get_datetime(ds3231_t *dev, ds3231_datetime_t *datetime);`
*   `int ds3231_set_time(ds3231_t *dev, uint8_t hours, uint8_t minutes, uint8_t seconds);`
*   `int ds3231_get_time(ds3231_t *dev, uint8_t *hours, uint8_t *minutes, uint8_t *seconds);`
*   `int ds3231_set_date(ds3231_t *dev, uint16_t year, uint8_t month, uint8_t date, uint8_t day);`
*   `int ds3231_get_date(ds3231_t *dev, uint16_t *year, uint8_t *month, uint8_t *date, uint8_t *day);`

### 6.4 Status & Features
*   `int ds3231_get_temperature(ds3231_t *dev, float *temperature);`
*   `int ds3231_is_oscillator_stopped(ds3231_t *dev, bool *stopped);`
*   `int ds3231_clear_oscillator_stop_flag(ds3231_t *dev);`
*   `int ds3231_enable_32khz(ds3231_t *dev, bool enable);`
*   `int ds3231_set_sqw_frequency(ds3231_t *dev, ds3231_sqw_frequency_t frequency);`
*   `int ds3231_enable_sqw(ds3231_t *dev, bool enable);`

### 6.5 Alarms
*   `int ds3231_set_alarm1(ds3231_t *dev, uint8_t seconds, uint8_t minutes, uint8_t hours, uint8_t date, ds3231_alarm1_mode_t mode);`
*   `int ds3231_enable_alarm1(ds3231_t *dev, bool enable);`
*   `int ds3231_get_alarm1_flag(ds3231_t *dev, bool *triggered);`
*   `int ds3231_clear_alarm1_flag(ds3231_t *dev);`
*   *(Equivalent functions exist for Alarm 2)*

### 6.6 Raw Register & Tuning
*   `int ds3231_set_aging_offset(ds3231_t *dev, int8_t offset);`
*   `int ds3231_get_aging_offset(ds3231_t *dev, int8_t *offset);`
*   `int ds3231_read_register(ds3231_t *dev, uint8_t reg, uint8_t *value);`
*   `int ds3231_write_register(ds3231_t *dev, uint8_t reg, uint8_t value);`

---

## ⚠️ 7. Error Codes

| Code | Meaning in this driver |
|---|---|
| `EOK` | Success |
| `EINVAL` | Invalid argument — `NULL` pointer, out-of-range field, uninitialized handle, or bad address/mode |
| `ENOMEM` | Mutex allocation failed during `ds3231_init()` |
| `(errno from open())` | Returned verbatim if the I²C device node fails to open (e.g. `ENOENT`, `EACCES`) |
| `(errno from devctl())` | Returned verbatim on I²C bus/transaction failure (e.g. `ETIMEDOUT`, `EIO`) |

---

## 💻 8. Example Programs

### Minimal init + read (from `test.c`)
```c
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

    int status = ds3231_init(&rtc, &config);
    if (status != EOK)
    {
        printf("DS3231 initialization failed: %d\n", status);
        return EXIT_FAILURE;
    }

    ds3231_datetime_t datetime;
    status = ds3231_get_datetime(&rtc, &datetime);
    if (status == EOK)
    {
        printf("%04u-%02u-%02u %02u:%02u:%02u Day=%u\n",
               datetime.year, datetime.month, datetime.date,
               datetime.hours, datetime.minutes, datetime.seconds,
               datetime.day);
    }

    ds3231_deinit(&rtc);
    return EXIT_SUCCESS;
}
```

### Checking for power loss
```c
bool stopped = false;

if (ds3231_is_oscillator_stopped(&rtc, &stopped) == EOK && stopped)
{
    printf("RTC lost power — time is stale, re-syncing needed\n");

    ds3231_datetime_t known_good = { /* from NTP, GPS, user input, etc. */ };
    ds3231_set_datetime(&rtc, &known_good);
    ds3231_clear_oscillator_stop_flag(&rtc);
}
```

---

## 📝 9. Design Notes & Known Limitations

*   **QNX-only:** The implementation is built directly on QNX's `devctl(DCMD_I2C_SENDRECV/DCMD_I2C_SEND, ...)` interface. Porting to Linux requires swapping the transport layer.
*   **7-bit addressing only:** `I2C_ADDRFMT_7BIT`.
*   **Alarm setters bypass the handle mutex:** `ds3231_set_alarm1()` and `ds3231_set_alarm2()` bypass the locked write path. If your app configures alarms concurrently, add external locking.
*   **24-hour mode enforced:** Every "set" function always writes the hour in 24-hour form.
*   **Shared SQW/INT pin:** Enabling square-wave output means alarm interrupts will not be visible on the pin, even if alarms are enabled (though flags still latch).
*   **Year range:** Hardware and every "set" path top out at year 2099.

---

## 👨‍💻 10. Credits

<div align="center">
  <p>Designed and developed by <b>Madeti Pavan Kumar</b>.</p>
  
  <a href="https://github.com/Pavan-1522"><img src="https://img.shields.io/badge/GitHub-100000?style=for-the-badge&logo=github&logoColor=white" alt="GitHub"></a>
  <a href="https://www.linkedin.com/in/pavankumarmadeti/"><img src="https://img.shields.io/badge/LinkedIn-0077B5?style=for-the-badge&logo=linkedin&logoColor=white" alt="LinkedIn"></a>
  <a href="https://pavankumarmadeti.elegets.in/"><img src="https://img.shields.io/badge/Website-4285F4?style=for-the-badge&logo=google-chrome&logoColor=white" alt="Website"></a>
</div>

