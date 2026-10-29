#include "ti_msp_dl_config.h"
#include "zf_common_headfile.h"

#include <stdbool.h>
#include <stdio.h>

#include "encoder.h"
#include "motor.h"
#include "pid.h"

#define MOTOR_PWM_PERIOD              (4000U)
#define MOTOR_PWM_CC_INDEX            (DL_TIMER_CC_1_INDEX)
#define MOTOR_MAX_DUTY_PERCENT        (70.0f)
#define MOTOR_PWM_INST                (TIMA0)
#define MOTOR_DIR_PORT                (GPIOB)
#define MOTOR_DIR_PIN                 (DL_GPIO_PIN_13)
#define MOTOR_DIR_REVERSE_LEVEL       (0U)
#define MOTOR_DIR_IDLE_LEVEL          (1U)

#define ENCODER_PPR                   (1024.0f)
#define ENCODER_REVS_PER_MOTOR_REV    (3.0f)
#define CONTROL_PERIOD_MS             (20U)
#define TARGET_RPS_MAX                (60.0f)

#define ANGLE_X_LOW_DEG               (-7.0f)
#define ANGLE_X_HIGH_DEG              (6.0f)
#define TARGET_RPS_LOW_ANGLE          (13.0f)
#define TARGET_RPS_HIGH_ANGLE         (7.0f)
#define TARGET_RPS_MIDDLE             (10.0f)

#define PID_KP_DEFAULT                (3.0f)
#define PID_KI_DEFAULT                (10.0f)
#define PID_KD_DEFAULT                (0.0f)
#define PID_INTEGRAL_LIMIT            (50.0f)
#define DUTY_RAMP_UP_PERCENT          (5.0f)
#define DUTY_RAMP_DOWN_PERCENT        (10.0f)
#define STALL_DUTY_PERCENT            (30.0f)
#define STALL_SPEED_RPS               (0.30f)
#define STALL_TRIP_MS                 (600U)

static pid_controller_struct motor_pid;
static float target_rps;
static float actual_rps;
static float duty_percent;
static float duty_output;
static uint16_t stall_ms;
static uint8_t speed_enabled;
static uint8_t direction_level;
static bool stall_latched;

static float motor_clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static float motor_absolute(float value)
{
    return (value < 0.0f) ? -value : value;
}

static void motor_apply_duty(float signed_duty_percent)
{
    uint8_t requested_level;
    float magnitude;
    uint32_t compare;

    magnitude = motor_absolute(signed_duty_percent);
    magnitude = motor_clamp(
        magnitude, 0.0f, MOTOR_MAX_DUTY_PERCENT);
    requested_level = (magnitude > 0.0f)
                          ? MOTOR_DIR_REVERSE_LEVEL
                          : MOTOR_DIR_IDLE_LEVEL;

    if (requested_level != direction_level) {
        DL_TimerA_setCaptureCompareValue(
            MOTOR_PWM_INST, 0U, MOTOR_PWM_CC_INDEX);
        delay_cycles(640U);
        if (requested_level != 0U) {
            DL_GPIO_setPins(MOTOR_DIR_PORT, MOTOR_DIR_PIN);
        } else {
            DL_GPIO_clearPins(MOTOR_DIR_PORT, MOTOR_DIR_PIN);
        }
        direction_level = requested_level;
    }

    compare = (uint32_t)(
        (magnitude * (float)MOTOR_PWM_PERIOD) / 100.0f);
    if (compare > MOTOR_PWM_PERIOD) {
        compare = MOTOR_PWM_PERIOD;
    }
    DL_TimerA_setCaptureCompareValue(
        MOTOR_PWM_INST, compare, MOTOR_PWM_CC_INDEX);
    duty_percent =
        (signed_duty_percent < 0.0f) ? -magnitude : magnitude;
}

static void motor_set_target_rps(float new_target_rps)
{
    target_rps = motor_clamp(new_target_rps, 0.0f, TARGET_RPS_MAX);
    stall_ms = 0U;
    speed_enabled = (target_rps > 0.0f) ? 1U : 0U;
    if (speed_enabled == 0U) {
        motor_apply_duty(0.0f);
    }
}

