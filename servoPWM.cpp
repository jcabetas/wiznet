/*
 * pwmInts.c
 *
 *  Created on: 03/06/2012, adaptado 15/11/2015 a Flip32,
 *  adaptado y muy simplificado para BlackPill 20/9/2022
 *      Author: joaquin
 *  Salida servo 1  (PWM1, PB8, TIM4_CH3, AF02)
 */

#include "hal.h"
#include "chprintf.h"
#include "sms.h"

extern "C" {
    void abrePuertaC(void);
    void cierraPuertaC(void);
    void initServo(void);
}

uint8_t puertaCerrada = 0;

static PWMConfig pwmcfg = {
  3000000,  //antes 3125000, /* 48 MHz PWM clock frequency */
    60000, // antes 62500,   /* PWM period 20 millisecond */
  NULL,  /* No callback */
  /* Channel 4 enabled */
  {
    {PWM_OUTPUT_DISABLED, NULL},
    {PWM_OUTPUT_DISABLED, NULL},
    {PWM_OUTPUT_DISABLED, NULL},
    {PWM_OUTPUT_ACTIVE_HIGH, NULL},
  },
  0,
  0,
  0
};

void initServo(void)
{
/*
 *   Salida servo: TIM4CH4 (PB9)
 */
    palSetPadMode(GPIOB, GPIOB_PWMSERVO,PAL_MODE_ALTERNATE(2) | PAL_STM32_OSPEED_HIGHEST);
    palSetLineMode(LINE_ONSERVO, PAL_MODE_OUTPUT_PUSHPULL);
    palSetLine(LINE_ONSERVO);
	pwmStart(&PWMD4, &pwmcfg);
}

void closeServo(void)
{
    palSetPadMode(GPIOB, GPIOB_PWMSERVO,PAL_MODE_INPUT_ANALOG );
    palClearLine(LINE_ONSERVO);
    pwmStop(&PWMD4);
}

void mueveServoAncho(uint16_t ancho, uint16_t ms)
{
  // minimo 3000, maximo 6000, medio 4500
  // lo dejo entre 2600 y 6400
  if (ancho<2700) ancho=2600;
  if (ancho>6300) ancho=6400;
  pwmEnableChannel(&PWMD4, 3, ancho);
  chThdSleepMilliseconds(ms);
}

void mueveServoPos(uint16_t porcPosicion)
{
  uint16_t ancho;
  // arranco servo
  initServo();
  palSetLine(LINE_ONSERVO);
  if (PWMD4.state==PWM_STOP)
      pwmStart(&PWMD4, &pwmcfg);
  if (porcPosicion>100) porcPosicion=100;
  // si esta en la posicion, recordar durante 1500ms
  ancho = (uint16_t) (2700.0f+3800.0f*porcPosicion/100.0f);
  mueveServoAncho(ancho, 1500);
  closeServo();
}

void mueveServoPosC(uint16_t porcPosicion)
{
    mueveServoPos(porcPosicion);
}

void cierraPuertaC(void)
{
    // si no hay bateria suficiente, no cierro
//    if (sms::porcBateriaSIM800L < 30)
//        return;
    mueveServoPos(0);
    puertaCerrada = 1;
}


void cierraPuerta(void)
{
    cierraPuertaC();
}

void abrePuertaC(void)
{
    mueveServoPos(1000);
    puertaCerrada = 0;
}

void abrePuerta(void)
{
    abrePuertaC();
}
