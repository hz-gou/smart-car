#include "ti_msp_dl_config.h"
#include "zf_common_headfile.h"

#include <stdbool.h>
#include <stdio.h>

/*
 * MSPM0G35XX motherboard wiring used by this project:
 *
 * SPI Wi-Fi module
 *   SCK  : PA12
 *   MOSI : PA9
 *   MISO : PA13
 *   CS   : PA7
 *   RST  : PA8
 *   INT  : PA15
 *
 * IMU963RA attitude sensor socket
 *   SCK  : PB23
 *   MOSI : PB22
 *   MISO : PB21
 *   CS   : PB19
 */

#define WIFI_SSID                    "YOUR_WIFI_SSID"
#define WIFI_PASSWORD                "YOUR_WIFI_PASSWORD"
#define HOST_IP                      "192.168.1.100"
#define HOST_PORT                    "8086"
#define LOCAL_PORT                   "6666"

#define SYSTEM_TICK_MS               (1U)
#define TELEMETRY_PERIOD_MS          (20U)
#define WIFI_RETRY_PERIOD_MS         (2000U)
#define IMU_RETRY_PERIOD_MS          (2000U)
#define WIFI_SEND_FAILURE_LIMIT      (3U)

static volatile uint32_t g_ms_tick;
static volatile bool g_wifi_connected;
static uint16_t g_wifi_send_failures;
static bool g_imu_ready;
static float g_angle_z_deg;
static float g_angle_x_deg;
static float g_angle_y_deg;

static bool WiFi_ConnectSocket(void)
{
    return wifi_spi_socket_connect(
               "TCP", HOST_IP, HOST_PORT, LOCAL_PORT) == 0U;
}

static void WiFi_ConnectAll(void)
{
    while (wifi_spi_init(WIFI_SSID, WIFI_PASSWORD) != 0U) {
        printf("\r\nWiFi connection failed, retrying...\r\n");
        system_delay_ms(500);
    }
    printf("\r\nWiFi connected: %s\r\n", wifi_spi_ip_addr_port);

    while (!WiFi_ConnectSocket()) {
        printf("\r\nTCP connect failed, retrying...\r\n");
        system_delay_ms(500);
    }

    g_wifi_send_failures = 0U;
    g_wifi_connected = true;
    printf("\r\nTCP connected: %s:%s\r\n", HOST_IP, HOST_PORT);
}

static void WiFi_Init(void)
{
    debug_init();

    /* The Wi-Fi driver owns SPI0 and its software chip-select pin. */
    DL_SPI_reset(SPI0);
    DL_SPI_enablePower(SPI0);
    delay_cycles(POWER_STARTUP_DELAY);

    WiFi_ConnectAll();
}

static bool IMU963RA_InitHardware(void)
{
    /* The SeekFree driver configures the pins and transfer format, while the
     * project enables the SPI1 peripheral clock before the first transfer. */
    DL_SPI_reset(SPI1);
    DL_SPI_enablePower(SPI1);
    delay_cycles(POWER_STARTUP_DELAY);

    return imu963ra_init() == 0U;
}

static void UpdateAngles(uint32_t now_ms, uint32_t *last_update_ms)
{
    uint32_t elapsed_ms;
    float delta_seconds;
    float gyro_x;
    float gyro_y;
    float gyro_z;

    elapsed_ms = (uint32_t)(now_ms - *last_update_ms);
    *last_update_ms = now_ms;

    if (!g_imu_ready || (elapsed_ms == 0U)) {
        return;
    }

    delta_seconds = (float)elapsed_ms / 1000.0f;
    imu963ra_get_gyro();

    gyro_x = imu963ra_gyro_transition(imu963ra_gyro_x);
    gyro_y = imu963ra_gyro_transition(imu963ra_gyro_y);
    gyro_z = imu963ra_gyro_transition(imu963ra_gyro_z);

    /* Gyro-only integration gives angles relative to the startup position. */
    g_angle_z_deg += gyro_z * delta_seconds;
    g_angle_x_deg += gyro_x * delta_seconds;
    g_angle_y_deg += gyro_y * delta_seconds;
}

static void SendTelemetry(void)
{
    char telemetry[80];
    int length;

    length = snprintf(
        telemetry, sizeof(telemetry),
        "angle:%.3f,%.3f,%.3f,%u\n",
        (double)g_angle_z_deg,
        (double)g_angle_x_deg,
        (double)g_angle_y_deg,
        (unsigned int)(g_imu_ready ? 1U : 0U));

    if ((length > 0) &&
        (wifi_spi_send_buffer(
             (const uint8 *)telemetry, (uint32_t)length) != 0U)) {
        if (g_wifi_send_failures < UINT16_MAX) {
            g_wifi_send_failures++;
        }
        if (g_wifi_send_failures >= WIFI_SEND_FAILURE_LIMIT) {
            g_wifi_connected = false;
            g_wifi_send_failures = 0U;
            printf("\r\nWiFi link lost, reconnecting...\r\n");
        }
    } else {
        g_wifi_send_failures = 0U;
    }
}

void SysTick_Handler(void)
{
    g_ms_tick += SYSTEM_TICK_MS;
}

int main(void)
{
    uint32_t last_send_ms = 0U;
    uint32_t last_wifi_retry_ms = 0U;
    uint32_t last_imu_retry_ms = 0U;
    uint32_t last_angle_update_ms = 0U;

    SYSCFG_DL_init();
    WiFi_Init();

    g_imu_ready = IMU963RA_InitHardware();
    printf("\r\nIMU963RA init: %s\r\n", g_imu_ready ? "OK" : "FAILED");

    DL_SYSTICK_config(
        (uint32_t)((double)CPUCLK_FREQ *
                   ((double)SYSTEM_TICK_MS / 1000.0)));

    while (1) {
        uint32_t now_ms = g_ms_tick;

        if (!g_imu_ready &&
            ((uint32_t)(now_ms - last_imu_retry_ms) >=
             IMU_RETRY_PERIOD_MS)) {
            last_imu_retry_ms = now_ms;
            g_imu_ready = IMU963RA_InitHardware();
            printf("\r\nIMU963RA retry: %s\r\n",
                   g_imu_ready ? "OK" : "FAILED");
        }

        if ((uint32_t)(now_ms - last_angle_update_ms) >=
            TELEMETRY_PERIOD_MS) {
            UpdateAngles(now_ms, &last_angle_update_ms);
        }

        if (g_wifi_connected &&
            ((uint32_t)(now_ms - last_send_ms) >=
             TELEMETRY_PERIOD_MS)) {
            last_send_ms = now_ms;
            SendTelemetry();
        }

        if (!g_wifi_connected &&
            ((uint32_t)(now_ms - last_wifi_retry_ms) >=
             WIFI_RETRY_PERIOD_MS)) {
            last_wifi_retry_ms = now_ms;
            WiFi_ConnectAll();
        }
    }
}
