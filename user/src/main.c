#include "ti_msp_dl_config.h"
#include "zf_common_headfile.h"

#include <stdbool.h>
#include <stdio.h>

#include "encoder.h"
#include "imu963.h"
#include "motor.h"
#include "wifi.h"

#define SYSTEM_TICK_MS         (1U)
#define CONTROL_PERIOD_MS      (20U)
#define TELEMETRY_PERIOD_MS    (20U)
#define IMU_RETRY_PERIOD_MS    (2000U)

static volatile uint32_t g_ms_tick;

void SysTick_Handler(void)
{
    g_ms_tick += SYSTEM_TICK_MS;
}

int main(void)
{
    uint32_t last_control_ms = 0U;
    uint32_t last_send_ms = 0U;
    uint32_t last_imu_retry_ms = 0U;
    bool imu_ready;

    SYSCFG_DL_init();

    WiFi_Init();
    imu_ready = imu963_init();
    printf("\r\nIMU963RA init: %s\r\n", imu_ready ? "OK" : "FAILED");

    encoder_init();
    motor_init();

    DL_SYSTICK_config(
        (uint32_t)((double)CPUCLK_FREQ *
                   ((double)SYSTEM_TICK_MS / 1000.0)));

    while (1) {
        uint32_t now_ms = g_ms_tick;

        if (!imu963_is_ready() &&
            ((uint32_t)(now_ms - last_imu_retry_ms) >=
             IMU_RETRY_PERIOD_MS)) {
            last_imu_retry_ms = now_ms;
            imu_ready = imu963_init();
            printf("\r\nIMU963RA retry: %s\r\n",
                   imu_ready ? "OK" : "FAILED");
        }

        if ((uint32_t)(now_ms - last_control_ms) >=
            CONTROL_PERIOD_MS) {
            uint32_t elapsed_ms =
                (uint32_t)(now_ms - last_control_ms);

            last_control_ms = now_ms;
            imu963_update(elapsed_ms);
            motor_update_target_from_angle(
                imu963_get_angle_x(), imu963_is_ready());
            motor_update_control();
        }

        if (WiFi_IsConnected() &&
            ((uint32_t)(now_ms - last_send_ms) >=
             TELEMETRY_PERIOD_MS)) {
            char telemetry[128];
            int length;

            last_send_ms = now_ms;
            length = snprintf(
                telemetry, sizeof(telemetry),
                "angle:%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u\n",
                (double)imu963_get_angle_z(),
                (double)imu963_get_angle_x(),
                (double)imu963_get_angle_y(),
                (double)motor_get_target_rps(),
                (double)motor_get_actual_rps(),
                (double)motor_get_duty_percent(),
                (unsigned int)(imu963_is_ready() ? 1U : 0U),
                (unsigned int)(motor_is_stall_latched() ? 1U : 0U));

            if ((length > 0) &&
                ((uint32_t)length < sizeof(telemetry))) {
                (void)WiFi_Send(
                    (const uint8_t *)telemetry,
                    (uint32_t)length,
                    now_ms);
            }
        }

        WiFi_Update(now_ms);
    }
}
