
#include "ch.hpp"
#include "hal.h"

#include "VL53L0X.h"

extern "C" {
  void initVL53L0X(void);
}

#include "string.h"
#include "chprintf.h"
#include "lcd.h"
#include "colas.h"
#include "jaula.h"

void cierraPuerta(void);

#define NUMMEDS 16

//extern sms *smsModem;
extern event_source_t enviarSMS_source;

char nomEstadoVL53[7][20] = {"Iniciando","Espero activar","Mido altura","Espero gato","Hay gato"};
enum estadoVL53_t {vl53iniciando=1, vl53esperoActivacion, vl53midoAltura, vl53esperoGato, vl53hayGato};
enum estadoVL53_t estadoVL53;

extern event_source_t activate_source, desactivate_source;
extern event_source_t cambioEnVL53_source;

#ifndef hayGY53
VL53L0X sensor;
#else
extern uint16_t distancia;
extern uint8_t gy53vive;
#endif

#ifdef LCD
#define FILADIST    2
#else
#define FILADIST    1
#endif

thread_t *procesoVL53L0X = NULL;
uint8_t VL53ok;
uint16_t mmLog[NUMMEDS], media = 0;
uint8_t posLog = 0, numMed = 0, numMedConGato = 0;


// retorno 1 si hay estabilidad
uint8_t medidaBuscandoEstabilidad(uint16_t nuevaMed)
{
    char buffer[25];
    uint16_t medMin, medMax;
    uint32_t suma = 0;
    if (nuevaMed>500)
    {
        VL53ok = 0;
        media = 0;
        numMed = 0;
        chsnprintf(buffer,sizeof(buffer),"Medida rara");
        ponEnColaLCD(FILADIST,buffer);
        return 0;
    }
    mmLog[posLog] = nuevaMed;
    if (++posLog >= NUMMEDS)
        posLog = 0;
    if (numMed<NUMMEDS)
    {
        numMed++;
        return 0;
    }
    // actualizo media y verifico estabilidad
    medMin = 1000;
    medMax = 0;
    media = 0;
    for (uint8_t i=0;i<NUMMEDS;i++)
    {
        suma += mmLog[i];
        if (mmLog[i] < medMin)
            medMin = mmLog[i];
        if (mmLog[i] > medMax)
            medMax = mmLog[i];
    }
    media = suma>>4;
    if (media>100 && media<600 && (medMax-medMin)<20)
        return 1;
    else
        return 0;
}


// llamar cuando espero gato, cada 250ms
// devuelve 1 si hay gato
uint8_t registraMedida(uint16_t nuevaMed)
{
    if (nuevaMed < media - 50)
    {
        if (numMedConGato<8)
        {
            if (++numMedConGato==8)
            {
                // hay gato!!
                return 1;
            }
        }
        return 0; // no actualizo medias cuando hay gato
    }
    else
    {
        numMedConGato = 0;
        return 0;
    }
}


static THD_WORKING_AREA(waVL53, 2024);

