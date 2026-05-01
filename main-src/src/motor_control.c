#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdbool.h>

#include "letters.h"

// RP35
// #define I2C_PORT i2c1
// #define I2C_SDA_PIN 38
// #define I2C_SCL_PIN 39
#define I2C_PORT i2c1
#define I2C_SDA_PIN 14
#define I2C_SCL_PIN 15

// PCA9685
#define PCA9685_I2C_ADDRESS_1 0x40
#define PCA9685_I2C_ADDRESS_2 0x41
#define PCA9685_CLOCK_FREQ_HZ 25000000
#define PCA9685_MODE1 0x00
#define PCA9685_MODE2 0x01
#define PCA9685_PRESCALE 0xFE
#define PCA9685_LED0_ON_L 0x06

#define SERVO_MIN_US 500
#define SERVO_MAX_US 2700
#define SERVO_MID_US 1600

static int current_us_values[MOTOR_COUNT];

static void init_i2c(void) {

    i2c_init(I2C_PORT, 100 * 1000);

    // 2) Tell the GPIO mux: "this pin is controlled by the I2C peripheral now"
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);

    // 3) Enable weak internal pull-ups (helps the lines idle HIGH for I2C)
    //    Many PCA9685 breakout boards already have external pull-ups; internal ones are just a backup.
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

static void pca9685_write_reg(uint8_t reg, uint8_t value, uint8_t address) {
    // 1) Make a 2-byte buffer: [register_address, register_value]
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = value;

    // 2) Send those 2 bytes to the PCA9685 over I2C
    //    - I2C_PORT: which I2C controller to use (i2c0)
    //    - PCA9685_I2C_ADDRESS: the device address (usually 0x40)
    //    - buf: pointer to bytes to send
    //    - 2: number of bytes to send
    //    - false: send a STOP condition at the end (end of transaction)
    i2c_write_blocking(I2C_PORT, address, buf, 2, false);
}

static uint8_t pca9685_read_reg(uint8_t reg, uint8_t address) {

    // 1) Tell the PCA9685 which register we want to read
    i2c_write_blocking(I2C_PORT, address, &reg, 1, true);

    // 2) Variable to store the value we read
    uint8_t value;

    // 3) Read one byte from the PCA9685
    i2c_read_blocking(I2C_PORT, address, &value, 1, false);

    // 4) Return the byte we read
    return value;
}

static void pca9685_set_pwm_freq(float hz, uint8_t address) {
    // 1) Convert desired frequency (Hz) into the PCA9685 PRESCALE value.
    //    Formula from PCA9685 datasheet:
    //    prescale = round(osc_clock / (4096 * hz)) - 1
    //    Most boards use osc_clock = 25,000,000 Hz (25 MHz).
    float prescale_f = (25000000.0f / (4096.0f * hz)) - 1.0f;

    // 2) Convert the float to the nearest integer prescale value.
    //    Add 0.5f before casting to do rounding (instead of truncation).
    uint8_t prescale = (uint8_t)(prescale_f + 0.5f);

    // 3) Read the current MODE1 register so we don't accidentally overwrite other bits.
    uint8_t old_mode = pca9685_read_reg(PCA9685_MODE1, address);

    // 4) Put the chip into SLEEP mode so PRESCALE can be changed safely.
    //    SLEEP bit is bit 4 (0x10).
    uint8_t sleep_mode = (old_mode & 0x7F) | 0x10;

    // 5) Write MODE1 with SLEEP=1 (go to sleep).
    pca9685_write_reg(PCA9685_MODE1, sleep_mode, address);

    // 6) Small delay to let the oscillator stop cleanly.
    sleep_ms(1);

    // 7) Write the computed PRESCALE value.
    pca9685_write_reg(PCA9685_PRESCALE, prescale, address);

    // 8) Delay for the prescale write to take effect.
    sleep_ms(1);

    // 9) Restore MODE1 (wake up: SLEEP=0).
    pca9685_write_reg(PCA9685_MODE1, old_mode, address);

    // 10) Give it time to restart.
    sleep_ms(1);

    // 11) Optional but common: set RESTART bit (bit 7) so PWM restarts cleanly.
    pca9685_write_reg(PCA9685_MODE1, old_mode | 0x80, address);
}

