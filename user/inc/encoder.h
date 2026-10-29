#ifndef WIFI_IMU963RA_VOFA_ENCODER_H
#define WIFI_IMU963RA_VOFA_ENCODER_H

#include <stdint.h>

void     encoder_init(void);
void     encoder_reset(void);
uint32_t encoder_get_pulses_and_reset(void);

#endif
