#ifndef LETTERS_H
#define LETTERS_H

#include <stdint.h>

#define MOTOR_COUNT 24
// #define LETTER_COUNT 26
#define POSE_COUNT 34

#define POSE_J1 26
#define POSE_J2 27
#define POSE_J3 28
#define POSE_J4 29
#define POSE_Z1 30
#define POSE_Z2 31
#define POSE_Z3 32
#define POSE_Z4 33

typedef struct {
    int motor_positions[MOTOR_COUNT];
} HandPose;

extern const HandPose hand_poses[POSE_COUNT];

void move_to_letter(char target_letter);
void move_to_letter_smoothly(char target_letter, int step_size_us, int tick_time_ms);

void init_servo_positions(void);

int letter_index(char c);

/////////////
void secondpwmtest(uint8_t channel, uint16_t pulse_us);

#endif
