#ifndef LETTERS_H
#define LETTERS_H

#include <stdint.h>

#define MOTOR_COUNT 24
#define LETTER_COUNT 26

typedef struct {
    int motor_positions[MOTOR_COUNT];
} HandPose;

extern const HandPose hand_poses[LETTER_COUNT];

void move_to_letter(char target_letter);
void move_to_letter_smoothly(char target_letter, int step_size_us, int tick_time_ms);

void init_servo_positions(void);

int letter_index(char c);

/////////////
void secondpwmtest(uint8_t channel, uint16_t pulse_us);

#endif
