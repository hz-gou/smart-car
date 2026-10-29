#include "ti_msp_dl_config.h"
#include "zf_common_headfile.h"

#include <stdbool.h>
#include <stdio.h>

#include "imu963.h"

#define GYRO_CALIBRATION_WARMUP_MS   (500U)
#define GYRO_CALIBRATION_SAMPLES     (512U)
#define GYRO_CALIBRATION_PERIOD_MS   (5U)
#define GYRO_DEADBAND_DPS            (0.03f)
#define GYRO_STATIONARY_RATE_DPS     (0.20f)
#define GYRO_STATIONARY_ACC_SQ_MIN   (0.90f)
#define GYRO_STATIONARY_ACC_SQ_MAX   (1.10f)
#define GYRO_STATIONARY_SAMPLES      (10U)
#define GYRO_BIAS_TRACK_ALPHA        (0.20f)

static bool imu_ready;
static float angle_z_deg;
static float angle_x_deg;
static float angle_y_deg;
static float gyro_bias_x_dps;
static float gyro_bias_y_dps;
static float gyro_bias_z_dps;
static float stationary_sum_x_dps;
static float stationary_sum_y_dps;
static float stationary_sum_z_dps;
static uint16_t stationary_samples;
static bool stationary_locked;

static float imu963_absolute(float value)
{
    return (value < 0.0f) ? -value : value;
}

static bool imu963_init_hardware(void)
{
    DL_SPI_reset(SPI1);
    DL_SPI_enablePower(SPI1);
    delay_cycles(POWER_STARTUP_DELAY);
    return imu963ra_init() == 0U;
}

static void imu963_calibrate_bias(void)
{
    uint16_t index;
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_z = 0.0f;

    printf("\r\nKeep IMU963RA still: calibrating gyro...\r\n");
    system_delay_ms(GYRO_CALIBRATION_WARMUP_MS);

    for (index = 0U; index < GYRO_CALIBRATION_SAMPLES; ++index) {
        imu963ra_get_gyro();
        sum_x += imu963ra_gyro_transition(imu963ra_gyro_x);
        sum_y += imu963ra_gyro_transition(imu963ra_gyro_y);
        sum_z += imu963ra_gyro_transition(imu963ra_gyro_z);
        system_delay_ms(GYRO_CALIBRATION_PERIOD_MS);
    }

    gyro_bias_x_dps = sum_x / (float)GYRO_CALIBRATION_SAMPLES;
    gyro_bias_y_dps = sum_y / (float)GYRO_CALIBRATION_SAMPLES;
    gyro_bias_z_dps = sum_z / (float)GYRO_CALIBRATION_SAMPLES;
    stationary_sum_x_dps = 0.0f;
    stationary_sum_y_dps = 0.0f;
    stationary_sum_z_dps = 0.0f;
    stationary_samples = 0U;
    stationary_locked = false;
    angle_z_deg = 0.0f;
    angle_x_deg = 0.0f;
    angle_y_deg = 0.0f;

    printf("Gyro bias: %.3f, %.3f, %.3f dps\r\n",
           (double)gyro_bias_z_dps,
           (double)gyro_bias_x_dps,
           (double)gyro_bias_y_dps);
}

bool imu963_init(void)
{
    imu_ready = false;
    if (!imu963_init_hardware()) {
        return false;
    }
    imu963_calibrate_bias();
    imu_ready = true;
    return true;
}

