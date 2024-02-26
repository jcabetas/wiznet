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
#include "varsFlash.h"
#include "lcd.h"

extern event_source_t updateLCD_source;
extern event_source_t activate_source, desactivate_source;

void pruebaADC(void);
void initW25q16(void);
void initSMS(void);
void initSensores(void);
void initAdc(void);
float hallaCapBatC(void);
void initDisplay(void);
void initColas(void);
void initVL53L0X(void);
void initGestor(void);
void initGY53(void);
void abrePuertaC(void);
void cierraPuertaC(void);
void initServo(void);
void testssd1306(void);

/*
 * Ordenes SMS:
 *  - "status", envia estado de puerta, batería y telefono de envío
 *  - "telefono 619262851": pone ese teléfono como receptor de llamadas
 *  - "pin 6742"
 *  - "estado"/"status"
 *  - "activar"/"desactivar"
 *
 * Hardware:
 * - Placa Alim. EE
 * - Servo
 * - Panel LCD 1604 (comprobar funcionamiento a bajas tensiones)
 * - Medidor de distancia GY53, conectado en serial2 y Vcc a Vcc-TOF
 * - Pulsador conectado en entrada de sensor
 * - Led verde (PA9) y rojo (PA10) anodo Vcc conectados en conector "GPS" través de resistencias para limitar a 10 mA
 *
 * Consumo: 85mA con led LCD, 66 mA sin led LCD
 *
 * Conexiones:
 * - PA10: Led verde
 * - PA9: Led rojo
 * - PB0: Reset SIM800L
 * - PB1: Vbat (redundante, se puede ver con SIM800L)
 * ====
 * - PB6: I2C1SCL Para LCD y VL053
 * - PB7: I2C1SDA idem
 * - PB8: salida servo
 *
 * Procedimiento
 * - Thread GSM:
 *   * Estados: "No listo", "listo"
 *   * Inicializa GSM
 *   * Queda a la escucha de SMS, lo decodifica y responde a las ordenes
 *   * También espera ordenes de transmision de estados por SMS
 * - Thread VL053:
 *   * Estados: "No inicializado", "Esperando boton", "Espero estabilidad", "Esperando gato", "Detecto gato"
 *   * Inicializa VL053
 *   * Cuando se apriete el boton 2s y reciba su evento:
 *     - Mido alturas, que tienen que ser estables por 5 segundos
 *     - Si son estables, lanza evento "VL53listo" y se mete en bucle monitorización. Si no, lanza evento "VL53inestable"
 *   * Si recibe evento "desactiva" y no esta en "No inicializado" se pone en "Esperando boton"
 *   * En modo "Esperando gato", si detecta disminucion de 5 cm de altura durante 2s, lanza evento "DetectadoGato"
 * - Thread main
 *   * Inicializa otros threads
 *   * Espera eventos y los organiza
 * - Thread LCD
 *   * Manejo por colas
 *
 *   DISPLAY SSD (11x3)
 *     12345678901
 *     B 95% G 30%
 *     150mm
 *     Pulsa!!
 *
 */

enum estado_t {esperoThreads=1, esperoBoton, esperoEstabilidad, esperoGato, gatoEnJaula};
extern enum estado_t estado;
uint8_t okSMS(void);

extern uint16_t porcBateriaSIM800L, mVSIM800L;



void tickLed(uint8_t numPuls, uint16_t msEntrePuls, stm32_gpio_t *GPIO, uint32_t PAD)
{
    for (uint8_t numP=0;numP<numPuls;numP++)
    {
        palClearPad(GPIO, PAD);         // enciende
        palClearPad(GPIOC, GPIOC_LED);         // enciende
        chThdSleepMilliseconds(100);    // mantiene 100 ms
        palSetPad(GPIO, PAD);           // apagado
        palSetPad(GPIOC, GPIOC_LED);         // enciende
        if (numP<numPuls-1)             // si no es el ultimo, deja apagado 200 ms
            chThdSleepMilliseconds(200);
    }
    chThdSleepMilliseconds(msEntrePuls);
}


