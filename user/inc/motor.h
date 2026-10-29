#ifndef WIFI_IMU963RA_VOFA_MOTOR_H
#define WIFI_IMU963RA_VOFA_MOTOR_H

#include <stdbool.h>

void  motor_init(void);
void  motor_stop(void);
void  motor_update_target_from_angle(float angle_x_deg, bool imu_ready);
void  motor_update_control(void);
float motor_get_target_rps(void);
float motor_get_actual_rps(void);
float motor_get_duty_percent(void);
bool  motor_is_stall_latched(void);

#endif
