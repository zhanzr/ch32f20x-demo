#include <stdio.h>

#include "board.h"
#include "uart_printf.h"

/* On-board LED1 on the f207-evt-r1 is NOT wired to the MCU on the PCB; for
 * this demo it is connected to PA0 with an external wire. LOW active:
 * GPIO_WriteBit(..., Bit_RESET) = ON. */
#define LED1_PORT GPIOA
#define LED1_PIN  GPIO_Pin_0

/* Internal ADC signal sources (the CH32F20x ADC has 16 external + 2 internal
 * channels). Both are wired through the TSVREFE bit in ADC_CTLR2. */
#define ADC_CH_TEMP    ADC_Channel_TempSensor    /* ADC1_IN16: die temperature */
#define ADC_CH_VREFINT ADC_Channel_Vrefint       /* ADC1_IN17: internal 1.20 V ref */

/* CH32F207 datasheet (CH32F207DS0, 4.3.22 Temperature sensor characteristics):
 *   V25        = 1.40 V typ @ 25 C   (1.34 .. 1.46 V)  <- per-chip scatter!
 *   Avg_Slope  = 4.3 mV/C typ        (3.8 .. 4.8 mV/C)
 *   TS_temp    = 17.1 us sample time @ fADC = 14 MHz  (239.5 cycles)
 * Internal reference: VREFINT = 1.20 V typ.
 *
 * The *typical* V25 alone would put the reading off by up to ~14 C, so the
 * per-chip factory calibration stored in the info ROM is used instead (the
 * same word the SPL helper TempSensor_Volt_To_Temper() reads):
 *   0x1FFFF720: bits[15:0]  = Refer_Volt   (temp-sensor voltage in mV)
 *               bits[31:16] = Refer_Temper (factory reference temperature in C)
 *   T(C) = Refer_Temper - (VSENSE_mV - Refer_Volt) / 4.3 */
#define TEMP_CAL_ADDR     0x1FFFF720u
#define TEMP_AVG_SLOPE    4.3f
#define VREFINT_TYP_V     1.20f

static uint16_t s_ReferVolt;      /* mV @ factory reference temperature */
static uint16_t s_ReferTemper;    /* degC */

static void TEMP_Calibration_Read(void)
{
    uint32_t cal = *(volatile uint32_t *)TEMP_CAL_ADDR;
    s_ReferVolt   = (uint16_t)(cal & 0xFFFFu);
    s_ReferTemper = (uint16_t)((cal >> 16) & 0xFFFFu);
}

static int16_t  s_CalibrationValue = 0;
static volatile uint32_t s_AdcRunning = 0;

/* ------------------------------------------------------------------------ */
static void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = LED1_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED1_PORT, &GPIO_InitStructure);

    GPIO_WriteBit(LED1_PORT, LED1_PIN, Bit_SET);   /* OFF (active low) */
}

static void LED_Toggle(void)
{
    if (GPIO_ReadOutputDataBit(LED1_PORT, LED1_PIN) == Bit_SET)
    {
        GPIO_WriteBit(LED1_PORT, LED1_PIN, Bit_RESET);   /* ON */
    }
    else
    {
        GPIO_WriteBit(LED1_PORT, LED1_PIN, Bit_SET);     /* OFF */
    }
}

/* ------------------------------------------------------------------------ */
/* ADC1: single conversion, software trigger, right aligned, temperature
 * sensor + VREFINT channels enabled (TSVREFE). The ADC clock is PCLK2/8;
 * at the 144 MHz system clock that is 18 MHz (the datasheet max is 14 MHz —
 * the prescaler only goes to /8 on CH32F20x, WCH's own EVT examples run
 * 12 MHz at 96 MHz). */
static void ADC1_Init(void)
{
    ADC_InitTypeDef ADC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);

    /* Run the factory-style calibration to get the offset correction value
     * (calibration must run with the ADC input buffer disabled). */
    ADC_BufferCmd(ADC1, DISABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1))
    {
    }
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1))
    {
    }
    s_CalibrationValue = Get_CalibrationValue(ADC1);
    ADC_BufferCmd(ADC1, ENABLE);

    /* Wake the internal channels: temperature sensor + VREFINT (TSVREFE). */
    ADC_TempSensorVrefintCmd(ENABLE);
}

/* Single conversion on the given channel. Sample time 239.5 cycles
 * (~17.1 us at 14 MHz, the recommended temperature-sensor setting). */
static uint16_t ADC_ReadChannel(uint8_t channel)
{
    uint16_t val;

    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_239Cycles5);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC))
    {
    }

    val = (uint16_t)ADC_GetConversionValue(ADC1);
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    return val;
}

static uint16_t ADC_ReadAveraged(uint8_t channel, uint8_t times)
{
    uint32_t sum = 0;
    uint8_t  t;

    for (t = 0; t < times; t++)
    {
        sum += ADC_ReadChannel(channel);
    }
    return (uint16_t)(sum / times);
}

/* Apply the calibration offset (WCH's Get_ConversionVal). */
static int32_t ADC_Calibrated(int32_t val)
{
    int32_t v = val + s_CalibrationValue;
    if (val == 0 || v < 0)
    {
        return 0;
    }
    if (val == 4095 || v > 4095)
    {
        return 4095;
    }
    return v;
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    uint32_t ticks = 0;

    Board_Init();          /* SystemCoreClock update + USART1 console + 1 ms tick */
    LED_Init();
    ADC1_Init();
    TEMP_Calibration_Read();

    printf("\r\n=== blink_hello on CH32F207VCT6 @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);
    printf("ADC internal channels: temperature (IN16) + VREFINT (IN17)\r\n");
    printf("Calibration value: %d\r\n", (int)s_CalibrationValue);
    printf("Temp sensor cal: Refer_Volt=%u mV @ Refer_Temper=%u C\r\n",
           (unsigned)s_ReferVolt, (unsigned)s_ReferTemper);

    while (1)
    {
        /* Average the two internal channels (10 samples each, ~17 us each). */
        int32_t r_temp    = ADC_Calibrated(ADC_ReadAveraged(ADC_CH_TEMP, 10));
        int32_t r_vrefint = ADC_Calibrated(ADC_ReadAveraged(ADC_CH_VREFINT, 10));

        /* VDDA back-calculated from the internal reference (VREFINT = 1.20 V typ). */
        float vdda_v = (r_vrefint != 0) ? (VREFINT_TYP_V * 4096.0f) / (float)r_vrefint
                                        : 0.0f;

        /* Temperature-sensor voltage in mV (VSENSE scales with VDDA), then the
         * factory-calibrated formula (same as SPL TempSensor_Volt_To_Temper):
         *   T (C) = Refer_Temper - (VSENSE_mV - Refer_Volt) / 4.3. */
        float v_sense_mv = (float)r_temp * (vdda_v * 1000.0f) / 4096.0f;
        float temp_c     = (float)s_ReferTemper
                           - ((float)v_sense_mv - (float)s_ReferVolt) / TEMP_AVG_SLOPE;

        LED_Toggle();
        printf("[%lu] LED1 %s | T=%5.1f C  VDDA=%.3f V  (IN16 raw %ld, VREFINT raw %ld)\r\n",
               (unsigned long)++ticks,
               (GPIO_ReadOutputDataBit(LED1_PORT, LED1_PIN) == Bit_RESET) ? "ON " : "OFF",
               temp_c, vdda_v, (long)r_temp, (long)r_vrefint);

        Delay_Ms(500);
    }

    return 0;
}
