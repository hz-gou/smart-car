#include "ti_msp_dl_config.h"
#include "zf_common_headfile.h"

#include "encoder.h"

#define ENCODER_A_PORT     (GPIOB)
#define ENCODER_A_PIN      (DL_GPIO_PIN_10)
#define ENCODER_A_IOMUX    (IOMUX_PINCM27)

static volatile uint32_t encoder_pulses;

void encoder_init(void)
{
    DL_GPIO_initDigitalInputFeatures(
        ENCODER_A_IOMUX,
        DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_setLowerPinsPolarity(
        ENCODER_A_PORT, DL_GPIO_PIN_10_EDGE_RISE);
    DL_GPIO_clearInterruptStatus(ENCODER_A_PORT, ENCODER_A_PIN);
    DL_GPIO_enableInterrupt(ENCODER_A_PORT, ENCODER_A_PIN);
    NVIC_ClearPendingIRQ(GPIOB_INT_IRQn);
    NVIC_EnableIRQ(GPIOB_INT_IRQn);
    encoder_pulses = 0U;
}

void encoder_reset(void)
{
    NVIC_DisableIRQ(GPIOB_INT_IRQn);
    encoder_pulses = 0U;
    DL_GPIO_clearInterruptStatus(ENCODER_A_PORT, ENCODER_A_PIN);
    NVIC_EnableIRQ(GPIOB_INT_IRQn);
}

uint32_t encoder_get_pulses_and_reset(void)
{
    uint32_t pulses;

    NVIC_DisableIRQ(GPIOB_INT_IRQn);
    pulses = encoder_pulses;
    encoder_pulses = 0U;
    NVIC_EnableIRQ(GPIOB_INT_IRQn);
    return pulses;
}

void GROUP1_IRQHandler(void)
{
    if (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1) ==
        DL_INTERRUPT_GROUP1_IIDX_GPIOB) {
        DL_GPIO_clearInterruptStatus(ENCODER_A_PORT, ENCODER_A_PIN);
        encoder_pulses++;
    }
}
