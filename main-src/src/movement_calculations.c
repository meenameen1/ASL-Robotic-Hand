#include "letter_positions.h"


MovementInstructions calculate_delta(char current_letter, char target_letter) {
    MovementInstructions instructions;
    for (int motor_index = 0; motor_index < MOTOR_COUNT; motor_index++) {
        instructions.deltas[motor_index] = hand_poses[target_letter - 'A'].motor_positions[motor_index] - 
                                           hand_poses[current_letter - 'A'].motor_positions[motor_index];
    }
    return instructions;
}