void motor_init(void)
{
    static const DL_TimerA_ClockConfig motor_clock_config = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
        .prescale = 0U
    };
    static const DL_TimerA_PWMConfig motor_pwm_config = {
        .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
        .period = MOTOR_PWM_PERIOD,
        .isTimerWithFourCC = true,
        .startTimer = DL_TIMER_STOP
    };

    DL_TimerA_reset(MOTOR_PWM_INST);
    DL_TimerA_enablePower(MOTOR_PWM_INST);
    DL_GPIO_initPeripheralOutputFunction(
        IOMUX_PINCM29, IOMUX_PINCM29_PF_TIMA0_CCP1);
    DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_12);
    DL_TimerA_setClockConfig(
        MOTOR_PWM_INST, (DL_TimerA_ClockConfig *)&motor_clock_config);
    DL_TimerA_initPWMMode(
        MOTOR_PWM_INST, (DL_TimerA_PWMConfig *)&motor_pwm_config);
    DL_TimerA_setCounterControl(
        MOTOR_PWM_INST,
        DL_TIMER_CZC_CCCTL1_ZCOND,
        DL_TIMER_CAC_CCCTL1_ACOND,
        DL_TIMER_CLC_CCCTL1_LCOND);
    DL_TimerA_setCaptureCompareOutCtl(
        MOTOR_PWM_INST,
        DL_TIMER_CC_OCTL_INIT_VAL_LOW,
        DL_TIMER_CC_OCTL_INV_OUT_ENABLED,
        DL_TIMER_CC_OCTL_SRC_FUNCVAL,
        DL_TIMERA_CAPTURE_COMPARE_1_INDEX);
    DL_TimerA_setCaptCompUpdateMethod(
        MOTOR_PWM_INST,
        DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE,
        DL_TIMERA_CAPTURE_COMPARE_1_INDEX);
    DL_TimerA_setCaptureCompareValue(
        MOTOR_PWM_INST, 0U, MOTOR_PWM_CC_INDEX);
    DL_TimerA_enableClock(MOTOR_PWM_INST);
    DL_TimerA_setCCPDirection(MOTOR_PWM_INST, DL_TIMER_CC1_OUTPUT);

    DL_GPIO_initDigitalOutput(IOMUX_PINCM30);
    DL_GPIO_setPins(MOTOR_DIR_PORT, MOTOR_DIR_PIN);
    DL_GPIO_enableOutput(MOTOR_DIR_PORT, MOTOR_DIR_PIN);

    pid_init(
        &motor_pid,
        PID_KP_DEFAULT,
        PID_KI_DEFAULT,
        PID_KD_DEFAULT,
        PID_INTEGRAL_LIMIT);
    target_rps = 0.0f;
    actual_rps = 0.0f;
    duty_percent = 0.0f;
    duty_output = 0.0f;
    stall_ms = 0U;
    speed_enabled = 0U;
    direction_level = MOTOR_DIR_IDLE_LEVEL;
    stall_latched = false;
    motor_apply_duty(0.0f);
    DL_TimerA_startCounter(MOTOR_PWM_INST);
}

void motor_stop(void)
{
    speed_enabled = 0U;
    target_rps = 0.0f;
    duty_output = 0.0f;
    stall_ms = 0U;
    pid_reset(&motor_pid, actual_rps);
    motor_apply_duty(0.0f);
}

void motor_update_target_from_angle(float angle_x_deg, bool imu_ready)
{
    float new_target_rps;

    if (!imu_ready || stall_latched) {
        if (speed_enabled != 0U) {
            motor_stop();
        }
        return;
    }

    if (angle_x_deg < ANGLE_X_LOW_DEG) {
        new_target_rps = TARGET_RPS_LOW_ANGLE;
    } else if (angle_x_deg > ANGLE_X_HIGH_DEG) {
        new_target_rps = TARGET_RPS_HIGH_ANGLE;
    } else {
        new_target_rps = TARGET_RPS_MIDDLE;
    }

    if (new_target_rps != target_rps) {
        motor_set_target_rps(new_target_rps);
    }
}

void motor_update_control(void)
{
    uint32_t pulses;
    float error;
    float output;
    float step_limit;

    pulses = encoder_get_pulses_and_reset();
    actual_rps = (float)pulses * 1000.0f /
                 (ENCODER_PPR *
                  ENCODER_REVS_PER_MOTOR_REV *
                  (float)CONTROL_PERIOD_MS);

    if (speed_enabled == 0U) {
        motor_apply_duty(0.0f);
        return;
    }

    error = target_rps - actual_rps;
    output = pid_calculate(
        &motor_pid,
        error,
        actual_rps,
        (float)CONTROL_PERIOD_MS / 1000.0f);

    step_limit = (output > duty_output)
                     ? DUTY_RAMP_UP_PERCENT
                     : DUTY_RAMP_DOWN_PERCENT;
    if (output > (duty_output + step_limit)) {
        output = duty_output + step_limit;
    } else if (output < (duty_output - step_limit)) {
        output = duty_output - step_limit;
    }
    output = motor_clamp(output, 0.0f, MOTOR_MAX_DUTY_PERCENT);

    if ((output > 0.0f) &&
        (output < MOTOR_MAX_DUTY_PERCENT)) {
        pid_update_integral(
            &motor_pid,
            error,
            (float)CONTROL_PERIOD_MS / 1000.0f);
    }

    duty_output = output;
    motor_apply_duty(output);

    if ((output >= STALL_DUTY_PERCENT) &&
        (actual_rps < STALL_SPEED_RPS)) {
        if (stall_ms < STALL_TRIP_MS) {
            stall_ms += CONTROL_PERIOD_MS;
        }
        if (stall_ms >= STALL_TRIP_MS) {
            stall_latched = true;
            motor_stop();
            printf("\r\nMotor stall detected, output stopped.\r\n");
        }
    } else {
        stall_ms = 0U;
    }
}

float motor_get_target_rps(void)
{
    return target_rps;
}

float motor_get_actual_rps(void)
{
    return actual_rps;
}

float motor_get_duty_percent(void)
{
    return duty_percent;
}

bool motor_is_stall_latched(void)
{
    return stall_latched;
}
