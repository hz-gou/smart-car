#include "ti_msp_dl_config.h"
#include "zf_common_headfile.h"

#include <stdbool.h>
#include <stdio.h>

#include "wifi.h"

#define WIFI_SSID                 "YOUR_WIFI_SSID"
#define WIFI_PASSWORD             "YOUR_WIFI_PASSWORD"
#define HOST_IP                   "192.168.1.100"
#define HOST_PORT                 "8086"
#define LOCAL_PORT                "6666"
#define WIFI_RETRY_PERIOD_MS      (2000U)
#define WIFI_SEND_FAILURE_LIMIT   (3U)

static bool wifi_connected;
static uint16_t wifi_send_failures;
static uint32_t wifi_last_retry_ms;

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

    wifi_send_failures = 0U;
    wifi_connected = true;
    printf("\r\nTCP connected: %s:%s\r\n", HOST_IP, HOST_PORT);
}

void WiFi_Init(void)
{
    debug_init();
    DL_SPI_reset(SPI0);
    DL_SPI_enablePower(SPI0);
    delay_cycles(POWER_STARTUP_DELAY);
    wifi_last_retry_ms = 0U;
    WiFi_ConnectAll();
}

bool WiFi_IsConnected(void)
{
    return wifi_connected;
}

bool WiFi_Send(const uint8_t *data, uint32_t length, uint32_t now_ms)
{
    if (!wifi_connected) {
        return false;
    }

    if (wifi_spi_send_buffer(data, length) == 0U) {
        wifi_send_failures = 0U;
        return true;
    }

    if (wifi_send_failures < UINT16_MAX) {
        wifi_send_failures++;
    }
    if (wifi_send_failures >= WIFI_SEND_FAILURE_LIMIT) {
        wifi_connected = false;
        wifi_send_failures = 0U;
        wifi_last_retry_ms = now_ms;
        printf("\r\nWiFi link lost, reconnecting...\r\n");
    }
    return false;
}

void WiFi_Update(uint32_t now_ms)
{
    if (wifi_connected) {
        return;
    }
    if ((uint32_t)(now_ms - wifi_last_retry_ms) >=
        WIFI_RETRY_PERIOD_MS) {
        wifi_last_retry_ms = now_ms;
        WiFi_ConnectAll();
    }
}
