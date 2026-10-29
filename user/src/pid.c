#include "pid.h"

static float pid_clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

void pid_init(pid_controller_struct *pid, float kp, float ki, float kd,
              float integral_limit)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->integral_limit = integral_limit;
    pid_reset(pid, 0.0f);
}

void pid_reset(pid_controller_struct *pid, float measurement)
{
    pid->integral = 0.0f;
    pid->previous_measurement = measurement;
}

float pid_calculate(pid_controller_struct *pid, float error,
                    float measurement, float delta_seconds)
{
    float derivative;
    float output;

    if (delta_seconds <= 0.0f) {
        return 0.0f;
    }

    derivative =
        (measurement - pid->previous_measurement) / delta_seconds;
    output = (pid->kp * error) +
             (pid->ki * pid->integral) -
             (pid->kd * derivative);
    pid->previous_measurement = measurement;
    return output;
}

void pid_update_integral(pid_controller_struct *pid, float error,
                         float delta_seconds)
{
    pid->integral += error * delta_seconds;
    pid->integral = pid_clamp(
        pid->integral, -pid->integral_limit, pid->integral_limit);
}