/*
 * Gestor led estados
 */
static THD_WORKING_AREA(waThreadLedEstados, 128);
static THD_FUNCTION(ThreadLedEstados, arg) {
  (void)arg;
  // numPulsos, interv. Por ejemplo 2,700 seria 100on,200off,100on,700off
  //enum estadoMain_t {esperoThreads=1, esperoBoton, esperoEstabilidad, esperoGato, gatoEnJaula};
  //                    1,100             2,100            1,100       1,2900        3,500
  uint16_t numPulsos[] =     {  3,  2,  1,  1,  3};
  uint16_t msEntrePulsos[] = {100,100,100,2900,500};
  chRegSetThreadName("blinker");
  while (true) {
      uint8_t estJaula = estado-1;
      if (estJaula>=sizeof(numPulsos))
          estJaula = 0;
      uint16_t numPuls = numPulsos[estJaula];
      uint16_t msEntrePuls = msEntrePulsos[estJaula];
      tickLed(numPuls, msEntrePuls, GPIOC,GPIOC_LED);
  }
}



/*
 * Gestor led alarma
 */
//static THD_WORKING_AREA(waThreadAlarma, 128);
//static THD_FUNCTION(ThreadAlarma, arg) {
//  (void)arg;
//  // pulsos {noOkSIM, batBaja, atasco};
//  float capBat;
//  uint8_t alarmaBat;
//  chRegSetThreadName("alarma");
//  capBat = hallaCapBatC();
//  if (capBat<20.0)
//      alarmaBat = 1;
//  else
//      alarmaBat = 0;
//  while (true) {
////      uint8_t estJaula = estado;
////      if (!okSMS())
////          tickLed(1, 1000,GPIOB,GPIOB_LEDROJO);
////      if (alarmaBat==1)
////          tickLed(2, 1000,GPIOB,GPIOB_LEDROJO);
////      if (estJaula==5)
////          tickLed(3, 1000,GPIOB,GPIOB_LEDROJO);
//      chThdSleepMilliseconds(3000);
//  }
//}




//static const I2CConfig i2ccfg = {
//  OPMODE_I2C,
//  100000,
//  STD_DUTY_CYCLE, //FAST_DUTY_CYCLE_2
//};
static const I2CConfig i2ccfg = {
  OPMODE_I2C,
  400000,
  FAST_DUTY_CYCLE_2,
};
void initI2C(void)
{
    palSetLineMode(LINE_I2C2_SDA,PAL_MODE_ALTERNATE(9) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    palSetLineMode(LINE_I2C2_SCL,PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    palSetLineMode(LINE_ONGPS, PAL_MODE_OUTPUT_PUSHPULL);
    palClearLine(LINE_ONGPS);
    chThdSleepMilliseconds(100);
    palSetLine(LINE_ONGPS);
    chThdSleepMilliseconds(50); // espera a que se inicie LCD
    i2cStart(&LCD_I2C, &i2ccfg); // LCD
}


/*
 * Application entry point.
 */
event_source_t cambioEnVL53_source, cambioBotonSource;

int main(void) {
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  // 13mA CPU, 21 mA con LCD

  halInit();
  chSysInit();

  chEvtObjectInit(&updateLCD_source);
  chEvtObjectInit(&activate_source);
  chEvtObjectInit(&desactivate_source);
  chEvtObjectInit(&cambioEnVL53_source);
  chEvtObjectInit(&cambioBotonSource);

  initColas();
  initI2C();
  initDisplay();
  initVL53L0X();
  abrePuertaC();
  initAdc();
  initW25q16();
  initSMS();
  initGestor();

  chThdCreateStatic(waThreadLedEstados, sizeof(waThreadLedEstados), NORMALPRIO, ThreadLedEstados, NULL);
//  chThdCreateStatic(waThreadAlarma, sizeof(waThreadAlarma), NORMALPRIO, ThreadAlarma, NULL);

  while (true) {
      chThdSleepMilliseconds(500);
  }
}
