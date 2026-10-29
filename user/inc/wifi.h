#ifndef WIFI_IMU963RA_VOFA_WIFI_H
#define WIFI_IMU963RA_VOFA_WIFI_H

#include <stdbool.h>
#include <stdint.h>

void WiFi_Init(void);
bool WiFi_IsConnected(void);
bool WiFi_Send(const uint8_t *data, uint32_t length, uint32_t now_ms);
void WiFi_Update(uint32_t now_ms);

#endif