void imu963_update(uint32_t elapsed_ms)
{
    float delta_seconds;
    float raw_gyro_x;
    float raw_gyro_y;
    float raw_gyro_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float acc_x;
    float acc_y;
    float acc_z;
    float acc_norm_sq;
    bool stationary_candidate;

    if (!imu_ready || (elapsed_ms == 0U)) {
        return;
    }
    if (elapsed_ms > 100U) {
        elapsed_ms = 100U;
    }

    delta_seconds = (float)elapsed_ms / 1000.0f;
    imu963ra_get_gyro();
    imu963ra_get_acc();

    raw_gyro_x = imu963ra_gyro_transition(imu963ra_gyro_x);
    raw_gyro_y = imu963ra_gyro_transition(imu963ra_gyro_y);
    raw_gyro_z = imu963ra_gyro_transition(imu963ra_gyro_z);
    gyro_x = raw_gyro_x - gyro_bias_x_dps;
    gyro_y = raw_gyro_y - gyro_bias_y_dps;
    gyro_z = raw_gyro_z - gyro_bias_z_dps;

    acc_x = imu963ra_acc_transition(imu963ra_acc_x);
    acc_y = imu963ra_acc_transition(imu963ra_acc_y);
    acc_z = imu963ra_acc_transition(imu963ra_acc_z);
    acc_norm_sq = (acc_x * acc_x) + (acc_y * acc_y) + (acc_z * acc_z);

    stationary_candidate =
        (imu963_absolute(gyro_x) < GYRO_STATIONARY_RATE_DPS) &&
        (imu963_absolute(gyro_y) < GYRO_STATIONARY_RATE_DPS) &&
        (imu963_absolute(gyro_z) < GYRO_STATIONARY_RATE_DPS) &&
        (acc_norm_sq > GYRO_STATIONARY_ACC_SQ_MIN) &&
        (acc_norm_sq < GYRO_STATIONARY_ACC_SQ_MAX);

    if (stationary_candidate) {
        stationary_sum_x_dps += raw_gyro_x;
        stationary_sum_y_dps += raw_gyro_y;
        stationary_sum_z_dps += raw_gyro_z;
        if (stationary_samples < GYRO_STATIONARY_SAMPLES) {
            stationary_samples++;
        }

        if (stationary_samples >= GYRO_STATIONARY_SAMPLES) {
            float average_x =
                stationary_sum_x_dps / (float)stationary_samples;
            float average_y =
                stationary_sum_y_dps / (float)stationary_samples;
            float average_z =
                stationary_sum_z_dps / (float)stationary_samples;

            gyro_bias_x_dps +=
                (average_x - gyro_bias_x_dps) * GYRO_BIAS_TRACK_ALPHA;
            gyro_bias_y_dps +=
                (average_y - gyro_bias_y_dps) * GYRO_BIAS_TRACK_ALPHA;
            gyro_bias_z_dps +=
                (average_z - gyro_bias_z_dps) * GYRO_BIAS_TRACK_ALPHA;
            stationary_sum_x_dps = 0.0f;
            stationary_sum_y_dps = 0.0f;
            stationary_sum_z_dps = 0.0f;
            stationary_samples = 0U;
            gyro_x = raw_gyro_x - gyro_bias_x_dps;
            gyro_y = raw_gyro_y - gyro_bias_y_dps;
            gyro_z = raw_gyro_z - gyro_bias_z_dps;
            stationary_locked = true;
        }
    } else {
        stationary_sum_x_dps = 0.0f;
        stationary_sum_y_dps = 0.0f;
        stationary_sum_z_dps = 0.0f;
        stationary_samples = 0U;
        stationary_locked = false;
    }

    if (stationary_locked) {
        gyro_x = 0.0f;
        gyro_y = 0.0f;
        gyro_z = 0.0f;
    }
    if (imu963_absolute(gyro_x) < GYRO_DEADBAND_DPS) {
        gyro_x = 0.0f;
    }
    if (imu963_absolute(gyro_y) < GYRO_DEADBAND_DPS) {
        gyro_y = 0.0f;
    }
    if (imu963_absolute(gyro_z) < GYRO_DEADBAND_DPS) {
        gyro_z = 0.0f;
    }

    angle_z_deg += gyro_z * delta_seconds;
    angle_x_deg += gyro_x * delta_seconds;
    angle_y_deg += gyro_y * delta_seconds;
}

bool imu963_is_ready(void)
{
    return imu_ready;
}

float imu963_get_angle_z(void)
{
    return angle_z_deg;
}

float imu963_get_angle_x(void)
{
    return angle_x_deg;
}

float imu963_get_angle_y(void)
{
    return angle_y_deg;
}
