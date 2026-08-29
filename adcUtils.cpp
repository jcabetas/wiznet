/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.h"
#include "hal.h"
#include "modbus.h"
#include "externRegistros.h"
#include "miniPozo.h"


#define ADC_GRP1_NUM_CHANNELS   1
#define ADC_GRP1_BUF_DEPTH      4

thread_t *procesoADC = NULL;
static adcsample_t samples1[ADC_GRP1_NUM_CHANNELS * ADC_GRP1_BUF_DEPTH];


extern "C" {
    void initAdcC(void);
}

static void adcerrorcallback(ADCDriver *adcp, adcerror_t err) {

  (void)adcp;
  (void)err;
}

/*
 * ADC conversion group.
 * Mode:        Linear buffer, 8 samples of 1 channel, SW triggered.
 * Channels:    IN1.
 */
static const ADCConversionGroup adcgrpcfg1 = {
  FALSE,
  ADC_GRP1_NUM_CHANNELS,
  NULL,
  adcerrorcallback,
  0,                        /* CR1 */
  ADC_CR2_SWSTART,          /* CR2 */
  0,                        /* SMPR1 */
  ADC_SMPR2_SMP_AN1(ADC_SAMPLE_480),                        /* SMPR2 */
  0,                        /* HTR */
  0,                        /* LTR */
  0,                        /* SQR1 */
  0,                        /* SQR2 */
  ADC_SQR3_SQ1_N(ADC_CHANNEL_IN1)
};


/*
 * Red LED blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waAdc, 128);
static THD_FUNCTION(ThreadAdc, arg) {
  (void)arg;
  float presionADC;
  chRegSetThreadName("ADC");
  while (true) {
      if (chThdShouldTerminateX())
          chThdExit((msg_t) 1);
      adcConvert(&ADCD1, &adcgrpcfg1, samples1, ADC_GRP1_BUF_DEPTH);
      chThdSleepMilliseconds(100);
      presionADC = samples1[0]*barMaxSensPresionHR->getValor()*0.00002442f;
      presBarIR->setValor((uint16_t) (presionADC*100.0f));
      chThdSleepMilliseconds(400);
  }
}

/*
 * Application entry point.
 */
void initAdcC(void) {

  palSetLineMode(LINE_420MA,PAL_MODE_INPUT_ANALOG);
  adcStart(&ADCD1, NULL);
  /*
   * Creates the adc thread.
   */
  if (!procesoADC)
      procesoADC = chThdCreateStatic(waAdc, sizeof(waAdc), NORMALPRIO, ThreadAdc, NULL);
}
