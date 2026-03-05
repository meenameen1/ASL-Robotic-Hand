#include "pico/stdlib.h"
#include "hardware/i2c.h"

// RP35
#define I2C_PORT i2c0
#define I2C_SDA_PIN 20
#define I2C_SCL_PIN 21

// PCA9685
#define PCA9685_I2C_ADDRESS 0x40

static void init_i2c(void) {
    // 1) Turn on and configure the I2C peripheral hardware (I2C_PORT = i2c0)
    i2c_init(I2C_PORT, 100 * 1000);

    // 2) Tell the GPIO mux: "this pin is controlled by the I2C peripheral now"
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);

    // 3) Enable weak internal pull-ups (helps the lines idle HIGH for I2C)
    //    Many PCA9685 breakout boards already have external pull-ups; internal ones are just a backup.
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

static void pca9685_write_reg(uint8_t reg, uint8_t value) {
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
    i2c_write_blocking(I2C_PORT, PCA9685_I2C_ADDRESS, buf, 2, false);
}

static uint8_t pca9685_read_reg(uint8_t reg) {

    // 1) Tell the PCA9685 which register we want to read
    i2c_write_blocking(I2C_PORT, PCA9685_I2C_ADDRESS, &reg, 1, true);

    // 2) Variable to store the value we read
    uint8_t value;

    // 3) Read one byte from the PCA9685
    i2c_read_blocking(I2C_PORT, PCA9685_I2C_ADDRESS, &value, 1, false);

    // 4) Return the byte we read
    return value;
}

#define PCA9685_MODE1     0x00
#define PCA9685_PRESCALE  0xFE

static void pca9685_set_pwm_freq(float hz) {
    // 1) Convert desired frequency (Hz) into the PCA9685 PRESCALE value.
    //    Formula from PCA9685 datasheet:
    //    prescale = round(osc_clock / (4096 * hz)) - 1
    //    Most boards use osc_clock = 25,000,000 Hz (25 MHz).
    float prescale_f = (25000000.0f / (4096.0f * hz)) - 1.0f;

    // 2) Convert the float to the nearest integer prescale value.
    //    Add 0.5f before casting to do rounding (instead of truncation).
    uint8_t prescale = (uint8_t)(prescale_f + 0.5f);

    // 3) Read the current MODE1 register so we don't accidentally overwrite other bits.
    uint8_t old_mode = pca9685_read_reg(PCA9685_MODE1);

    // 4) Put the chip into SLEEP mode so PRESCALE can be changed safely.
    //    SLEEP bit is bit 4 (0x10).
    uint8_t sleep_mode = (old_mode & 0x7F) | 0x10;

    // 5) Write MODE1 with SLEEP=1 (go to sleep).
    pca9685_write_reg(PCA9685_MODE1, sleep_mode);

    // 6) Small delay to let the oscillator stop cleanly.
    sleep_ms(1);

    // 7) Write the computed PRESCALE value.
    pca9685_write_reg(PCA9685_PRESCALE, prescale);

    // 8) Delay for the prescale write to take effect.
    sleep_ms(1);

    // 9) Restore MODE1 (wake up: SLEEP=0).
    pca9685_write_reg(PCA9685_MODE1, old_mode);

    // 10) Give it time to restart.
    sleep_ms(1);

    // 11) Optional but common: set RESTART bit (bit 7) so PWM restarts cleanly.
    pca9685_write_reg(PCA9685_MODE1, old_mode | 0x80);
}

#define PCA9685_LED0_ON_L 0x06

static void pca9685_set_pwm(uint8_t channel, uint16_t on, uint16_t off) {
    // 1) Compute the base register address for this channel.
    //    Channel 0 starts at LED0_ON_L (0x06).
    //    Each channel uses 4 registers: ON_L, ON_H, OFF_L, OFF_H.
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
    i2c_write_blocking(I2C_PORT, PCA9685_I2C_ADDRESS, buf, 5, false);
}

