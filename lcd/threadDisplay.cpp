/**
 * @file stm32/LCD/threadLCD.c
 * @brief proceso LCD
 * @addtogroup LCD
 * @{
 */

#include "ch.hpp"
#include "hal.h"

extern "C" {
  void initDisplay(void);
}
#include "string.h"
#include "chprintf.h"
#include "lcd.h"
#include "colas.h"
#include "../miniPozo.h"

thread_t *procesoLCD = NULL;
event_source_t updateLCD_source;
extern struct queu_t colaMsgLcd;

static THD_WORKING_AREA(waLCD, 1024);

static THD_FUNCTION(threadLCD, arg) {
    (void) arg;
    uint8_t exito;
    eventmask_t evt;
    event_listener_t lcdUpdate_listener;
    struct msgLcd_t ptrMsgLcd;
    chRegSetThreadName("Display");

    chEvtRegisterMask(&updateLCD_source, &lcdUpdate_listener,EVENT_MASK(0));

    while (true)
    {
        evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(100));
        if (chThdShouldTerminateX())
        {
            chEvtUnregister(&updateLCD_source, &lcdUpdate_listener);
            chThdExit((msg_t) 1);
        }
        if (evt & EVENT_MASK(0)) // Evento update LCD
        {
            do
            {
            exito = getQueu(&colaMsgLcd, &ptrMsgLcd);
            if (exito)
#ifdef LCD
                chLcdprintfFila(ptrMsgLcd.fila,"%s",ptrMsgLcd.msg);
#endif
#ifdef SSD1306
            printfFilaSSD1306(ptrMsgLcd.fila,ptrMsgLcd.msg);
#endif
            } while (exito);
        }
    }
}

//static const I2CConfig i2ccfg = {
//  OPMODE_I2C,
//  400000,
//  FAST_DUTY_CYCLE_2, //STD_DUTY_CYCLE
//};
static const I2CConfig i2ccfg = {
  OPMODE_I2C,
  400000,
  FAST_DUTY_CYCLE_2,
};
void initI2C(void)
{
    palSetLineMode(LINE_SDA2,PAL_MODE_ALTERNATE(9) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    palSetLineMode(LINE_SCL2,PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    chThdSleepMilliseconds(50); // espera a que se inicie LCD
    i2cStart(&LCD_I2C, &i2ccfg); // LCD
}

void initDisplay(void)
{
   initI2C();
#ifdef LCD
    lcd_I2Cinit();
    lcd_CustomChars();
    lcd_backlight();
    lcd_clear();
#endif
#ifdef SSD1306
    ssd1306Init();
#endif
    if (!procesoLCD)
        procesoLCD = chThdCreateStatic(waLCD, sizeof(waLCD), NORMALPRIO, threadLCD, NULL);
}


/** @} */