static void pca9685_set_pwm(uint8_t channel, uint16_t on, uint16_t off, uint8_t address) {
    // 1) Compute the base register address for this channel.
    //    Channel 0 starts at LED0_ON_L (0x06).
    //    Each channel uses 4 registers: ON     _L, ON_H, OFF_L, OFF_H.
    uint8_t reg = PCA9685_LED0_ON_L + (4 * channel);

    // 2) Build a 5-byte buffer:
    //    [start_register, ON_L, ON_H, OFF_L, OFF_H]
    uint8_t buf[5];
    buf[0] = reg;

    // 3) Split the 16-bit 'on' value into low and high bytes.
    buf[1] = (uint8_t)(on & 0xFF);       // ON_L
    buf[2] = (uint8_t)((on >> 8) & 0xFF); // ON_H

    // 4) Split the 16-bit 'off' value into low and high bytes.
    buf[3] = (uint8_t)(off & 0xFF);       // OFF_L
    buf[4] = (uint8_t)((off >> 8) & 0xFF); // OFF_H

    // 5) Write all 4 PWM registers in one I2C transaction.
    //    This relies on the PCA9685 auto-increment feature (common default / recommended).
    i2c_write_blocking(I2C_PORT, address, buf, 5, false);
}

static void servo_set_us(uint8_t channel, uint16_t pulse_us) {
    uint8_t address = PCA9685_I2C_ADDRESS_1;
    if (channel > 15) {
        channel -= 16;
        address = PCA9685_I2C_ADDRESS_2;
    }
    //     ~500..2500us pusle width 0 - 180 degrees
    if (pulse_us < SERVO_MIN_US) pulse_us = SERVO_MIN_US;
    if (pulse_us > SERVO_MAX_US) pulse_us = SERVO_MAX_US;

    // 2) At 50 Hz, one frame is 20,000 microseconds (20 ms).
    //    The PCA9685 frame is always 4096 counts long (0..4095).
    //    So we convert microseconds to counts:
    //      counts = (pulse_us / 20000) * 4096
    //    Use integer math with rounding.
    uint32_t counts = (pulse_us * 4096u) / 20000u;

    // 3) Ensure counts stays in range (0..4095)
    if (counts > 4095u) counts = 4095u;

    // 4) Create the pulse by turning ON at 0 and OFF at 'counts'.
    //    That means: output is HIGH for 'counts' ticks each frame.
    pca9685_set_pwm(channel, 0, (uint16_t)counts, address);
}

static void pca9685_init_for_servos(void) {
    // 1) Initialize the RP2350 I2C controller and configure the SDA/SCL pins.
    init_i2c();

    // 2) Small delay so the bus and device power have time to stabilize.
    sleep_ms(10);

    // 3) Set MODE2:
    //    0x04 sets "OUTDRV" = 1 (totem-pole output). Most PCA9685 servo boards expect this.
    //    This affects how the output pins drive HIGH/LOW.
    pca9685_write_reg(PCA9685_MODE2, 0x04, PCA9685_I2C_ADDRESS_1);
    pca9685_write_reg(PCA9685_MODE2, 0x04, PCA9685_I2C_ADDRESS_2);

    // 4) Set MODE1 to a known state (normal mode, not sleeping).
    //    0x00 clears SLEEP and other special modes.
    pca9685_write_reg(PCA9685_MODE1, 0x00, PCA9685_I2C_ADDRESS_1);
    pca9685_write_reg(PCA9685_MODE1, 0x00, PCA9685_I2C_ADDRESS_2);

    // 5) Give the oscillator time to start.
    sleep_ms(10);

    // 6) Set PWM frequency to 50 Hz (standard for hobby servos).
    pca9685_set_pwm_freq(50.0f, PCA9685_I2C_ADDRESS_1);
    pca9685_set_pwm_freq(50.0f, PCA9685_I2C_ADDRESS_2);

    // 7) Optional: set all channels off initially (prevents surprises).
    //    You can comment this out if you want.
    for (uint8_t ch = 0; ch < 16; ch++) {
        pca9685_set_pwm(ch, 0, 0, PCA9685_I2C_ADDRESS_1);
        pca9685_set_pwm(ch, 0, 0, PCA9685_I2C_ADDRESS_2);
    }
}

static const uint32_t swappedMotorsIndices[MOTOR_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 20, 21, 22, 23, 19, 18, 17, 16};