static void servo_set_us(uint8_t channel, uint16_t pulse_us) {
    // 1) Clamp pulse width to a safe typical servo range.
    //    Many servos use ~1000..2000us. Some can do ~500..2500us, but start safe.
    if (pulse_us < 1000) pulse_us = 1000;
    if (pulse_us > 2000) pulse_us = 2000;

    // 2) At 50 Hz, one frame is 20,000 microseconds (20 ms).
    //    The PCA9685 frame is always 4096 counts long (0..4095).
    //    So we convert microseconds to counts:
    //      counts = (pulse_us / 20000) * 4096
    //    Use integer math with rounding.
    uint32_t counts = (pulse_us * 4096u + 10000u) / 20000u;

    // 3) Ensure counts stays in range (0..4095)
    if (counts > 4095u) counts = 4095u;

    // 4) Create the pulse by turning ON at 0 and OFF at 'counts'.
    //    That means: output is HIGH for 'counts' ticks each frame.
    pca9685_set_pwm(channel, 0, (uint16_t)counts);
}

#define PCA9685_MODE2 0x01

static void pca9685_init_for_servos(void) {
    // 1) Initialize the RP2350 I2C controller and configure the SDA/SCL pins.
    init_i2c();

    // 2) Small delay so the bus and device power have time to stabilize.
    sleep_ms(10);

    // 3) Set MODE2:
    //    0x04 sets "OUTDRV" = 1 (totem-pole output). Most PCA9685 servo boards expect this.
    //    This affects how the output pins drive HIGH/LOW.
    pca9685_write_reg(PCA9685_MODE2, 0x04);

    // 4) Set MODE1 to a known state (normal mode, not sleeping).
    //    0x00 clears SLEEP and other special modes.
    pca9685_write_reg(PCA9685_MODE1, 0x00);

    // 5) Give the oscillator time to start.
    sleep_ms(10);

    // 6) Set PWM frequency to 50 Hz (standard for hobby servos).
    pca9685_set_pwm_freq(50.0f);

    // 7) Optional: set all channels off initially (prevents surprises).
    //    You can comment this out if you want.
    for (uint8_t ch = 0; ch < 16; ch++) {
        pca9685_set_pwm(ch, 0, 0);
    }
}

void servo_test_sweep(uint8_t channel) {
    // 1) Initialize I2C + PCA9685 for servo output (50 Hz).
    pca9685_init_for_servos();

    // 2) Give things a moment before moving (optional, but helps during bring-up).
    sleep_ms(200);

    // 3) Loop forever and command the servo to three positions:
    //    1000us (one end), 1500us (center), 2000us (other end).
    while (true) {
        servo_set_us(channel, 1000);
        sleep_ms(800);

        servo_set_us(channel, 1500);
        sleep_ms(800);

        servo_set_us(channel, 2000);
        sleep_ms(800);
    }
}

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdbool.h>

bool pca9685_ack_test(void) {
    // Zero-length write: if the device exists, it will ACK its address.
    int rc = i2c_write_blocking(I2C_PORT, PCA9685_I2C_ADDRESS, NULL, 0, false);
    return (rc >= 0);
}

#define PCA9685_LED0_ON_L  0x06

static void pca9685_force_channel(uint8_t channel, bool force_high) {
    // Base register for this channel (4 regs per channel)
    uint8_t reg = PCA9685_LED0_ON_L + 4 * channel;

    // We will write: ON_L, ON_H, OFF_L, OFF_H.
    // FULL ON is bit 4 of ON_H.
    // FULL OFF is bit 4 of OFF_H.
    uint8_t on_l  = 0;
    uint8_t on_h  = force_high ? 0x10 : 0x00;  // 0x10 => FULL ON
    uint8_t off_l = 0;
    uint8_t off_h = force_high ? 0x00 : 0x10;  // 0x10 => FULL OFF

    uint8_t buf[5] = { reg, on_l, on_h, off_l, off_h };
    i2c_write_blocking(I2C_PORT, PCA9685_I2C_ADDRESS, buf, 5, false);
}


void test_force(void) {
    pca9685_write_reg(PCA9685_MODE2, 0x04); // OUTDRV=1 (totem-pole), important
    pca9685_force_channel(0, true);         // channel 0 forced HIGH
    sleep_ms(2000);
    pca9685_force_channel(0, false);        // channel 0 forced LOW
    sleep_ms(2000);
}