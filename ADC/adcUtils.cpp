#include "ch.hpp"
#include "hal.h"
#include "string.h"
#include "chprintf.h"


using namespace chibios_rt;

extern "C"
{
    void pruebaADC(void);
    void initAdc(void);
    float hallaCapBatC(void);
}

//                       0%      5%    10%    15%    20%    25%    30%    35%   40%     45%    50%
float lipoVoltCharge[]={3.3f,  3.50f, 3.69f, 3.71f, 3.73f, 3.75f, 3.77f, 3.79f, 3.8f, 3.82f, 3.84f,\
                        3.85f, 3.87f, 3.91f, 3.95f, 3.98f, 4.02f, 4.08f, 4.11f, 4.15f, 4.20f };
//                       55%    60%    65%    70%    75%    80%    85%    90%    95%    100%

/*
 * La tension se lee en PA7 (ADC7), a traves de divisor con factor 0,69894
 *   El fondo de escala es 4096 corresponde a 3.3 V
 *   Por tanto la tension es ADC*3.314/(0,69894*4096) = ADC*0,001157584
 */

#define ADC_GRP1_NUM_CHANNELS   1
#define ADC_GRP1_BUF_DEPTH      4

// Create buffer to store ADC results. This is
// one-dimensional interleaved array

adcsample_t samples_buf[ADC_GRP1_NUM_CHANNELS * ADC_GRP1_BUF_DEPTH]; // results array

static void adcerrorcallback(ADCDriver *adcp, adcerror_t err) {

  (void)adcp;
  (void)err;
}

/*
 * ADC conversion group.
 * Mode:        Linear buffer, 4 samples of 1 channel, SW triggered.
 * Channels:    B1 == AN9.
 */
static const ADCConversionGroup adcgrpcfg1 = {
  FALSE,
  ADC_GRP1_NUM_CHANNELS,
  NULL,
  adcerrorcallback,
  0,                        /* CR1 */
  ADC_CR2_SWSTART,          /* CR2 */
  0,                        /* SMPR1 */
  ADC_SMPR2_SMP_AN9(ADC_SAMPLE_480), /* SMPR2 */
  0,                        /* HTR */
  0,                        /* LTR */
  0,                        /* SQR1 */
  0,  /* SQR2 */
  ADC_SQR3_SQ1_N(ADC_CHANNEL_IN8)
};




/* Lee tension */
void leeTension(float *vBat)
{
    adcStart(&ADCD1,NULL);
	adcConvert(&ADCD1, &adcgrpcfg1, &samples_buf[0], ADC_GRP1_BUF_DEPTH);
    adcStop(&ADCD1);
	// promedia
	adcsample_t media = 0;
	for (uint8_t n=0;n<ADC_GRP1_BUF_DEPTH;n++)
	    media += samples_buf[n];
	media /= ADC_GRP1_BUF_DEPTH;
	*vBat = media*0.001157584f;
}

float hallaCapBat(float *vBat)
{
    if (*vBat<=lipoVoltCharge[0])
        return 0.0f;
    if (*vBat>=lipoVoltCharge[20])
        return 100.0f;
    // miro en que rango se encuentra
    for (uint8_t n=1;n<sizeof(lipoVoltCharge);n++)
        if (*vBat<=lipoVoltCharge[n])
        {
            // se encuentra entre n-1 y n. Los escalones de carga son del 5%
            float result = (n-1)*5.0f+5.0f*(*vBat - lipoVoltCharge[n-1])/(lipoVoltCharge[n]-lipoVoltCharge[n-1]);
            return result;
        }
    return 999.9f; //
}

float hallaCapBatC(void)
{
    float vBat;
    leeTension(&vBat);
    return hallaCapBat(&vBat);
}

void pruebaADC(void)
{
    char buff[30];
    float vBat;
    leeTension(&vBat);
    float porcBat = hallaCapBat(&vBat);
    chsnprintf(buff,sizeof(buff),"Vbat:%.3fV (%d%%)\r\n",vBat,(int16_t) porcBat);
}

void initAdc(void)
{
    palSetLineMode(LINE_VBAT, PAL_MODE_INPUT_ANALOG); // medida de bateria
}