static THD_FUNCTION(threadVL53, arg) {
    (void) arg;
    event_listener_t activateListener, desactivateListener;
    eventmask_t evt;
    char buffer[25];
    chRegSetThreadName("VL53");
    chEvtRegisterMask(&activate_source, &activateListener, EVENT_MASK(0));
    chEvtRegisterMask(&desactivate_source, &desactivateListener, EVENT_MASK(1));
    while (true)
    {
        evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(250));
        if (chThdShouldTerminateX())
        {
            chThdExit((msg_t) 1);
        }
        if (evt & EVENT_MASK(0)) // activar
        {
            if (estadoVL53 == vl53esperoActivacion)
            {
                estadoVL53 = vl53midoAltura;
                chEvtBroadcast(&cambioEnVL53_source);
            }
            continue;
        }
        if (evt & EVENT_MASK(1)) // desactivar
        {
            if (estadoVL53 != vl53iniciando)
            {
                estadoVL53 = vl53esperoActivacion;
                chEvtBroadcast(&cambioEnVL53_source);
            }
            continue;
        }
        if (evt == 0)  // timeout
        {
#ifndef hayGY53
            uint16_t mmDist = sensor.readRangeSingleMillimeters();
            if (sensor.timeoutOccurred())
            {
                media = 0;
                numMed = 0;
                ponEnColaLCD(FILADIST,"Timeout VL053");
                estadoVL53 = vl53iniciando;
                chEvtBroadcast(&cambioEnVL53_source);
                if (!sensor.init())
                    chThdSleepMilliseconds(2000);
                else
                {
                    ponEnColaLCD(FILADIST,"Rearrancado VL053");
                    estadoVL53 = vl53esperoActivacion;
                }
                continue;
            }
#else
            uint16_t mmDist = distancia;
#endif
            switch (estadoVL53)
            {
                case vl53iniciando:
                    break;
                case vl53esperoActivacion:
                    chsnprintf(buffer,sizeof(buffer),"dist:%3dmm ",mmDist);
                    ponEnColaLCD(FILADIST,buffer);
                    break;
                case vl53midoAltura:
                {
                    uint8_t finMedida = medidaBuscandoEstabilidad(mmDist);
                    if (finMedida)
                    {
                        estadoVL53 = vl53esperoGato;
                        chEvtBroadcast(&cambioEnVL53_source);
                    }
                    chsnprintf(buffer,sizeof(buffer),"dist:%3dmm ",mmDist);
                    ponEnColaLCD(FILADIST,buffer);
                    break;
                }
                case vl53esperoGato:
                {
                    uint8_t hayGato = registraMedida(mmDist);
                    if (hayGato)
                    {
                        estadoVL53 = vl53hayGato;
                        chEvtBroadcast(&cambioEnVL53_source);
                        //ponEnColaLCD(2,"HAY GATO!");
                        continue;
                    }
                    chsnprintf(buffer,sizeof(buffer),"dist:%3dmm ",mmDist);
                    ponEnColaLCD(FILADIST,buffer);
                    break;
                }
                case vl53hayGato:
                    break;
            }
        }
    }
}

void initVL53L0X(void)
{
#ifndef hayGY53
    VL53ok = 0;
    estadoVL53 = vl53iniciando;
    ponEnColaLCD(FILADIST,"Inicializando VL53");
    chThdSleepMilliseconds(100);
//    // PC14 es SHUTDOWN
//    palSetLine(LINE_SHUTVLX);
//    palSetLineMode(LINE_SHUTVLX, PAL_MODE_OUTPUT_PUSHPULL);
//    // reseteamos
//    palClearLine(LINE_SHUTVLX);
//    chThdSleepMilliseconds(5);
//    palSetLine(LINE_SHUTVLX);
    chThdSleepMilliseconds(50);
    sensor.setTimeout(500);
    if (!sensor.init())
    {
        ponEnColaLCD(FILADIST,"Fallo VL053");
        chThdSleepMilliseconds(5000);
        return;
    }
#if defined LONG_RANGE
  // lower the return signal rate limit (default is 0.25 MCPS)
  sensor.setSignalRateLimit(0.1);
  // increase laser pulse periods (defaults are 14 and 10 PCLKs)
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
#endif

#if defined HIGH_SPEED
  // reduce timing budget to 20 ms (default is about 33 ms)
  sensor.setMeasurementTimingBudget(20000);
#elif defined HIGH_ACCURACY
  // increase timing budget to 200 ms
  sensor.setMeasurementTimingBudget(200000);
#endif
#else
    initGY53();
#endif
  estadoVL53 = vl53esperoActivacion;
  if (!procesoVL53L0X)
        procesoVL53L0X = chThdCreateStatic(waVL53, sizeof(waVL53), NORMALPRIO, threadVL53, NULL);
}


