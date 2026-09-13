<div align="center">

# 🏎️ L298N Dual H-Bridge Motor Driver
### Software Datasheet

[![Platform](https://img.shields.io/badge/Platform-QNX_SDP_8.0-blue?style=for-the-badge&logo=qnx)](#)
[![Hardware](https://img.shields.io/badge/Hardware-BCM2711_(Raspberry_Pi_4)-C51A4A?style=for-the-badge&logo=raspberry-pi)](#)
[![Language](https://img.shields.io/badge/Language-C_(C11)-00599C?style=for-the-badge&logo=c)](#)
[![Maintained by](https://img.shields.io/badge/Maintained_by-Madeti_Pavan_Kumar-success?style=for-the-badge)](https://github.com/Pavan-1522)

*A robust, thread-safe C API for controlling a single L298N dual H-bridge module.*

</div>

---

**Target platform:** QNX (POSIX threads / `pthread`, C11 `stdatomic`)  
**Language:** C (C11)  
**Files:** `l298n.h`, `l298n.c`  
**Scope:** GPIO/PWM-level control of one L298N module driving a two-motor differential (skid-steer) rover  

---

## 🎯 1. Overview

This driver provides a thread-safe C API for controlling a single **L298N dual H-bridge** module from a QNX application. It owns GPIO direction pins and PWM enable pins only — it has **no** navigation, obstacle-avoidance, PID/closed-loop control, route planning, or sensor logic inside it. It is meant to sit underneath a separate motor-control task in a multithreaded QNX robot application.

| Property | Value |
|---|---|
| **Motors controlled** | 2 (left, right — differential/skid-steer) |
| **Speed range** | `-100` to `+100` (signed, percent PWM duty) |
| **Direction control** | Per-wheel, via sign of speed |
| **Thread safety** | Yes — mutex-protected, with a lock-free emergency-stop path |
| **Configuration** | Fully caller-supplied via `l298n_config_t` (no hard-coded pins) |
| **Error reporting** | Explicit `l298n_err_t` codes on every call |
| **Dependencies** | `<stdbool.h>`, `<pthread.h>`, `<stdatomic.h>`, `rpi_gpio.h` (your GPIO/PWM HAL) |

---

## ⚙️ 2. Hardware Model

The L298N has two independent H-bridges, each needing one PWM enable line and two direction pins:

| L298N pin | Config field | Function |
|:---:|:---:|---|
| **IN1** | `in1` | Left motor direction |
| **IN2** | `in2` | Left motor direction |
| **IN3** | `in3` | Right motor direction |
| **IN4** | `in4` | Right motor direction |
| **ENA** | `ena` | Left motor PWM (speed) |
| **ENB** | `enb` | Right motor PWM (speed) |

All six pin numbers, plus the PWM frequency, are supplied by the caller at init time — nothing is hard-coded inside the driver, so it's portable across boards as long as `rpi_gpio.h` is implemented for that board.

### 2.1 Direction convention

> ⚠️ **IMPORTANT:** This is the contract the driver assumes. **Verify it matches your physical wiring** before trusting any direction command.

```text
Left motor (IN1 / IN2):
    IN1 = HIGH, IN2 = LOW   -> left motor spins FORWARD
    IN1 = LOW,  IN2 = HIGH  -> left motor spins BACKWARD

Right motor (IN3 / IN4):
    IN3 = HIGH, IN4 = LOW   -> right motor spins FORWARD
    IN3 = LOW,  IN4 = HIGH  -> right motor spins BACKWARD
```

**Rover-level behavior built from this convention:**

```text
             FORWARD                    PIVOT LEFT
         L: fwd   R: fwd            L: back   R: fwd
             ↑ ↑                        ↺
         ┌───────┐                  ┌───────┐
         │ ROVER │                  │ ROVER │
         └───────┘                  └───────┘

            BACKWARD                   PIVOT RIGHT
         L: back  R: back           L: fwd   R: back
             ↓ ↓                        ↻
         ┌───────┐                  ┌───────┐
         │ ROVER │                  │ ROVER │
         └───────┘                  └───────┘
```

---

## 📦 3. Data Types

### 3.1 `l298n_config_t`

```c
typedef struct
{
    int in1;    /* left motor direction pin  */
    int in2;    /* left motor direction pin  */

    int in3;    /* right motor direction pin */
    int in4;    /* right motor direction pin */

    int ena;    /* left motor PWM enable pin  */
    int enb;    /* right motor PWM enable pin */

    unsigned int pwm_frequency;    /* PWM frequency in Hz, must be > 0 */

} l298n_config_t;
```

Passed once to `l298n_init()`. Every pin must be non-negative and distinct from every other pin in the struct — the driver validates this and rejects duplicates/negatives before touching any hardware.

### 3.2 `l298n_status_t`

```c
typedef struct
{
    int left_speed;    /* signed, -100..100, same convention as commands */
    int right_speed;
    bool enabled;       /* true if either wheel is non-zero */
} l298n_status_t;
```

Reflects the **last successfully commanded** state — this is not sensor feedback (no encoders or current sensing exist in this driver).

### 3.3 `l298n_err_t`

```c
typedef enum
{
    L298N_OK                  = 0,   /* success                            */
    L298N_ERR_INVALID_ARG     = -1,  /* NULL pointer or out-of-range value */
    L298N_ERR_NOT_INITIALIZED = -2,  /* driver not initialized             */
    L298N_ERR_ALREADY_INIT    = -3,  /* l298n_init() called while running  */
    L298N_ERR_HW              = -4   /* underlying GPIO/PWM call failed    */
} l298n_err_t;
```

Every function returns `int`, using these values (`L298N_OK` on success, one of the negative codes on failure).

### 3.4 Constants

| Constant | Value | Meaning |
|---|:---:|---|
| `L298N_SPEED_MIN` | `-100` | Full reverse |
| `L298N_SPEED_MAX` | `100` | Full forward |
| `L298N_MAX_PWM_FREQUENCY_HZ` | `1000000` | Sanity ceiling on configured PWM frequency — catches unit mistakes at init, not a hardware limit |

---

## 🛠️ 4. API Reference

### 4.1 `l298n_init`

```c
int l298n_init(const l298n_config_t *config);
```

Validates `config`, configures all six GPIO/PWM pins, and leaves the driver in a stopped, known-safe state.

| | |
|---|---|
| **Parameters** | `config` — pin/frequency configuration. Must not be `NULL`. |
| **Returns** | `L298N_OK`, or `L298N_ERR_INVALID_ARG` / `L298N_ERR_ALREADY_INIT` / `L298N_ERR_HW` |
| **Preconditions** | Driver must not already be initialized (call `l298n_deinit()` first to reconfigure) |
| **Guarantees** | Motors are stopped at return, whether init succeeds or fails partway through |
| **Concurrency** | Must not be called concurrently with itself, `l298n_deinit()`, or any other `l298n_*` call |

### 4.2 `l298n_deinit`

```c
int l298n_deinit(void);
```

Stops the motors and marks the driver uninitialized.

| | |
|---|---|
| **Returns** | `L298N_OK`, or `L298N_ERR_NOT_INITIALIZED` / `L298N_ERR_HW` |
| **Concurrency** | Same rule as `l298n_init()` |

### 4.3 `l298n_forward` / `l298n_backward`

```c
int l298n_forward(int speed);
int l298n_backward(int speed);
```

Drive both wheels the same direction at the same speed.

| | |
|---|---|
| **Parameters** | `speed` — magnitude, `0` to `L298N_SPEED_MAX` (unsigned range; direction is fixed by which function you call) |
| **Returns** | `L298N_OK`, or `L298N_ERR_INVALID_ARG` / `L298N_ERR_NOT_INITIALIZED` / `L298N_ERR_HW` |

### 4.4 `l298n_left` / `l298n_right`

```c
int l298n_left(int speed);   /* pivot: left backward, right forward */
int l298n_right(int speed);  /* pivot: left forward,  right backward */
```

Pivot turns in place — the two wheels are driven in opposite directions at equal magnitude.

| | |
|---|---|
| **Parameters** | `speed` — magnitude, `0` to `L298N_SPEED_MAX` |
| **Returns** | Same as §4.3 |

### 4.5 `l298n_set_motor_speed` — primary control entry point

```c
int l298n_set_motor_speed(int left_speed, int right_speed);
```

Full differential drive: each wheel gets an independent **signed** speed. This is what a higher-level motor-control task should call for arbitrary steering (not just the four canned directions above).

| | |
|---|---|
| **Parameters** | `left_speed`, `right_speed` — each `L298N_SPEED_MIN` (`-100`) to `L298N_SPEED_MAX` (`100`). Sign selects direction per wheel; magnitude selects PWM duty. |
| **Returns** | `L298N_OK`, or `L298N_ERR_INVALID_ARG` / `L298N_ERR_NOT_INITIALIZED` / `L298N_ERR_HW` |
| **Fail-safe behavior** | If a GPIO/PWM call fails partway through applying the command, the driver does **not** leave the H-bridges half-set — it forces a full stop and returns `L298N_ERR_HW`. |

* **Examples:** 
  * `l298n_set_motor_speed(80, 80)` → straight forward at 80%. 
  * `l298n_set_motor_speed(-50, 50)` → pivot right in place at 50%. 
  * `l298n_set_motor_speed(30, 60)` → gentle left-curving forward arc.

### 4.6 `l298n_stop`

```c
int l298n_stop(void);
```

Routine stop: PWM duty → 0, all direction pins → low.

| | |
|---|---|
| **Returns** | `L298N_OK`, or `L298N_ERR_NOT_INITIALIZED` / `L298N_ERR_HW` |

### 4.7 `l298n_emergency_stop` — safety-critical stop

```c
int l298n_emergency_stop(void);
```

Reaches the **same electrical end-state** as `l298n_stop()`, but through a different, safety-oriented code path:

- Does **not** wait on the driver's internal mutex — so it can't be blocked by another thread stuck mid-call (e.g. a hung GPIO write). Hardware is written to a safe state immediately.
- Afterward, makes one **non-blocking** attempt to also update the status returned by `l298n_get_status()`. If the lock isn't immediately available, the hardware is still guaranteed safe — the status snapshot may just lag briefly until the next successful call.

| | |
|---|---|
| **Returns** | `L298N_OK`, or `L298N_ERR_NOT_INITIALIZED` / `L298N_ERR_HW` |
| **Intended callers** | A fault/watchdog task, a signal path, or any thread that needs a hard guarantee the motors go safe *now* |

### 4.8 `l298n_get_status`

```c
int l298n_get_status(l298n_status_t *status);
```

Returns a consistent (mutex-protected, non-torn) snapshot of the last commanded state.

| | |
|---|---|
| **Parameters** | `status` — output pointer, must not be `NULL` |
| **Returns** | `L298N_OK`, or `L298N_ERR_INVALID_ARG` / `L298N_ERR_NOT_INITIALIZED` |

---

## 🔒 5. Thread Safety Summary

| Function | Blocks on mutex? | Safe to call from a fault/watchdog thread while another call is in flight? |
|---|:---:|---|
| `l298n_init` / `l298n_deinit` | Yes | No — must not overlap any other `l298n_*` call |
| `l298n_forward/backward/left/right` | Yes | Yes (will simply wait its turn) |
| `l298n_set_motor_speed` | Yes | Yes |
| `l298n_stop` | Yes | Yes |
| `l298n_get_status` | Yes | Yes |
| **`l298n_emergency_stop`** | **No (hardware path is lock-free)** | **Yes — this is its purpose** |

---

## 💻 6. Example Code

### 6.1 Basic initialization and straight-line drive

```c
#include "l298n.h"
#include <stdio.h>

int main(void)
{
    l298n_config_t cfg = {
        .in1 = 17, .in2 = 27,
        .in3 = 22, .in4 = 23,
        .ena = 12, .enb = 13,
        .pwm_frequency = 1000   /* 1 kHz */
    };

    int rc = l298n_init(&cfg);
    if (rc != L298N_OK)
    {
        fprintf(stderr, "l298n_init failed: %d\n", rc);
        return 1;
    }

    l298n_forward(60);          /* straight ahead at 60% */
    /* ... hold for some duration via your QNX timer/sleep ... */

    l298n_set_motor_speed(-40, 40);  /* pivot right at 40% */
    /* ... */

    l298n_stop();
    l298n_deinit();

    return 0;
}
```

### 6.2 Differential steering from a control loop

```c
/* Called periodically by a motor-control task, e.g. every 20 ms. */
void motor_control_tick(float forward_cmd, float turn_cmd)
{
    /* forward_cmd, turn_cmd in [-1.0, 1.0], from your navigation layer */
    int left  = (int)((forward_cmd + turn_cmd) * L298N_SPEED_MAX);
    int right = (int)((forward_cmd - turn_cmd) * L298N_SPEED_MAX);

    if (left  > L298N_SPEED_MAX) left  = L298N_SPEED_MAX;
    if (left  < L298N_SPEED_MIN) left  = L298N_SPEED_MIN;
    if (right > L298N_SPEED_MAX) right = L298N_SPEED_MAX;
    if (right < L298N_SPEED_MIN) right = L298N_SPEED_MIN;

    int rc = l298n_set_motor_speed(left, right);
    if (rc != L298N_OK)
    {
        /* Driver already fails safe internally on L298N_ERR_HW,
         * but the caller should still log/escalate. */
    }
}
```

### 6.3 QNX watchdog thread using `l298n_emergency_stop`

```c
#include "l298n.h"
#include <pthread.h>
#include <unistd.h>

static volatile sig_atomic_t fault_detected = 0;

void *watchdog_thread(void *arg)
{
    (void)arg;

    while (1)
    {
        if (fault_detected)
        {
            /* Guaranteed non-blocking hardware-safing, even if the
             * motor-control thread is currently stuck mid-call. */
            l298n_emergency_stop();
        }

        usleep(10000);   /* poll every 10 ms */
    }

    return NULL;
}

/* elsewhere: pthread_create(&tid, NULL, watchdog_thread, NULL); */
```

### 6.4 Handling status

```c
l298n_status_t status;

if (l298n_get_status(&status) == L298N_OK)
{
    printf("left=%d right=%d enabled=%s\n",
           status.left_speed,
           status.right_speed,
           status.enabled ? "yes" : "no");
}
```

---

## ⚠️ 7. Error Handling Reference

| Code | When it occurs | Recommended caller response |
|---|---|---|
| `L298N_ERR_INVALID_ARG` | `NULL` config/status pointer, out-of-range speed, bad pin config, `pwm_frequency` of 0 or above the sanity ceiling | Programming error — fix the caller, don't retry |
| `L298N_ERR_NOT_INITIALIZED` | Any motor/status call before `l298n_init()` succeeds, or after `l298n_deinit()` | Call `l298n_init()` first |
| `L298N_ERR_ALREADY_INIT` | `l298n_init()` called while already initialized | Call `l298n_deinit()` first if reconfiguration is intended |
| `L298N_ERR_HW` | An underlying `rpi_gpio_*` call failed | Motors are already forced to a safe stopped state by the driver; log and consider this a hardware fault (wiring, HAL, power) |

---

## 📝 8. Design Notes / Out of Scope

- **No feedback path.** `l298n_status_t` reflects commanded, not measured, state. If you need actual wheel speed, add encoders and a separate sensing module — don't put it in this driver.
- **No PID, no navigation, no obstacle avoidance.** Those belong in the task that calls `l298n_set_motor_speed()`.
- **Single instance.** This driver controls one L298N via internal static state, matching a typical single-board-per-process QNX robot deployment. For multiple L298N modules, instance the struct-based state (not covered by this revision).

---

## 👨‍💻 9. Credits

<div align="center">
  <p>Designed and developed by <b>Madeti Pavan Kumar</b>.</p>
  
  <a href="https://github.com/Pavan-1522"><img src="https://img.shields.io/badge/GitHub-100000?style=for-the-badge&logo=github&logoColor=white" alt="GitHub"></a>
  <a href="https://www.linkedin.com/in/pavankumarmadeti/"><img src="https://img.shields.io/badge/LinkedIn-0077B5?style=for-the-badge&logo=linkedin&logoColor=white" alt="LinkedIn"></a>
  <a href="https://pavankumarmadeti.elegets.in/"><img src="https://img.shields.io/badge/Website-4285F4?style=for-the-badge&logo=google-chrome&logoColor=white" alt="Website"></a>
</div>

