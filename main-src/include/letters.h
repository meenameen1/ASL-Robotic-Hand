// ONLY INCLUDE THIS HEADER ONE TIME IN CALCULATIONS.C
// DO NOT INCLUDE ELSEWHERE --> LINKER ERROR

#ifndef LETTERS_H
#define LETTERS_H

#define MOTOR_COUNT 22
#define LETTER_COUNT 26

typedef struct {
    int motor_positions[MOTOR_COUNT];
} HandPose;

typedef struct {
    int deltas[MOTOR_COUNT];
} MovementInstructions;

extern const HandPose hand_poses[LETTER_COUNT];

MovementInstructions calculate_delta(char current_letter, char target_letter);

#endif
