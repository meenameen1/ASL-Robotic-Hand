#ifndef LETTERS_H
#define LETTERS_H

#define MOTOR_COUNT 22
#define LETTER_COUNT 26

typedef struct {
    int motor_positions[MOTOR_COUNT];
} HandPose;

extern const HandPose hand_poses[LETTER_COUNT];

void move_to_letter(char target_letter);
void move_to_letter_smoothly(char target_letter, int step_size_us, int tick_time_ms);

void init_servo_positions(void);

int letter_index(char c);

#endif