void move_to_letter(char target_letter) {
    if(target_letter == 'J') {
        move_to_letter_smoothly('J', 50, 50);
    }
    else if(target_letter == 'Z') {
        move_to_letter_smoothly('Z', 50, 50);
    }
    else{
        for (int servo_index = 0; servo_index < MOTOR_COUNT; servo_index++) {
        servo_set_us(servo_index, hand_poses[letter_index(target_letter)].motor_positions[swappedMotorsIndices[servo_index]]);
        }
    }
}

int clamp_us(int value) {
    if (value < SERVO_MIN_US) return SERVO_MIN_US;
    if (value > SERVO_MAX_US) return SERVO_MAX_US;
    return value;
}

void move_to_letter_smoothly(char target_letter, int step_size_us, int tick_time_ms) {
    if (step_size_us <= 0) step_size_us = 5;
    if (tick_time_ms <= 0) tick_time_ms = 5;

    int pose_index = letter_index(target_letter);

    // Optional safety check, depending on how letter_index works
    if (pose_index < 0) {
        return;
    }

    // Match behavior of move_to_letter()
    if (target_letter == 'J') {
        move_to_letter_smoothly(POSE_J1, step_size_us, tick_time_ms);
        move_to_letter_smoothly(POSE_J2, step_size_us, tick_time_ms);
        move_to_letter_smoothly(POSE_J3, step_size_us, tick_time_ms);
        move_to_letter_smoothly(POSE_J4, step_size_us, tick_time_ms);
        return;
    }
    else if (target_letter == 'Z') {
        move_to_letter_smoothly(POSE_Z1, step_size_us, tick_time_ms);
        move_to_letter_smoothly(POSE_Z2, step_size_us, tick_time_ms);
        move_to_letter_smoothly(POSE_Z3, step_size_us, tick_time_ms);
        move_to_letter_smoothly(POSE_Z4, step_size_us, tick_time_ms);
        return;
    }

    while (1) {
        bool complete = true;

        for (int servo_index = 0; servo_index < MOTOR_COUNT; servo_index++) {
            int target = hand_poses[pose_index].motor_positions[swappedMotorsIndices[servo_index]];
            target = clamp_us(target);

            int current = current_us_values[swappedMotorsIndices[servo_index]];
            current = clamp_us(current);

            if (current < target) {
                current += step_size_us;

                if (current > target) {
                    current = target;
                }
            }
            else if (current > target) {
                current -= step_size_us;

                if (current < target) {
                    current = target;
                }
            }

            current = clamp_us(current);

            if (current != target) {
                complete = false;
            }

            current_us_values[swappedMotorsIndices[servo_index]] = current;
            servo_set_us(servo_index, (uint16_t)current);
        }

        if (complete) {
            return;
        }

        sleep_ms(tick_time_ms);
    }
}

void init_servo_positions(void) {
    pca9685_init_for_servos();
    init_i2c();
    sleep_ms(1000);
    pca9685_set_pwm_freq(50.0f, PCA9685_I2C_ADDRESS_1);
    pca9685_set_pwm_freq(50.0f, PCA9685_I2C_ADDRESS_2);
    sleep_ms(10);
    pca9685_write_reg(PCA9685_MODE1, 0x20, PCA9685_I2C_ADDRESS_1); // Need this line for the auto increment. After a read or write the control register is incremented
    pca9685_write_reg(PCA9685_MODE1, 0x20, PCA9685_I2C_ADDRESS_2);
    sleep_ms(10); // Wait for oscillator to stabilize
    pca9685_write_reg(PCA9685_MODE2, 0x04, PCA9685_I2C_ADDRESS_1); // OUTDRV=1 (totem-pole), important
    pca9685_write_reg(PCA9685_MODE2, 0x04, PCA9685_I2C_ADDRESS_2);

    for (int i = 0; i < MOTOR_COUNT; i++) {
        servo_set_us(i, SERVO_MID_US);
        current_us_values[i] = SERVO_MID_US;
    }
    sleep_ms(1000);
}

void secondpwmtest(uint8_t channel, uint16_t pulse_us) {
    servo_set_us(swappedMotorsIndices[channel], pulse_us);
    // servo_index, hand_poses[letter_index(target_letter)].motor_positions[servo_index])
}
