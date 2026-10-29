#ifndef WIFI_IMU963RA_VOFA_PID_H
#define WIFI_IMU963RA_VOFA_PID_H

typedef struct
{
    float kp;
    float ki;
    float kd;
    float integral;
    float integral_limit;
    float previous_measurement;
} pid_controller_struct;

void  pid_init(pid_controller_struct *pid, float kp, float ki, float kd,
               float integral_limit);
void  pid_reset(pid_controller_struct *pid, float measurement);
float pid_calculate(pid_controller_struct *pid, float error,
                    float measurement, float delta_seconds);
void  pid_update_integral(pid_controller_struct *pid, float error,
                          float delta_seconds);

#endif
