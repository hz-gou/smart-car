#ifndef WIFI_IMU963RA_VOFA_IMU963_H
#define WIFI_IMU963RA_VOFA_IMU963_H

#include <stdbool.h>
#include <stdint.h>

bool  imu963_init(void);
void  imu963_update(uint32_t elapsed_ms);
bool  imu963_is_ready(void);
float imu963_get_angle_z(void);
float imu963_get_angle_x(void);
float imu963_get_angle_y(void);

#endif
